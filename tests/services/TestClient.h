#pragma once

#include "FakeGeocoder.h"
#include "FakeUnitLookup.h"
#include "FakeWardStore.h"
#include "Identity.h"
#include "Server.h"
#include "WardSession.h"
#include "WardSync.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <memory>

class FakeServer;

/// A full client for tests: one device, with its own identity, ward store, fakes and session,
/// and nothing shared with any other client but the server it is given.
class TestClient
{
public:
    /// A device named deviceName, with no ward open and no server.
    explicit TestClient(const QString& deviceName);

    /// A device named deviceName, with no ward open, which shares the ward it opens through
    /// server once startSync is called.
    TestClient(const QString& deviceName, FakeServer* server);

    /// The sharing of the open ward through the server this client was given.
    WardSync& sync() { return *m_sync; }

    /// Starts sharing the open ward, and settles the server.
    void startSync();

    WardSession& session() { return *m_session; }
    const Identity& identity() const { return m_identity; }
    const QString& deviceName() const { return m_deviceName; }
    FakeWardStore& store() { return m_store; }
    FakeGeocoder& geocoder() { return m_geocoder; }
    FakeUnitLookup& unitLookup() { return m_unitLookup; }

    /// This device as an access-list entry, named deviceName.
    AccessDevice device() const;

    /// Creates a ward holding content under the user Alice Smith, on this device.
    void createWard(const Document& content);

    /// Creates a ward here and replays other's document onto it as a version, so this device
    /// holds other's document as it stands, access list included, under its own document id.
    /// Its local file is sealed to that access list, which names other's device and not this
    /// one.
    void takeWardOf(TestClient& other);

    /// Gives each of holders a local file of one ward and opens it: document under dataKey,
    /// holding content with an access list of one admin user per client of listed, named for
    /// its device and holding that device alone, in listed's order, as version 1 written by
    /// listed's first client, with no landed marks and an empty journal. Every holder is
    /// listed, so each can open its file, and every holder of one call holds an equal ward.
    static void holdWard(const QList<TestClient*>& holders, const QList<TestClient*>& listed,
                         const DocumentId& document, const QByteArray& dataKey, const Document& content);

    /// Gives each of holders a local file of one ward and opens it: document under dataKey,
    /// holding content with access as its access list, as version 1 written by writer, with
    /// no landed marks, an empty journal, and access's recipients as the held version's.
    /// Every holder is on access, so each can open its file.
    static void holdWard(const QList<TestClient*>& holders, const AccessList& access, const TestClient& writer,
                         const DocumentId& document, const QByteArray& dataKey, const Document& content);

    /// Closes the app and starts it again: a new session on the same identity and store,
    /// opening the ward the old session had open, with its sharing stopped.
    OpenResult reopen();

private:
    QString m_deviceName;
    Identity m_identity;
    FakeWardStore m_store;
    FakeGeocoder m_geocoder;
    FakeUnitLookup m_unitLookup;
    std::unique_ptr<WardSession> m_session;
    FakeServer* m_fakeServer;
    std::unique_ptr<Server> m_server;
    std::unique_ptr<WardSync> m_sync;  ///< declared after the session it works on
};
