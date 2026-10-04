#pragma once

#include "ChainTestSupport.h"
#include "DeviceIdentity.h"
#include "DocumentRecords.h"
#include "DocumentSession.h"
#include "FakeServer.h"
#include "RosterChangeQueue.h"
#include "ServerConnection.h"
#include "SessionTimers.h"

#include <QHash>
#include <QString>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <vector>

/// Running devices of one ward against one FakeServer, for the session and queue
/// suites.
///
/// Everything is in memory or in a temporary folder, every timer fires only when
/// a test fires it, and every reply arrives only when a test lets it.
namespace SessionTestSupport
{

class MemoryRecords : public DocumentRecords
{
public:
    DocumentRecord record(const QString& documentId) const override;
    void setRecord(const QString& documentId, const DocumentRecord& record) override;
    void removeRecord(const QString& documentId) override;
    QStringList documentIds() const override;

    bool contains(const QString& documentId) const { return m_records.contains(documentId); }

private:
    QHash<QString, DocumentRecord> m_records;
};

/// Records each running timer and its interval, and fires one on fire().
class ManualTimers : public SessionTimers
{
public:
    void start(Timer timer, int milliseconds) override;
    void stop(Timer timer) override;

    bool isRunning(Timer timer) const { return m_running.contains(int(timer)); }

    /// The interval the timer was last started with; -1 when it is not running.
    int interval(Timer timer) const { return m_running.value(int(timer), -1); }

    /// Fires a running timer, as its timeout would. False when it is not running.
    bool fire(Timer timer);

private:
    QHash<int, int> m_running;
};

/// A new person with one device.
DeviceIdentity newIdentity(const QString& displayName, const QString& deviceName);

/// Another device of the same person.
DeviceIdentity newDeviceOf(const DeviceIdentity& person, const QString& deviceName);

ChainTestSupport::TestDevice asTestDevice(const DeviceIdentity& identity);

/// One installation: its identity, its records, its timers, its connection, and its
/// own folder.
class Device
{
public:
    Device(FakeServer& server, const DeviceIdentity& identity);

    DeviceIdentity identity;
    MemoryRecords records;
    ServerConnection connection;
    QTemporaryDir folder;

    /// The timers of the session or queue made last, which own them. Valid while
    /// that session or queue lives.
    ManualTimers* timers = nullptr;
    ManualTimers* queueTimers = nullptr;

    DocumentSession::Context context();
    DocumentPaths paths(const QString& documentId) const;

    std::unique_ptr<DocumentSession> create(const QString& documentId);
    std::unique_ptr<DocumentSession> open(const QString& documentId);
    std::unique_ptr<DocumentSession> open(const QString& documentId, LoadFailure& failure);
    std::unique_ptr<RosterChangeQueue> queue();
};

/// A ward's devices over one FakeServer.
class TestWard
{
public:
    FakeServer server;

    Device& add(const DeviceIdentity& identity);

    /// Lets every reply arrive, held or not, until nothing is waiting.
    void settle();

    /// Creates the ward on owner with two families, Adams and Baker, and a storm
    /// response recording both, then shares it. documentId is set.
    std::unique_ptr<DocumentSession> createShared(Device& owner);

    /// Asks for access as newcomer, has admin grant it through the queue, and
    /// downloads the ward to newcomer.
    bool grant(Device& admin, Device& newcomer);

    /// Queues one change to the ward on device and lets it publish; the outcome it
    /// finished with.
    std::optional<RosterChangeOutcome> change(Device& device, const RosterChange& change);

    /// Pairs newDevice with existing, which must already be a member somewhere.
    bool pair(Device& newDevice, Device& existing);

    /// The server's current version of the ward, as a device of it downloads it.
    std::optional<ServerVersion::Fetched> serverCopy(Device& reader);

    QString documentId;
    FamilyId adams;
    FamilyId baker;

private:
    std::vector<std::unique_ptr<Device>> m_devices;
};

// --- Edits, as the app makes them ---

/// A directory edit: moves a family.
std::function<void(Document&)> moveFamily(const FamilyId& family, double latitude);

/// A directory edit: adds a family.
std::function<void(Document&)> addFamily(const FamilyId& family, double latitude);

/// A response edit: records a contact.
std::function<void(Document&)> recordContact(const FamilyId& family, ContactStatus status);

std::optional<double> latitudeOf(const Document& document, const FamilyId& family);
ContactStatus statusOf(const Document& document, const FamilyId& family);

/// The header of the file on disk.
std::optional<DocumentHeader> headerOnDisk(const QString& path);

} // namespace SessionTestSupport
