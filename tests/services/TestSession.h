#pragma once

#include "FakeGeocoder.h"
#include "FakeUnitLookup.h"
#include "FakeWardStore.h"
#include "Identity.h"
#include "WardSession.h"

/// The identity and fakes a TestSession runs over. TestSession derives from this before
/// WardSession so they are built before the session that uses them.
class TestSessionFakes
{
protected:
    TestSessionFakes();

    Identity m_deviceIdentity;
    FakeWardStore m_fakeStore;
    FakeGeocoder m_fakeGeocoder;
    FakeUnitLookup m_fakeUnitLookup;
};

/// A WardSession under its own identity, over a FakeWardStore, a FakeGeocoder and a
/// FakeUnitLookup, which the test drives.
class TestSession : private TestSessionFakes, public WardSession
{
public:
    TestSession();

    FakeGeocoder& geocoder() { return m_fakeGeocoder; }
    FakeUnitLookup& unitLookup() { return m_fakeUnitLookup; }
};
