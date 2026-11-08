#pragma once

#include "FakeGeocoder.h"
#include "FakeUnitLookup.h"
#include "FakeWardStore.h"
#include "Identity.h"
#include "WardSession.h"

#include <QString>
#include <memory>

/// A full client for tests: one device, with its own identity, ward store, fakes and session,
/// and nothing shared with any other client.
class TestClient
{
public:
    /// A device named deviceName, with no ward open.
    explicit TestClient(const QString& deviceName);

    WardSession& session() { return *m_session; }
    const Identity& identity() const { return m_identity; }
    const QString& deviceName() const { return m_deviceName; }
    FakeWardStore& store() { return m_store; }
    FakeGeocoder& geocoder() { return m_geocoder; }
    FakeUnitLookup& unitLookup() { return m_unitLookup; }

    /// Creates a ward holding content under the user Alice Smith, on this device.
    void createWard(const Document& content);

    /// Gives this device the ward other has open as it stands, access list included: a ward
    /// created here takes other's document as a version, so the two hold one ward. Its local
    /// file is sealed to that access list, which names other's device and not this one.
    void takeWardOf(TestClient& other);

    /// Closes the app and starts it again: a new session on the same identity and store,
    /// opening the ward the old session had open.
    OpenResult reopen();

private:
    QString m_deviceName;
    Identity m_identity;
    FakeWardStore m_store;
    FakeGeocoder m_geocoder;
    FakeUnitLookup m_unitLookup;
    std::unique_ptr<WardSession> m_session;
};
