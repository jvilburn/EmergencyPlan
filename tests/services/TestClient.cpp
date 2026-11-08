#include "TestClient.h"

TestClient::TestClient(const QString& deviceName)
    : m_deviceName(deviceName)
    , m_identity(Identity::generate())
    , m_geocoder(nullptr)
    , m_unitLookup(nullptr)
    , m_session(std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr))
{
}

void TestClient::createWard(const Document& content)
{
    m_session->createWard(content, "Alice Smith", m_deviceName);
}

OpenResult TestClient::reopen()
{
    DocumentId document = m_session->documentId();
    m_session = std::make_unique<WardSession>(&m_identity, &m_store, &m_geocoder, &m_unitLookup, nullptr);
    return m_session->openWard(document);
}
