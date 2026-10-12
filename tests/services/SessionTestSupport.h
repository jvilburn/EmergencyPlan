#pragma once

#include "DeviceIdentity.h"
#include "DocumentRecords.h"
#include "DocumentSession.h"
#include "FakeServer.h"
#include "ServerConnection.h"
#include "ServerTestSupport.h"
#include "SessionTimers.h"

#include <QHash>
#include <QString>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <vector>

/// Running devices of one ward against one FakeServer, for the session suites.
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

ServerTestSupport::TestDevice asTestDevice(const DeviceIdentity& identity);

/// A Joined that keeps the outcome in out.
void keepJoin(std::optional<DocumentSession::Join>& out,
              DocumentSession::Join join,
              const QString& reason,
              const std::optional<Document>& document);

/// A roster of these devices as invites would build it: the first its maker, an
/// admin; each later one joined as a new member through an invite of the maker's.
SealingSet rosterOf(const QList<DeviceIdentity>& devices);

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

    /// The timers of the session made last, which owns them. Valid while that
    /// session lives.
    ManualTimers* timers = nullptr;

    DocumentSession::Context context();
    DocumentPaths paths(const QString& documentId) const;

    std::unique_ptr<DocumentSession> create(const QString& documentId);
    std::unique_ptr<DocumentSession> open(const QString& documentId);
    std::unique_ptr<DocumentSession> open(const QString& documentId, LoadFailure& failure);
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

    /// An invite made on inviter's open session, as DocumentManager makes one, its
    /// version published: the link, or nullopt when the roster refused it or the
    /// publish did not land.
    std::optional<InviteLink> invite(DocumentSession& inviter,
                                                     Invite::For purpose,
                                                     Role role,
                                                     const QString& label);

    /// Joins link on joiner, as opening the link does there.
    std::optional<DocumentSession::Join> join(Device& joiner, const InviteLink& link);

    /// A User invite from inviter, joined by newcomer as a new person.
    bool addUser(DocumentSession& inviter, Device& newcomer, Role role);

    /// A Device invite from owner, joined by newDevice as another of owner's devices.
    bool addDevice(DocumentSession& owner, Device& newDevice);

    /// The server's head of the ward, read as anyone may.
    std::optional<ServerContract::Head> head();

    /// The server's current version of the ward, or of wardId, as a device of
    /// it downloads it.
    std::optional<ServerVersion::Fetched> serverCopy(Device& reader);
    std::optional<ServerVersion::Fetched> serverCopy(Device& reader, const QString& wardId);

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

/// Roster edits, as DocumentManager makes them through Edit::Roster; one the roster
/// refuses ends the run.
std::function<void(Document&)> removePerson(const MemberId& person);
std::function<void(Document&)> removeDevice(const Fingerprint& device);
std::function<void(Document&)> removeInvite(const Fingerprint& invite);
std::function<void(Document&)> setRole(const MemberId& person, Role role);

std::optional<double> latitudeOf(const Document& document, const FamilyId& family);
ContactStatus statusOf(const Document& document, const FamilyId& family);

/// The header of the file on disk.
std::optional<DocumentHeader> headerOnDisk(const QString& path);

} // namespace SessionTestSupport
