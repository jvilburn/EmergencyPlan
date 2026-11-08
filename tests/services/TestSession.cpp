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
