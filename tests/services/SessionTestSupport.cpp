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

void keepDownload(std::optional<DocumentSession::Download>& out, DocumentSession::Download download)
{
    out = download;
}

void keepOutcome(std::optional<RosterChangeOutcome>& out,
                 const QString&,
                 const RosterChange&,
                 RosterChangeOutcome outcome)
{
    out = outcome;
}

/// Timers for a session or queue to own, with a pointer kept for the test to
/// fire them.
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
    identity.member = ChainTestSupport::newMember();
    identity.displayName = displayName;
    identity.deviceName = deviceName;
    return identity;
}

DeviceIdentity newDeviceOf(const DeviceIdentity& person, const QString& deviceName)
{
    DeviceIdentity identity = newIdentity(person.displayName, deviceName);
    identity.member = person.member;
    return identity;
}

ChainTestSupport::TestDevice asTestDevice(const DeviceIdentity& identity)
{
    return ChainTestSupport::TestDevice{identity.member, identity.sealing, identity.signing};
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

std::unique_ptr<RosterChangeQueue> Device::queue()
{
    return std::make_unique<RosterChangeQueue>(identity,
                                               records,
                                               connection,
                                               keptTimers(queueTimers),
                                               std::bind(&Device::paths, this, _1),
                                               nullptr);
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
    documentId = ChainTestSupport::newDocumentId();
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

    session->share({});
    settle();
    if (!session->isShared())
    {
        return nullptr;
    }
    return session;
}

bool TestWard::grant(Device& admin, Device& newcomer)
{
    const ChainTestSupport::TestDevice asked = asTestDevice(newcomer.identity);
    const std::optional<Session> session = ServerTestSupport::signIn(server, asked);
    if (!session.has_value()
        || !ServerTestSupport::signedRequestAccess(server,
                                                   session.value(),
                                                   documentId,
                                                   ServerTestSupport::requestFor(asked, ""))
                .ok())
    {
        return false;
    }

    RosterChange grant;
    grant.kind = RosterChange::Kind::Grant;
    grant.person = newcomer.identity.asNewPerson();
    if (change(admin, grant) != RosterChangeOutcome::Published)
    {
        return false;
    }

    QObject receiver;
    std::optional<DocumentSession::Download> downloaded;
    DocumentSession::download(newcomer.context(),
                              documentId,
                              newcomer.paths(documentId),
                              &receiver,
                              std::bind(&keepDownload, std::ref(downloaded), _1));
    settle();
    return downloaded == DocumentSession::Download::Done;
}

std::optional<RosterChangeOutcome> TestWard::change(Device& device, const RosterChange& change)
{
    std::unique_ptr<RosterChangeQueue> queue = device.queue();
    std::optional<RosterChangeOutcome> outcome;
    QObject::connect(queue.get(),
                     &RosterChangeQueue::changeFinished,
                     queue.get(),
                     std::bind(&keepOutcome, std::ref(outcome), _1, _2, _3));
    queue->enqueue(documentId, change);
    settle();
    return outcome;
}

bool TestWard::pair(Device& newDevice, Device& existing)
{
    const ChainTestSupport::TestDevice initiator = asTestDevice(newDevice.identity);
    const ChainTestSupport::TestDevice responder = asTestDevice(existing.identity);
    const std::optional<Session> initiatorSession = ServerTestSupport::signIn(server, initiator);
    const std::optional<Session> responderSession = ServerTestSupport::signIn(server, responder);
    return initiatorSession.has_value() && responderSession.has_value()
        && ServerTestSupport::completePairing(server,
                                              initiator,
                                              initiatorSession.value(),
                                              responder,
                                              responderSession.value());
}

std::optional<ServerVersion::Fetched> TestWard::serverCopy(Device& reader)
{
    // A throwaway record and paths no file has, so reading the server's copy
    // changes nothing the device keeps.
    MemoryRecords scratch;
    QObject receiver;
    std::optional<ServerVersion::Fetched> fetched;
    ServerVersion::fetch(reader.identity,
                         scratch,
                         reader.connection,
                         documentId,
                         reader.paths("server copy"),
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
