#pragma once

#include "FakeGeocoder.h"
#include "FakeUnitLookup.h"
#include "WardSession.h"

/// The fakes a TestSession runs over. TestSession derives from this before WardSession so
/// the fakes are built before the session that connects to them.
class TestSessionFakes
{
protected:
    TestSessionFakes();

    FakeGeocoder m_fakeGeocoder;
    FakeUnitLookup m_fakeUnitLookup;
};

/// A WardSession over a FakeGeocoder and a FakeUnitLookup, which the test drives.
class TestSession : private TestSessionFakes, public WardSession
{
public:
    TestSession();

    FakeGeocoder& geocoder() { return m_fakeGeocoder; }
    FakeUnitLookup& unitLookup() { return m_fakeUnitLookup; }
};
