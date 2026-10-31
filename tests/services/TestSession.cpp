#include "TestSession.h"

TestSessionFakes::TestSessionFakes()
    : m_fakeGeocoder(nullptr)
    , m_fakeUnitLookup(nullptr)
{
}

TestSession::TestSession()
    : TestSessionFakes()
    , WardSession(&m_fakeGeocoder, &m_fakeUnitLookup, nullptr)
{
}
