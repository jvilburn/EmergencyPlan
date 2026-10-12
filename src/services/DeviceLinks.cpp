#include "DeviceLinks.h"

#include "DocumentCrypto.h"

using namespace ServerContract;
using namespace std::placeholders;

DeviceLinks::DeviceLinks(const DeviceIdentity& identity,
                         DocumentRecords& records,
                         ServerConnection& server,
                         std::function<DocumentPaths(const QString&)> pathsFor,
                         QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(records)
    , m_server(server)
    , m_pathsFor(std::move(pathsFor))
{
}

void DeviceLinks::read(const QString& code)
{
    m_server.link(code.trimmed().toUpper(), this, std::bind(&DeviceLinks::onRead, this, code.trimmed().toUpper(), _1));
}

void DeviceLinks::onRead(const QString& code, const Result<LinkState>& state)
{
    if (!state.ok())
    {
        emit failed(state.error);
        return;
    }
    m_names.insert(code, state.value->deviceName);
    emit linkRead(code, state.value.value());
}

void DeviceLinks::approve(const QString& code, const DeviceKeys& device)
{
    if (m_approval.has_value())
    {
        return;
    }
    startApproval(code, device, true);
}

void DeviceLinks::startApproval(const QString& code, const DeviceKeys& device, bool mayRetry)
{
    m_approval = Approval{code, device, mayRetry, 0, {}};
    for (const QString& documentId : m_records.documentIds())
    {
        if (!m_records.record(documentId).baseVersion.has_value())
        {
            continue;
        }
        ++m_approval->building;
        ServerVersion::buildResealing(
            m_identity.self(),
            ServerVersion::trustedKeys(documentId, m_pathsFor(documentId), m_records, m_identity.self()),
            m_records.linkedDevices(),
            device,
            m_server,
            documentId,
            this,
            std::bind(&DeviceLinks::onBuilt, this, _1, _2));
    }
    if (m_approval->building == 0)
    {
        sendApproval();
    }
}

void DeviceLinks::onBuilt(const ServerVersion::Fetched& theirs, const std::optional<Resealing>& built)
{
    // A ward whose version this device cannot read is left out; this device seals it
    // on its next write of that ward, and the new device waits for access until then.
    // A ward whose call got no answer is another matter: the approval is refused
    // here, so the person approves again once the connection is back, and the new
    // device is not left short by a dropped connection.
    if (built.has_value())
    {
        m_approval->built.append(built.value());
    }
    else if (theirs.outcome == ServerVersion::Fetched::Outcome::CallFailed
             && m_approval->unanswered == ServerError::None)
    {
        m_approval->unanswered = theirs.error;
    }

    if (--m_approval->building > 0)
    {
        return;
    }
    if (m_approval->unanswered != ServerError::None)
    {
        const ServerError error = m_approval->unanswered;
        m_approval.reset();
        emit failed(error);
        return;
    }
    sendApproval();
}

void DeviceLinks::sendApproval()
{
    m_server.approveLink(m_approval->code, m_approval->built, this, std::bind(&DeviceLinks::onApproved, this, _1));
}

void DeviceLinks::onApproved(const Result<Done>& result)
{
    const Approval approval = m_approval.value();
    m_approval.reset();

    // Someone published to one of the wards meanwhile: once more on their versions.
    if (result.error == ServerError::VersionConflict && approval.mayRetry)
    {
        startApproval(approval.code, approval.device, false);
        return;
    }
    if (!result.ok())
    {
        emit failed(result.error);
        return;
    }

    QList<DeviceKeys> linked = m_records.linkedDevices();
    if (!linked.contains(approval.device))
    {
        linked.append(approval.device);
        m_records.setLinkedDevices(linked);
    }
    emit approved(m_names.value(approval.code));
}

void DeviceLinks::unlink(const QList<Fingerprint>& devices)
{
    m_unlinking = int(devices.size());
    m_unlinked = 0;
    for (const Fingerprint& device : devices)
    {
        m_server.unlinkDevice(device, this, std::bind(&DeviceLinks::onUnlinked, this, device, _1));
    }
}

void DeviceLinks::onUnlinked(const Fingerprint& device, const Result<Done>& result)
{
    --m_unlinking;

    // NotFound: already gone from this person, which is what was asked.
    if (result.ok() || result.error == ServerError::NotFound)
    {
        ++m_unlinked;

        // Nothing more to seal it into; it is pruned from each ward's next version.
        QList<DeviceKeys> linked = m_records.linkedDevices();
        linked.removeIf(std::bind(&DeviceLinks::isDevice, device, _1));
        m_records.setLinkedDevices(linked);
    }
    else
    {
        emit failed(result.error);
    }

    if (m_unlinking == 0 && m_unlinked > 0)
    {
        emit unlinked(m_unlinked);
    }
}

bool DeviceLinks::isDevice(const Fingerprint& fingerprint, const DeviceKeys& device)
{
    return DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey) == fingerprint;
}
