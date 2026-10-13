#include "SessionTestSupport.h"

#include "Address.h"
#include "ServerTestSupport.h"

#include <QCoreApplication>
#include <QDir>

using namespace ServerContract;
using namespace std::placeholders;

namespace
{

void applyMove(Document& document, const FamilyId& family, double latitude)
{
    std::optional<Family> existing = document.findFamilyById(family);
    if (existing.has_value())
    {
        existing.value().setLatitude(latitude);
        document.updateFamily(existing.value());
    }
}

void applyAdd(Document& document, const FamilyId& family, double latitude)
{
    document.addFamily(Family::createWithId(family, latitude, -111.0, Address(), {}));
}

void applyContact(Document& document, const FamilyId& family, ContactStatus status)
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        response = EmergencyResponse::create("Storm");
    }
    if (response.value().findRecord(family) == nullptr)
    {
        response.value().addFamilyRecord(FamilyResponseRecord::create(family, "Family", "1 Oak St"));
    }
    response.value().mutableRecord(family)->setContactStatus(status);
    document.setEmergencyResponse(response);
}

void keepFetched(std::optional<ServerVersion::Fetched>& out, const ServerVersion::Fetched& fetched)
{
    out = fetched;
}

// A refused roster change is a test's mistake, and loud.

void applyRemovePerson(SealingSet& roster, const MemberId& person)
{
    if (!roster.removePerson(person))
    {
        qFatal("removePerson refused");
    }
}

void applyRemoveDevice(SealingSet& roster, const Fingerprint& device)
{
    if (!roster.removeDevice(device))
    {
        qFatal("removeDevice refused");
    }
}

void applyRemoveInvite(SealingSet& roster, const Fingerprint& invite)
{
    if (!roster.removeInvite(invite))
    {
        qFatal("removeInvite refused");
    }
}

void applySetRole(SealingSet& roster, const MemberId& person, Role role)
{
    if (!roster.setRole(person, role))
    {
        qFatal("setRole refused");
    }
}

/// Timers for a session to own, with a pointer kept for the test to fire them.
std::unique_ptr<SessionTimers> keptTimers(SessionTestSupport::ManualTimers*& kept)
{
    std::unique_ptr<SessionTestSupport::ManualTimers> timers = std::make_unique<SessionTestSupport::ManualTimers>();
    kept = timers.get();
    return timers;
}

} // namespace

