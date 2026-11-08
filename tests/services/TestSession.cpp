#include "TestSession.h"

TestSessionFakes::TestSessionFakes()
    : m_deviceIdentity(Identity::generate())
    , m_fakeGeocoder(nullptr)
    , m_fakeUnitLookup(nullptr)
{
}

TestSession::TestSession()
    : TestSessionFakes()
    , WardSession(&m_deviceIdentity, &m_fakeStore, &m_fakeGeocoder, &m_fakeUnitLookup, nullptr)
{
}

void TestSession::createWard(const Document& content)
{
    WardSession::createWard(content, "Test User", "Test Device");
}
