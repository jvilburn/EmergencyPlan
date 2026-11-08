#include "TestClient.h"

TestClient::TestClient(const QString& deviceName)
    : m_deviceName(deviceName)
    , m_identity(Identity::generate())
    , m_geocoder(nullptr)
    , m_unitLookup(nullptr)
    , m_session(std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr))
{
}

void TestClient::createWard(const Document& document)
{
    Document ward = document;
    ward.setAccess(AccessList::founding(UserId::generate(), "Alice Smith", m_deviceName, m_identity));
    m_session->createWard(ward);
}

OpenResult TestClient::reopen()
{
    DocumentId document = m_session->documentId();
    m_session = std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr);
    return m_session->openWard(document);
}