namespace SessionTestSupport
{

// --- MemoryRecords ---------------------------------------------------------------

DocumentRecord MemoryRecords::record(const QString& documentId) const
{
    return m_records.value(documentId);
}

void MemoryRecords::setRecord(const QString& documentId, const DocumentRecord& record)
{
    m_records.insert(documentId, record);
}

void MemoryRecords::removeRecord(const QString& documentId)
{
    m_records.remove(documentId);
}

QStringList MemoryRecords::documentIds() const
{
    return m_records.keys();
}

// --- ManualTimers ----------------------------------------------------------------

void ManualTimers::start(Timer timer, int milliseconds)
{
    m_running.insert(int(timer), milliseconds);
}

void ManualTimers::stop(Timer timer)
{
    m_running.remove(int(timer));
}

bool ManualTimers::fire(Timer timer)
{
    if (!m_running.contains(int(timer)))
    {
        return false;
    }

    // Single-shot, as QTimer is here: stopped before its timeout runs, so the
    // timeout may start it again.
    m_running.remove(int(timer));
    if (onTimeout)
    {
        onTimeout(timer);
    }
    return true;
}

// --- Identities ------------------------------------------------------------------

DeviceIdentity newIdentity(const QString& displayName, const QString& deviceName)
{
    DeviceIdentity identity;
    identity.sealing = DocumentCrypto::generateKeypair();
    identity.signing = DocumentCrypto::generateSigningKeypair();
    identity.member = ServerTestSupport::newMember();
    identity.displayName = displayName;
    identity.deviceName = deviceName;
    return identity;
}

ServerTestSupport::TestDevice asTestDevice(const DeviceIdentity& identity)
{
    return ServerTestSupport::TestDevice{identity.deviceName, identity.sealing, identity.signing};
}

void keepJoin(std::optional<DocumentSession::Join>& out,
              DocumentSession::Join join,
              const QString& reason,
              const std::optional<Document>& document)
{
    Q_UNUSED(reason);
    Q_UNUSED(document);
    out = join;
}

SealingSet rosterOf(const QList<DeviceIdentity>& devices)
{
    SealingSet roster = SealingSet::of(devices.first().member, devices.first().displayName, devices.first().device());
    for (qsizetype i = 1; i < devices.size(); ++i)
    {
        const DeviceIdentity& joiner = devices.at(i);
        Invite invite;
        invite.maker = devices.first().member;
        invite.label = "for " + joiner.displayName;
        invite.keys = DeviceKeys{DocumentCrypto::generateKeypair().publicKey,
                                 DocumentCrypto::generateSigningKeypair().verifyKey};
        if (!roster.addInvite(invite) || !roster.join(invite.fingerprint(), joiner.device(), joiner.member, joiner.displayName))
        {
            qFatal("rosterOf: a join refused");
        }
    }
    return roster;
}

// --- Device ----------------------------------------------------------------------

Device::Device(FakeServer& server, const DeviceIdentity& identity)
    : identity(identity)
    , connection(server, identity)
{
}

DocumentSession::Context Device::context()
{
    return DocumentSession::Context{identity, records, connection};
}

DocumentPaths Device::paths(const QString& documentId) const
{
    const QDir directory(folder.path());
    return DocumentPaths{directory.filePath(documentId + ".wardplanning"),
                         directory.filePath(documentId + ".base")};
}

std::unique_ptr<DocumentSession> Device::create(const QString& documentId)
{
    return DocumentSession::create(context(), keptTimers(timers), documentId, paths(documentId), nullptr);
}

std::unique_ptr<DocumentSession> Device::open(const QString& documentId)
{
    LoadFailure failure = LoadFailure::None;
    return open(documentId, failure);
}

std::unique_ptr<DocumentSession> Device::open(const QString& documentId, LoadFailure& failure)
{
    return DocumentSession::open(context(), keptTimers(timers), documentId, paths(documentId), failure, nullptr);
}

// --- TestWard --------------------------------------------------------------------

Device& TestWard::add(const DeviceIdentity& identity)
{
    m_devices.push_back(std::make_unique<Device>(server, identity));
    return *m_devices.back();
}

void TestWard::settle()
{
    // Quiet twice running, since a reply may start a call whose reply is queued
    // for the next turn.
    int quietRounds = 0;
    while (quietRounds < 2)
    {
        QCoreApplication::processEvents();
        bool delivered = false;
        while (server.deliverNext())
        {
            delivered = true;
        }
        quietRounds = delivered ? 0 : quietRounds + 1;
    }
}

std::unique_ptr<DocumentSession> TestWard::createShared(Device& owner)
{
    documentId = ServerTestSupport::newDocumentId();
    adams = FamilyId::generate();
    baker = FamilyId::generate();

    std::unique_ptr<DocumentSession> session = owner.create(documentId);
    if (!session)
    {
        return nullptr;
    }

    session->edit(DocumentSession::Edit::Directory, addFamily(adams, 40.0));
    session->edit(DocumentSession::Edit::Directory, addFamily(baker, 41.0));
    session->edit(DocumentSession::Edit::Response, recordContact(adams, ContactStatus::NotContacted));
    session->edit(DocumentSession::Edit::Response, recordContact(baker, ContactStatus::NotContacted));
    if (!session->flush(nullptr))
    {
        return nullptr;
    }

    session->share();
    settle();
    if (!session->isShared())
    {
        return nullptr;
    }
    return session;
}

std::optional<InviteLink> TestWard::invite(DocumentSession& inviter, Invite::For purpose, Role role, const QString& label)
{
    // As DocumentManager::invite makes one.
    const std::optional<Holder> me = inviter.self();
    if (!inviter.isShared() || !me.has_value())
    {
        return std::nullopt;
    }
    const InviteLink link{inviter.documentId(), DocumentCrypto::randomBytes(InviteLink::SeedSize)};
    const Invite made = link.invite(purpose, me->id, role, label);
    SealingSet changed = inviter.document().sealingSet();
    if (!changed.addInvite(made))
    {
        return std::nullopt;
    }
    inviter.setRoster(changed);
    inviter.publishNow();
    settle();

    // Published: the server's current header seals to the new invite.
    const std::optional<Head> current = head();
    if (inviter.syncState() != SyncState::UpToDate || !current.has_value()
        || !ServerTestSupport::recipientsOf(current.value()).contains(made.fingerprint().toHex()))
    {
        return std::nullopt;
    }
    return link;
}

std::optional<DocumentSession::Join> TestWard::join(Device& joiner, const InviteLink& link)
{
    QObject receiver;
    std::optional<DocumentSession::Join> joined;
    DocumentSession::join(joiner.context(),
                          link,
                          joiner.paths(link.documentId),
                          &receiver,
                          std::bind(&keepJoin, std::ref(joined), _1, _2, _3));
    settle();
    return joined;
}

bool TestWard::addUser(DocumentSession& inviter, Device& newcomer, Role role)
{
    const std::optional<InviteLink> link =
        invite(inviter, Invite::For::User, role, "for " + newcomer.identity.displayName);
    return link.has_value() && join(newcomer, link.value()) == DocumentSession::Join::Done;
}

bool TestWard::addDevice(DocumentSession& owner, Device& newDevice)
{
    const std::optional<InviteLink> link =
        invite(owner, Invite::For::Device, Role::Member, "for " + newDevice.identity.deviceName);
    return link.has_value() && join(newDevice, link.value()) == DocumentSession::Join::Done;
}

std::optional<Head> TestWard::head()
{
    const Result<Head> answered = ServerTestSupport::headOf(server, documentId);
    if (!answered.ok())
    {
        return std::nullopt;
    }
    return answered.value;
}

QStringList TestWard::recipientsOnServer()
{
    const std::optional<Head> current = head();
    return current.has_value() ? ServerTestSupport::recipientsOf(current.value()) : QStringList();
}

std::optional<ServerVersion::Fetched> TestWard::serverCopy(Device& reader)
{
    // Fetched against no trusted key, so reading the server's copy changes nothing
    // the device keeps and takes the version as it stands.
    QObject receiver;
    std::optional<ServerVersion::Fetched> fetched;
    ServerVersion::fetch(reader.identity.self(),
                         {},
                         reader.connection,
                         documentId,
                         &receiver,
                         std::bind(&keepFetched, std::ref(fetched), _1));
    settle();
    if (!fetched.has_value() || fetched.value().outcome != ServerVersion::Fetched::Outcome::Accepted)
    {
        return std::nullopt;
    }
    return fetched;
}

// --- Edits -----------------------------------------------------------------------

std::function<void(Document&)> moveFamily(const FamilyId& family, double latitude)
{
    return std::bind(&applyMove, _1, family, latitude);
}

std::function<void(Document&)> addFamily(const FamilyId& family, double latitude)
{
    return std::bind(&applyAdd, _1, family, latitude);
}

std::function<void(Document&)> recordContact(const FamilyId& family, ContactStatus status)
{
    return std::bind(&applyContact, _1, family, status);
}

std::function<void(SealingSet&)> removePerson(const MemberId& person)
{
    return std::bind(&applyRemovePerson, _1, person);
}

std::function<void(SealingSet&)> removeDevice(const Fingerprint& device)
{
    return std::bind(&applyRemoveDevice, _1, device);
}

std::function<void(SealingSet&)> removeInvite(const Fingerprint& invite)
{
    return std::bind(&applyRemoveInvite, _1, invite);
}

std::function<void(SealingSet&)> setRole(const MemberId& person, Role role)
{
    return std::bind(&applySetRole, _1, person, role);
}

void changeRoster(DocumentSession& session, const std::function<void(SealingSet&)>& change)
{
    SealingSet changed = session.document().sealingSet();
    change(changed);
    session.setRoster(changed);
}

void nameTheWard(Document& document)
{
    document.metadata().addOrUpdateWard("123456", "Tanglewood", "654321", "Riverside Stake");
}

std::optional<double> latitudeOf(const Document& document, const FamilyId& family)
{
    const std::optional<Family> found = document.findFamilyById(family);
    return found.has_value() ? found.value().latitude() : std::nullopt;
}

ContactStatus statusOf(const Document& document, const FamilyId& family)
{
    if (!document.emergencyResponse().has_value())
    {
        return ContactStatus::NotContacted;
    }
    const FamilyResponseRecord* record = document.emergencyResponse().value().findRecord(family);
    return record != nullptr ? record->contactStatus() : ContactStatus::NotContacted;
}

std::optional<DocumentHeader> headerOnDisk(const QString& path)
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(path);
    DocumentFile::Sealed sealed;
    if (!bytes.has_value() || DocumentFile::decode(bytes.value(), sealed) != DocumentFile::Read::Ok)
    {
        return std::nullopt;
    }
    return sealed.header;
}

} // namespace SessionTestSupport
