#include "TestClient.h"
#include "WardFile.h"

TestClient::TestClient(const QString& deviceName)
    : m_deviceName(deviceName)
    , m_identity(Identity::generate())
    , m_geocoder(nullptr)
    , m_unitLookup(nullptr)
    , m_session(std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr))
{
}

AccessDevice TestClient::device() const
{
    return AccessDevice(m_deviceName, m_identity.publicKey(), m_identity.verifyKey());
}

void TestClient::createWard(const Document& content)
{
    m_session->createWard(content, "Alice Smith", m_deviceName);
}

void TestClient::takeWardOf(TestClient& other)
{
    createWard(other.session().document());
    m_session->replayOnto(other.session().document());
}

void TestClient::holdWard(const QList<TestClient*>& holders, const QList<TestClient*>& listed,
                          const DocumentId& document, const QByteArray& dataKey, const Document& content)
{
    QList<AccessUser> users;
    for (const TestClient* client : listed)
    {
        users.append(AccessUser(UserId::generate(), client->deviceName(), AccessRole::Admin, {client->device()}));
    }
    holdWard(holders, AccessList(users, {}, {}), *listed.first(), document, dataKey, content);
}

void TestClient::holdWard(const QList<TestClient*>& holders, const AccessList& access, const TestClient& writer,
                          const DocumentId& document, const QByteArray& dataKey, const Document& content)
{
    Document ward = content;
    ward.setAccess(access);
    DeviceState device;
    device.heldRecipients = access.recipientFingerprints();
    VersionRank first{1, writer.identity().fingerprint()};
    QByteArray file = WardFile::write(document, first, dataKey, WardPayload{ward, {}, device});
    for (TestClient* holder : holders)
    {
        holder->store().put(document, file);
        if (holder->session().openWard(document).status != OpenStatus::Opened)
        {
            qFatal("TestClient: the ward %s did not open on %s", qPrintable(document.toString()),
                   qPrintable(holder->deviceName()));
        }
    }
}

OpenResult TestClient::reopen()
{
    DocumentId document = m_session->documentId();
    m_session = std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr);
    return m_session->openWard(document);
}
