#include "DeviceLinks.h"

#include "DocumentCrypto.h"

using namespace ServerContract;
using namespace std::placeholders;

DeviceLinks::DeviceLinks(DocumentRecords& records, ServerConnection& server, SealingQueue& queue, QObject* parent)
    : QObject(parent)
    , m_records(records)
    , m_server(server)
    , m_queue(queue)
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
    m_server.approveLink(code,
                         this,
                         std::bind(&DeviceLinks::onApproved, this, device, m_names.value(code), _1));
}

void DeviceLinks::onApproved(const DeviceKeys& device, const QString& deviceName, const Result<Done>& result)
{
    if (!result.ok())
    {
        emit failed(result.error);
        return;
    }

    QList<DeviceKeys> linked = m_records.linkedDevices();
    if (!linked.contains(device))
    {
        linked.append(device);
        m_records.setLinkedDevices(linked);
    }
    m_queue.sealEverywhere();
    emit approved(deviceName);
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
