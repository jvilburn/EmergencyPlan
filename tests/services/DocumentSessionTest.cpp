#include "DocumentSessionTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QtTest>

using namespace ServerContract;
using namespace SessionTestSupport;
using namespace std::placeholders;

using Timer = SessionTimers::Timer;

namespace
{

/// What a session told its window.
struct Recorder
{
    int replaced = 0;
    int adopted = 0;
    QList<DocumentSession::Conflict> conflicts;
    QStringList saveFailures;
    bool removed = false;
    QString removedBy;
    bool left = false;

    void watch(DocumentSession& session);

    void onReplaced() { ++replaced; }
    void onAdopted() { ++adopted; }
    void onConflict(const DocumentSession::Conflict& conflict) { conflicts.append(conflict); }
    void onSaveFailed(const QString& message) { saveFailures.append(message); }
    void onRemoved(const QString& byWhom, bool byLeaving);
};

void Recorder::watch(DocumentSession& session)
{
    QObject::connect(&session, &DocumentSession::documentReplaced, &session, std::bind(&Recorder::onReplaced, this));
    QObject::connect(&session, &DocumentSession::versionAdopted, &session, std::bind(&Recorder::onAdopted, this));
    QObject::connect(&session, &DocumentSession::conflictRaised, &session, std::bind(&Recorder::onConflict, this, _1));
    QObject::connect(&session, &DocumentSession::saveFailed, &session, std::bind(&Recorder::onSaveFailed, this, _1));
    QObject::connect(&session, &DocumentSession::removed, &session, std::bind(&Recorder::onRemoved, this, _1, _2));
}

void Recorder::onRemoved(const QString& byWhom, bool byLeaving)
{
    removed = true;
    removedBy = byWhom;
    left = byLeaving;
}

void markCalled(bool& called, Document&)
{
    called = true;
}

std::function<void(Document&)> noticeCall(bool& called)
{
    return std::bind(&markCalled, std::ref(called), _1);
}

void keepDownload(std::optional<DocumentSession::Download>& out, DocumentSession::Download download)
{
    out = download;
}

QByteArray bytesOf(const QString& path)
{
    return DocumentFile::readAll(path).value_or(QByteArray());
}

bool flipAByte(const QString& path)
{
    QByteArray bytes = bytesOf(path);
    if (bytes.isEmpty())
    {
        return false;
    }
    bytes[bytes.size() - 5] = char(bytes.at(bytes.size() - 5) ^ 0x01);
    return DocumentFile::write(path, bytes, nullptr);
}

qint64 counterOnDisk(const QString& path)
{
    const std::optional<DocumentHeader> header = headerOnDisk(path);
    return header.has_value() ? header.value().saveCounter : -1;
}

qint64 serverVersion(TestWard& ward, Device& reader)
{
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader);
    return copy.has_value() ? copy.value().version : -1;
}

/// Alice's shared ward with Bob granted: both sessions open and current, and
/// Alice holding the lease.
bool openAliceAndBob(TestWard& ward,
                     Device& alice,
                     Device& bob,
                     std::unique_ptr<DocumentSession>& aliceSession,
                     std::unique_ptr<DocumentSession>& bobSession)
{
    aliceSession = ward.createShared(alice);
    if (!aliceSession || !ward.grant(alice, bob))
    {
        return false;
    }

    aliceSession->checkServer();
    bobSession = bob.open(ward.documentId);
    ward.settle();
    return bobSession != nullptr;
}

/// The server's current version, from the head alone, so it reads whatever
/// format the current header is in.
qint64 headVersion(TestWard& ward, Device& reader)
{
    const Session session = ServerTestSupport::signIn(ward.server, asTestDevice(reader.identity)).value();
    return ServerTestSupport::wait<Head>([&](Reply<Head> reply) { ward.server.head(session, ward.documentId, reply); })
        .value->version;
}

/// Who holds the lease, as the server tells a device of the ward.
std::optional<Fingerprint> leaseHolder(TestWard& ward, Device& reader)
{
    const Session session = ServerTestSupport::signIn(ward.server, asTestDevice(reader.identity)).value();
    return ServerTestSupport::wait<Head>([&](Reply<Head> reply) { ward.server.head(session, ward.documentId, reply); })
        .value->leaseHolder;
}

std::optional<DocumentSession::Download> downloadTo(TestWard& ward, Device& device)
{
    QObject receiver;
    std::optional<DocumentSession::Download> result;
    DocumentSession::download(device.context(),
                              ward.documentId,
                              device.paths(ward.documentId),
                              &receiver,
                              std::bind(&keepDownload, std::ref(result), _1));
    ward.settle();
    return result;
}

RosterChange removal(const DeviceIdentity& person)
{
    RosterChange change;
    change.kind = RosterChange::Kind::Remove;
    change.target = person.member;
    return change;
}

RosterChange promotion(const DeviceIdentity& person)
{
    RosterChange change;
    change.kind = RosterChange::Kind::SetRole;
    change.target = person.member;
    change.role = RosterPerson::Role::Admin;
    return change;
}

/// Bob moves Adams to 50 while the server is unreachable and closes; then Alice
/// moves Adams to 60 and publishes. Bob has not yet seen hers.
bool divergeOnAdams(TestWard& ward,
                    Device& bob,
                    std::unique_ptr<DocumentSession>& aliceSession,
                    std::unique_ptr<DocumentSession>& bobSession)
{
    bobSession.reset();

    ward.server.setOffline(true);
    bobSession = bob.open(ward.documentId);
    ward.settle();
    if (!bobSession || bobSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 50.0))
                           != DocumentSession::Refusal::None)
    {
        return false;
    }
    bobSession.reset();
    ward.server.setOffline(false);

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 60.0));
    aliceSession->publishNow();
    ward.settle();
    return latitudeOf(ward.serverCopy(bob)->document, ward.adams) == 60.0;
}

} // namespace

void DocumentSessionTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Creating and opening --------------------------------------------------------

void DocumentSessionTest::aCreatedWardReopensWithOnePersonAndTheSameContent()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();

    std::unique_ptr<DocumentSession> created = alice.create(documentId);
    QVERIFY(created);
    QCOMPARE(created->syncState(), SyncState::Local);
    created->edit(DocumentSession::Edit::Directory, addFamily(FamilyId::generate(), 40.0));
    const Document content = created->document();
    created.reset();

    std::unique_ptr<DocumentSession> reopened = alice.open(documentId);
    QVERIFY(reopened);
    QVERIFY(reopened->document() == content);
    QCOMPARE(reopened->header().documentId, documentId);
    QCOMPARE(reopened->header().keyGeneration, 1);

    const AccessRoster& roster = reopened->document().accessRoster();
    QCOMPARE(roster.personCount(), 1);
    const RosterPerson person = roster.person(alice.identity.member).value();
    QVERIFY(person.isAdmin());
    QCOMPARE(person.devices(), QList<RosterDevice>{alice.identity.asRosterDevice()});
    QCOMPARE(reopened->header().recipients.size(), qsizetype(1));
    QVERIFY(reopened->header().recipients.first().fingerprint == alice.identity.fingerprint());
}

void DocumentSessionTest::openReportsEachFailure()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    const DocumentPaths paths = alice.paths(documentId);
    QVERIFY(alice.create(documentId));
    const QByteArray good = bytesOf(paths.file);

    LoadFailure failure = LoadFailure::None;
    QVERIFY(!alice.open(ChainTestSupport::newDocumentId(), failure));
    QCOMPARE(failure, LoadFailure::NotFound);

    DocumentFile::Sealed later;
    QCOMPARE(DocumentFile::decode(good, later), DocumentFile::Read::Ok);
    later.header.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    QVERIFY(DocumentFile::write(paths.file, DocumentFile::encode(later), nullptr));
    QVERIFY(!alice.open(documentId, failure));
    QCOMPARE(failure, LoadFailure::NeedsNewerVersion);

    QVERIFY(DocumentFile::write(paths.file, good, nullptr));
    QVERIFY(flipAByte(paths.file));
    QVERIFY(!alice.open(documentId, failure));
    QCOMPARE(failure, LoadFailure::Refused);

    // Anchor: the untouched file opens.
    QVERIFY(DocumentFile::write(paths.file, good, nullptr));
    QVERIFY(alice.open(documentId, failure));
    QCOMPARE(failure, LoadFailure::None);
}

void DocumentSessionTest::aFileRolledBackToALowerSaveCounterIsRefused()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    const DocumentPaths paths = alice.paths(ward.documentId);
    const QByteArray older = bytesOf(paths.file);

    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 45.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(session->syncState(), SyncState::UpToDate);
    session.reset();

    const QByteArray newer = bytesOf(paths.file);
    QVERIFY(DocumentFile::write(paths.file, older, nullptr));
    LoadFailure failure = LoadFailure::None;
    QVERIFY(!alice.open(ward.documentId, failure));
    QCOMPARE(failure, LoadFailure::Refused);

    QVERIFY(DocumentFile::write(paths.file, newer, nullptr));
    QVERIFY(alice.open(ward.documentId, failure));
}

void DocumentSessionTest::aFileRolledBackToALowerGenerationIsRefused()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, carol));
    session->checkServer();
    ward.settle();
    const DocumentPaths paths = alice.paths(ward.documentId);
    const QByteArray firstGeneration = bytesOf(paths.file);
    QCOMPARE(headerOnDisk(paths.file)->keyGeneration, 1);

    QVERIFY(ward.change(alice, removal(carol.identity)) == RosterChangeOutcome::Published);
    session->checkServer();
    ward.settle();
    QCOMPARE(session->header().keyGeneration, 2);
    session.reset();

    QVERIFY(DocumentFile::write(paths.file, firstGeneration, nullptr));
    LoadFailure failure = LoadFailure::None;
    QVERIFY(!alice.open(ward.documentId, failure));
    QCOMPARE(failure, LoadFailure::Refused);
}

void DocumentSessionTest::aSharedWardWhoseLocalFileWasDeletedIsDownloadedThenOpens()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));

    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(QFile::remove(paths.file));

    LoadFailure failure = LoadFailure::None;
    QVERIFY(!bob.open(ward.documentId, failure));
    QCOMPARE(failure, LoadFailure::NotFound);

    QVERIFY(downloadTo(ward, bob) == DocumentSession::Download::Done);
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    QVERIFY(bobSession->document() == ward.serverCopy(bob)->document);
}

void DocumentSessionTest::aSharedWardWhoseLocalFileWasCorruptedIsDownloadedThenOpens()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));

    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(flipAByte(paths.file));

    LoadFailure failure = LoadFailure::None;
    QVERIFY(!bob.open(ward.documentId, failure));
    QCOMPARE(failure, LoadFailure::Refused);

    QVERIFY(downloadTo(ward, bob) == DocumentSession::Download::Done);
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 40.0);
}

void DocumentSessionTest::atBaseWithNothingUnpublishedOpensTheLocalFile()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    session.reset();

    const QByteArray before = bytesOf(alice.paths(ward.documentId).file);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    Recorder recorder;
    recorder.watch(*session);
    ward.settle();

    QCOMPARE(session->syncState(), SyncState::UpToDate);
    QCOMPARE(bytesOf(alice.paths(ward.documentId).file), before);
    QCOMPARE(recorder.replaced, 0);
    QCOMPARE(serverVersion(ward, alice), qint64(1));
}

void DocumentSessionTest::aheadWithNothingUnpublishedFastForwards()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    bobSession.reset();

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 47.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 47.0);
    QCOMPARE(recorder.adopted, 1);
    QCOMPARE(recorder.replaced, 1);
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);

    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QCOMPARE(bytesOf(bob.paths(ward.documentId).file), copy.bytes());
    QVERIFY(bob.records.record(ward.documentId).baseVersion == copy.version);
}

void DocumentSessionTest::atBaseWithUnpublishedChangesPublishes()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    ward.server.setOffline(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 48.0));
    session.reset();
    ward.server.setOffline(false);
    QCOMPARE(serverVersion(ward, alice), qint64(1));

    session = alice.open(ward.documentId);
    QVERIFY(session);
    ward.settle();

    QCOMPARE(session->syncState(), SyncState::UpToDate);
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(2));
    QVERIFY(latitudeOf(copy.document, ward.adams) == 48.0);
}

void DocumentSessionTest::aheadWithUnpublishedChangesMerges()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));

    // Bob records a contact offline and closes; Alice moves a family meanwhile.
    ward.server.setOffline(true);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    bobSession.reset();
    ward.server.setOffline(false);

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 49.0));
    aliceSession->publishNow();
    ward.settle();

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QVERIFY(latitudeOf(copy.document, ward.adams) == 49.0);
    QCOMPARE(statusOf(copy.document, ward.baker), ContactStatus::OK);
    QVERIFY(bobSession->document() == copy.document);
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
}

void DocumentSessionTest::unreachableOpensTheLocalFile()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    const Document content = session->document();
    session.reset();

    ward.server.setOffline(true);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    ward.settle();

    QCOMPARE(session->syncState(), SyncState::Unreachable);
    QVERIFY(session->document() == content);
    QVERIFY(!session->isDirectoryReadOnly());
}

// --- Saving ----------------------------------------------------------------------

void DocumentSessionTest::severalEditsThenOneSaveAdvanceTheCounterByOne()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    const QString file = alice.paths(documentId).file;
    const qint64 before = counterOnDisk(file);

    const FamilyId family = FamilyId::generate();
    session->edit(DocumentSession::Edit::Directory, addFamily(family, 40.0));
    session->edit(DocumentSession::Edit::Directory, moveFamily(family, 41.0));
    session->edit(DocumentSession::Edit::Directory, moveFamily(family, 42.0));
    QCOMPARE(counterOnDisk(file), before);
    QCOMPARE(alice.timers->interval(Timer::Save), DocumentSession::SaveDelayMilliseconds);

    QVERIFY(alice.timers->fire(Timer::Save));
    QCOMPARE(counterOnDisk(file), before + 1);
    QVERIFY(!alice.timers->isRunning(Timer::Save));
}

void DocumentSessionTest::flushWritesAScheduledSaveAtOnce()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    const QString file = alice.paths(documentId).file;
    const qint64 before = counterOnDisk(file);

    QVERIFY(session->flush(nullptr));
    QCOMPARE(counterOnDisk(file), before);

    session->edit(DocumentSession::Edit::Directory, addFamily(FamilyId::generate(), 40.0));
    QVERIFY(session->flush(nullptr));
    QCOMPARE(counterOnDisk(file), before + 1);
    QVERIFY(!alice.timers->isRunning(Timer::Save));
}

void DocumentSessionTest::directoryAndResponseEditsSaveThroughTheSamePath()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    const QString file = alice.paths(documentId).file;
    const qint64 before = counterOnDisk(file);
    const FamilyId family = FamilyId::generate();

    QCOMPARE(session->edit(DocumentSession::Edit::Directory, addFamily(family, 40.0)), DocumentSession::Refusal::None);
    QVERIFY(alice.timers->fire(Timer::Save));
    QCOMPARE(counterOnDisk(file), before + 1);

    QCOMPARE(session->edit(DocumentSession::Edit::Response, recordContact(family, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(alice.timers->fire(Timer::Save));
    QCOMPARE(counterOnDisk(file), before + 2);

    std::unique_ptr<DocumentSession> reopened = alice.open(documentId);
    QCOMPARE(statusOf(reopened->document(), family), ContactStatus::OK);
}

void DocumentSessionTest::aFailedWriteReportsAndLeavesThePreviousFile()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    Recorder recorder;
    recorder.watch(*session);
    const QString file = alice.paths(documentId).file;
    const QByteArray before = bytesOf(file);

    QVERIFY(QFile::setPermissions(file, QFileDevice::ReadOwner | QFileDevice::ReadUser));
    session->edit(DocumentSession::Edit::Directory, addFamily(FamilyId::generate(), 40.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    QFile::setPermissions(file, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    QCOMPARE(recorder.saveFailures.size(), qsizetype(1));
    QCOMPARE(bytesOf(file), before);

    // The failed save stays scheduled: writable again, a flush writes it.
    QVERIFY(session->flush(nullptr));
    QCOMPARE(recorder.saveFailures.size(), qsizetype(1));
    QCOMPARE(counterOnDisk(file), session->header().saveCounter);
    QVERIFY(bytesOf(file) != before);

    const Document saved = session->document();
    session.reset();
    QVERIFY(alice.open(documentId)->document() == saved);
}

// --- Publishing ------------------------------------------------------------------

void DocumentSessionTest::offlineSavesLeaveOnePendingUpload()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    ward.server.setOffline(true);
    for (double latitude : {43.0, 44.0, 45.0})
    {
        session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, latitude));
        QVERIFY(alice.timers->fire(Timer::Save));
        ward.settle();
    }
    QCOMPARE(session->syncState(), SyncState::Unreachable);
    QVERIFY(alice.timers->isRunning(Timer::Retry));

    ward.server.setOffline(false);
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();

    // One publish: the local file, which is the last of the three saves.
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(2));
    QVERIFY(latitudeOf(copy.document, ward.adams) == 45.0);
    QCOMPARE(session->syncState(), SyncState::UpToDate);
    QVERIFY(!alice.timers->isRunning(Timer::Retry));
}

void DocumentSessionTest::theRetryIntervalGrowsAndCaps()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    ward.server.setOffline(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 43.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    const QList<int> expected = {5000, 10000, 20000, 40000, 80000, 160000, 300000, 300000};
    for (int interval : expected)
    {
        QCOMPARE(alice.timers->interval(Timer::Retry), interval);
        QVERIFY(alice.timers->fire(Timer::Retry));
        ward.settle();
    }

    // Any success resets it.
    ward.server.setOffline(false);
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();
    ward.server.setOffline(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 44.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(alice.timers->interval(Timer::Retry), 5000);
}

void DocumentSessionTest::aPublishPendingAtCloseIsPendingAgainOnReopen()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    ward.server.setOffline(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 46.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(session->syncState(), SyncState::Unreachable);
    session.reset();

    ward.server.setOffline(false);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    QCOMPARE(session->syncState(), SyncState::PendingUpload);
    ward.settle();

    QCOMPARE(session->syncState(), SyncState::UpToDate);
    QVERIFY(latitudeOf(ward.serverCopy(alice)->document, ward.adams) == 46.0);
}

void DocumentSessionTest::aSaveDuringAPublishPublishesAfterIt()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    Recorder recorder;
    recorder.watch(*session);

    ward.server.holdReplies(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 50.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    QCOMPARE(ward.server.waitingReplies(), qsizetype(1));

    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 51.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    QCOMPARE(ward.server.waitingReplies(), qsizetype(1));

    // A head check while the publish is in flight asks the server nothing.
    QVERIFY(alice.timers->fire(Timer::HeadCheck));
    QCOMPARE(ward.server.waitingReplies(), qsizetype(1));

    ward.server.holdReplies(false);
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(3));
    QVERIFY(latitudeOf(copy.document, ward.adams) == 51.0);
    QVERIFY(recorder.conflicts.isEmpty());
    QCOMPARE(recorder.adopted, 0);
    QCOMPARE(session->syncState(), SyncState::UpToDate);
}

void DocumentSessionTest::aPublishWhoseReplyWasLostIsAdoptedAsItsOwn()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    Recorder recorder;
    recorder.watch(*session);

    ward.server.loseReplies(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 52.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    ward.server.loseReplies(false);
    QCOMPARE(session->syncState(), SyncState::Unreachable);
    QCOMPARE(serverVersion(ward, alice), qint64(2));

    // Retried, refused as a conflict with itself, and taken in: no conflict and
    // nothing published a second time.
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    QCOMPARE(serverVersion(ward, alice), qint64(2));
    QVERIFY(alice.records.record(ward.documentId).baseVersion == qint64(2));
    QCOMPARE(session->syncState(), SyncState::UpToDate);
    QVERIFY(latitudeOf(session->document(), ward.adams) == 52.0);
}

void DocumentSessionTest::publishesAndFetchesWriteABaseCopyMatchingTheRecord()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 53.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    bobSession->checkServer();
    ward.settle();

    // Alice's publish and Bob's fetch.
    for (Device* device : {&alice, &bob})
    {
        const DocumentPaths paths = device->paths(ward.documentId);
        const std::optional<ServerVersion::BaseCopy> base =
            ServerVersion::readBase(ward.documentId, paths, device->records, device->identity.self());
        QVERIFY(base.has_value());
        QCOMPARE(base.value().sealed.header.payloadHash, device->records.record(ward.documentId).baseHash);
        QVERIFY(latitudeOf(base.value().document, ward.adams) == 53.0);
    }

    const DocumentPaths paths = bob.paths(ward.documentId);
    const QByteArray base = bytesOf(paths.base);
    QVERIFY(flipAByte(paths.base));
    QVERIFY(!ServerVersion::readBase(ward.documentId, paths, bob.records, bob.identity.self()).has_value());

    QVERIFY(DocumentFile::write(paths.base, base, nullptr));
    QVERIFY(ServerVersion::readBase(ward.documentId, paths, bob.records, bob.identity.self()).has_value());
    QVERIFY(QFile::remove(paths.base));
    QVERIFY(!ServerVersion::readBase(ward.documentId, paths, bob.records, bob.identity.self()).has_value());
}

// --- Downloads -------------------------------------------------------------------

enum class Sabotage
{
    Unsealable,
    HashMismatch,
    BadSignature,
    BrokenChain
};

void DocumentSessionTest::aServerVersionFailingValidationTouchesNothing_data()
{
    QTest::addColumn<int>("sabotage");
    QTest::newRow("unsealable") << int(Sabotage::Unsealable);
    QTest::newRow("hash mismatch") << int(Sabotage::HashMismatch);
    QTest::newRow("bad signature") << int(Sabotage::BadSignature);
    QTest::newRow("broken chain") << int(Sabotage::BrokenChain);
}

void DocumentSessionTest::aServerVersionFailingValidationTouchesNothing()
{
    QFETCH(int, sabotage);

    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    bobSession.reset();

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 54.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    DocumentHeader forged = copy.sealed.header;
    const DeviceIdentity stranger = newIdentity("Stranger", "Laptop");

    switch (Sabotage(sabotage))
    {
    case Sabotage::Unsealable:
        for (RecipientEntry& entry : forged.recipients)
        {
            if (entry.fingerprint == bob.identity.fingerprint())
            {
                entry.sealedKey = DocumentCrypto::sealDataKey(copy.dataKey, stranger.sealing.publicKey);
            }
        }
        forged.writerSignature = DocumentCrypto::signHeader(forged, alice.identity.signing.signingKey);
        break;
    case Sabotage::HashMismatch:
        forged.payloadHash = DocumentCrypto::payloadHash("some other ciphertext");
        forged.writerSignature = DocumentCrypto::signHeader(forged, alice.identity.signing.signingKey);
        break;
    case Sabotage::BadSignature:
        forged.writerSignature[0] = char(forged.writerSignature.at(0) ^ 0x01);
        break;
    case Sabotage::BrokenChain:
        forged.lastWriter = stranger.fingerprint();
        forged.writerPublicKey = stranger.sealing.publicKey;
        forged.writerVerifyKey = stranger.signing.verifyKey;
        forged.writerSignature = DocumentCrypto::signHeader(forged, stranger.signing.signingKey);
        break;
    }
    ward.server.substituteHeader(ward.documentId, copy.version, forged);

    const DocumentPaths paths = bob.paths(ward.documentId);
    const QByteArray file = bytesOf(paths.file);
    const QByteArray base = bytesOf(paths.base);
    const DocumentRecord record = bob.records.record(ward.documentId);

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QCOMPARE(bobSession->syncState(), SyncState::ServerFault);
    QCOMPARE(bytesOf(paths.file), file);
    QCOMPARE(bytesOf(paths.base), base);
    QVERIFY(bob.records.record(ward.documentId) == record);
    QCOMPARE(recorder.replaced, 0);
    QVERIFY(!recorder.removed);
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 40.0);
}

void DocumentSessionTest::aVersionBelowTheHighWaterMarkIsRefused()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));

    // Bob has seen a later generation than the server now offers, and has no base
    // copy to walk a chain from, so the version is judged by itself.
    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(QFile::remove(paths.file));
    QVERIFY(QFile::remove(paths.base));
    DocumentRecord record = bob.records.record(ward.documentId);
    record.highWaterMark.keyGeneration = 2;
    bob.records.setRecord(ward.documentId, record);

    QVERIFY(downloadTo(ward, bob) == DocumentSession::Download::ServerFault);
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(bob.records.record(ward.documentId) == record);

    // Anchor: at the generation the server serves, the same download succeeds.
    record.highWaterMark.keyGeneration = 1;
    bob.records.setRecord(ward.documentId, record);
    QVERIFY(downloadTo(ward, bob) == DocumentSession::Download::Done);
}

// --- The refused-publish route ---------------------------------------------------

void DocumentSessionTest::aRefusedPublishAdoptsTheirKeyGenerationAndRoster()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.grant(alice, carol));
    QVERIFY(ward.change(alice, removal(carol.identity)) == RosterChangeOutcome::Published);

    Recorder recorder;
    recorder.watch(*bobSession);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    QCOMPARE(bobSession->header().keyGeneration, 2);
    QVERIFY(!bobSession->document().accessRoster().contains(carol.identity.member));
    QVERIFY(!bobSession->header().entryFor(carol.identity.fingerprint()).has_value());

    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::OK);
    QCOMPARE(copy.sealed.header.keyGeneration, 2);
    QVERIFY(copy.sealed.header == bobSession->header());
}

void DocumentSessionTest::aResponseChangeAndAFamilyEditMergeSilently()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.baker, 55.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    const qint64 theirs = serverVersion(ward, alice);

    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::UnableToReach));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QCOMPARE(copy.version, theirs + 1);
    QVERIFY(latitudeOf(copy.document, ward.baker) == 55.0);
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::UnableToReach);
    QVERIFY(bob.records.record(ward.documentId).baseVersion == copy.version);
}

void DocumentSessionTest::twoRefusalsInARowBothMergeSilently()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.grant(alice, carol));
    std::unique_ptr<DocumentSession> carolSession = carol.open(ward.documentId);
    QVERIFY(carolSession);
    aliceSession->checkServer();
    bobSession->checkServer();
    ward.settle();

    Recorder bobRecorder;
    bobRecorder.watch(*bobSession);
    Recorder carolRecorder;
    carolRecorder.watch(*carolSession);

    // Alice moves Adams; Bob catches up. Carol has not.
    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 56.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    bobSession->checkServer();
    ward.settle();

    // Carol's publish is refused; while she fetches, Bob publishes, so her merged
    // publish is refused again.
    carolSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    ward.server.holdReplies(true);
    QVERIFY(carol.timers->fire(Timer::Save));
    QVERIFY(ward.server.deliverNext());

    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    QVERIFY(ward.server.deliverNext());

    ward.server.holdReplies(false);
    ward.settle();

    QVERIFY(carolRecorder.conflicts.isEmpty());
    QVERIFY(bobRecorder.conflicts.isEmpty());
    QCOMPARE(carolRecorder.adopted, 2);

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QVERIFY(latitudeOf(copy.document, ward.adams) == 56.0);
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::OK);
    QCOMPARE(statusOf(copy.document, ward.baker), ContactStatus::OK);
    QVERIFY(carolSession->document() == copy.document);
}

void DocumentSessionTest::aFamilyEditOnBothSidesRaisesAConflict()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));
    const qint64 theirs = serverVersion(ward, alice);

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QCOMPARE(recorder.conflicts.size(), qsizetype(1));
    QCOMPARE(recorder.conflicts.first().writerName, QString("Alice"));
    QCOMPARE(serverVersion(ward, alice), theirs);
    QVERIFY(latitudeOf(ward.serverCopy(alice)->document, ward.adams) == 60.0);
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 50.0);

    // No head check runs until resolve, and nothing publishes.
    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    bobSession->publishNow();
    QCOMPARE(ward.server.waitingReplies(), qsizetype(0));
    ward.settle();
    QCOMPARE(serverVersion(ward, alice), theirs);
}

void DocumentSessionTest::reopeningBeforeResolveMeetsTheConflictAgain()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();
    bobSession.reset();

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QCOMPARE(recorder.conflicts.size(), qsizetype(1));
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 50.0);
    QVERIFY(latitudeOf(ward.serverCopy(alice)->document, ward.adams) == 60.0);
}

void DocumentSessionTest::keepMineAfterTheyGrantedKeepsMyDirectoryAndTheirPerson()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));
    QVERIFY(ward.grant(alice, carol));

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();
    QCOMPARE(recorder.conflicts.size(), qsizetype(1));
    const qint64 theirs = serverVersion(ward, alice);

    QVERIFY(bobSession->resolve(DocumentSession::Resolution::KeepMine, nullptr));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, theirs + 1);
    QVERIFY(latitudeOf(copy.document, ward.adams) == 50.0);
    QVERIFY(copy.document.accessRoster().contains(carol.identity.member));
    QVERIFY(copy.sealed.header.entryFor(carol.identity.fingerprint()).has_value());
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
}

void DocumentSessionTest::keepMineNeverRestoresAMemberTheyRemoved()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.grant(alice, carol));
    bobSession->checkServer();
    ward.settle();
    QVERIFY(bobSession->document().accessRoster().contains(carol.identity.member));

    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));
    QVERIFY(ward.change(alice, removal(carol.identity)) == RosterChangeOutcome::Published);

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();
    QCOMPARE(recorder.conflicts.size(), qsizetype(1));

    QVERIFY(bobSession->resolve(DocumentSession::Resolution::KeepMine, nullptr));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QVERIFY(latitudeOf(copy.document, ward.adams) == 50.0);
    QVERIFY(!copy.document.accessRoster().contains(carol.identity.member));
    QVERIFY(!copy.sealed.header.entryFor(carol.identity.fingerprint()).has_value());
    QCOMPARE(copy.sealed.header.keyGeneration, 2);
}

void DocumentSessionTest::keepTheirsHoldsTheirDirectoryAndTheMergedResponse()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));

    // Bob records Baker offline as well as moving Adams; Alice records Adams.
    bobSession.reset();
    ward.server.setOffline(true);
    bobSession = bob.open(ward.documentId);
    ward.settle();
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    bobSession.reset();
    ward.server.setOffline(false);
    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));
    aliceSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::UnableToReach));
    aliceSession->publishNow();
    ward.settle();

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();
    QCOMPARE(recorder.conflicts.size(), qsizetype(1));

    QVERIFY(bobSession->resolve(DocumentSession::Resolution::KeepTheirs, nullptr));
    ward.settle();

    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 60.0);
    QCOMPARE(statusOf(bobSession->document(), ward.adams), ContactStatus::UnableToReach);
    QCOMPARE(statusOf(bobSession->document(), ward.baker), ContactStatus::OK);
    QVERIFY(recorder.replaced >= 1);

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QVERIFY(copy.document == bobSession->document());
}

// --- Removal ---------------------------------------------------------------------

void DocumentSessionTest::aProvenRevocationMidSessionDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);
    bobSession->checkServer();
    ward.settle();

    QVERIFY(recorder.removed);
    QCOMPARE(recorder.removedBy, QString("Alice"));
    QVERIFY(!recorder.left);
    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!bob.records.contains(ward.documentId));
    QVERIFY(!bob.timers->isRunning(Timer::HeadCheck));
    QVERIFY(!bob.timers->isRunning(Timer::Heartbeat));
}

void DocumentSessionTest::aProvenRevocationOnOpenDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    bobSession.reset();

    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QVERIFY(recorder.removed);
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(!bob.records.contains(ward.documentId));
}

void DocumentSessionTest::aProvenRevocationOnAPublishDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    QVERIFY(recorder.removed);
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(!bob.records.contains(ward.documentId));
}

void DocumentSessionTest::aRevocationByAnAdminGrantedAfterTheBaseIsProven()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    // Carol joins and is made an admin after Bob's base; then she removes him.
    QVERIFY(ward.grant(alice, carol));
    QVERIFY(ward.change(alice, promotion(carol.identity)) == RosterChangeOutcome::Published);
    QVERIFY(ward.change(carol, removal(bob.identity)) == RosterChangeOutcome::Published);

    bobSession->checkServer();
    ward.settle();

    QVERIFY(recorder.removed);
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
}

void DocumentSessionTest::aWithheldHeaderDeletesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    QVERIFY(ward.grant(alice, carol));
    QVERIFY(ward.change(alice, promotion(carol.identity)) == RosterChangeOutcome::Published);
    const qint64 promotedAt = serverVersion(ward, alice);
    QVERIFY(ward.change(carol, removal(bob.identity)) == RosterChangeOutcome::Published);

    // Without the promotion, Carol's removal of Bob does not follow.
    ward.server.withholdHeader(ward.documentId, promotedAt);
    bobSession->checkServer();
    ward.settle();

    QVERIFY(!recorder.removed);
    QCOMPARE(bobSession->syncState(), SyncState::ServerFault);
    QVERIFY(QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(QFile::exists(bob.paths(ward.documentId).base));
    QVERIFY(bob.records.contains(ward.documentId));
}

void DocumentSessionTest::aSubstitutedHeaderDeletesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    // A header that drops Bob, served in place of a real version: signed by
    // someone who was never a member.
    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 57.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    const DeviceIdentity stranger = newIdentity("Stranger", "Laptop");
    DocumentHeader forged = copy.sealed.header;
    forged.recipients.removeIf([&bob](const RecipientEntry& entry) { return entry.member == bob.identity.member; });
    forged.lastWriter = stranger.fingerprint();
    forged.writerPublicKey = stranger.sealing.publicKey;
    forged.writerVerifyKey = stranger.signing.verifyKey;
    forged.writerSignature = DocumentCrypto::signHeader(forged, stranger.signing.signingKey);
    ward.server.substituteHeader(ward.documentId, copy.version, forged);

    bobSession->checkServer();
    ward.settle();

    QVERIFY(!recorder.removed);
    QCOMPARE(bobSession->syncState(), SyncState::ServerFault);
    QVERIFY(QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(bob.records.contains(ward.documentId));
}

void DocumentSessionTest::aRefusalWithNoProvingChainDeletesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    // The server refuses Bob, and Bob has no base copy to prove anything from.
    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);
    QVERIFY(QFile::remove(bob.paths(ward.documentId).base));
    bobSession->checkServer();
    ward.settle();

    QVERIFY(!recorder.removed);
    QCOMPARE(bobSession->syncState(), SyncState::ServerFault);
    QVERIFY(QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(bob.records.contains(ward.documentId));
}

void DocumentSessionTest::aLeaveFromThisPersonsOtherDeviceReportsLeft()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& laptop = ward.add(newIdentity("Bob", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> laptopSession;
    QVERIFY(openAliceAndBob(ward, alice, laptop, aliceSession, laptopSession));
    Recorder recorder;
    recorder.watch(*laptopSession);

    QVERIFY(ward.pair(phone, laptop));
    RosterChange link;
    link.kind = RosterChange::Kind::Link;
    link.device = phone.identity.asRosterDevice();
    QVERIFY(ward.change(laptop, link) == RosterChangeOutcome::Published);
    QVERIFY(downloadTo(ward, phone) == DocumentSession::Download::Done);

    RosterChange leave;
    leave.kind = RosterChange::Kind::Leave;
    QVERIFY(ward.change(phone, leave) == RosterChangeOutcome::Published);

    laptopSession->checkServer();
    ward.settle();

    QVERIFY(recorder.removed);
    QVERIFY(recorder.left);
    QVERIFY(!QFile::exists(laptop.paths(ward.documentId).file));
}

// --- Read-only and updates -------------------------------------------------------

void DocumentSessionTest::aForeignLeaseRefusesDirectoryEditsButNotResponses()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(session->mayWriteDirectory());

    ward.server.holdLeaseAs(ward.documentId, newIdentity("Someone", "Laptop").fingerprint());
    session->checkServer();
    ward.settle();
    QVERIFY(session->isDirectoryReadOnly());
    QVERIFY(!session->mayWriteDirectory());

    const QString file = alice.paths(ward.documentId).file;
    const qint64 before = counterOnDisk(file);
    const Document content = session->document();

    bool called = false;
    QCOMPARE(session->edit(DocumentSession::Edit::Directory, noticeCall(called)),
             DocumentSession::Refusal::DirectoryReadOnly);
    QVERIFY(!called);
    QVERIFY(session->document() == content);
    QVERIFY(!alice.timers->isRunning(Timer::Save));
    QCOMPARE(counterOnDisk(file), before);

    QCOMPARE(session->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(counterOnDisk(file), before + 1);
    QCOMPARE(statusOf(ward.serverCopy(alice)->document, ward.adams), ContactStatus::OK);
}

void DocumentSessionTest::aLeaseWhoseHeartbeatStoppedIsTakenOverAfterExpiry()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(bobSession->isDirectoryReadOnly());
    QVERIFY(!bobSession->mayWriteDirectory());

    // Alice's app stops without releasing; her lease runs out.
    aliceSession.reset();
    QVERIFY(ServerTestSupport::wait<Done>([&](Reply<Done> reply)
    {
        ward.server.advanceClock(FakeServer::Limits().leaseSeconds + 1, reply);
    }).ok());

    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(!bobSession->isDirectoryReadOnly());
    QVERIFY(bobSession->mayWriteDirectory());
    QCOMPARE(bob.timers->interval(Timer::Heartbeat), DocumentSession::HeartbeatMilliseconds);
}

void DocumentSessionTest::goingUnreachableLeavesTheSessionEditable()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(!aliceSession->isDirectoryReadOnly());
    QVERIFY(bobSession->isDirectoryReadOnly());

    ward.server.setOffline(true);
    QVERIFY(alice.timers->fire(Timer::HeadCheck));
    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    // The lease cannot speak for a disconnected device.
    QVERIFY(!aliceSession->isDirectoryReadOnly());
    QVERIFY(!bobSession->isDirectoryReadOnly());
    QCOMPARE(bobSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 58.0)),
             DocumentSession::Refusal::None);
    QCOMPARE(aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.baker, 58.0)),
             DocumentSession::Refusal::None);
}

void DocumentSessionTest::aReadOnlySessionFastForwardsToTheHoldersPublish()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(bobSession->isDirectoryReadOnly());
    Recorder recorder;
    recorder.watch(*bobSession);

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 59.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 59.0);
    QCOMPARE(recorder.replaced, 1);
    QVERIFY(bobSession->isDirectoryReadOnly());
}

void DocumentSessionTest::headChecksRunFasterDuringAnEmergency()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    QCOMPARE(alice.timers->interval(Timer::HeadCheck), DocumentSession::HeadCheckMilliseconds);
    session->setEmergencyActive(true);
    QCOMPARE(alice.timers->interval(Timer::HeadCheck), DocumentSession::EmergencyHeadCheckMilliseconds);

    // Restarted at the same pace after each check.
    QVERIFY(alice.timers->fire(Timer::HeadCheck));
    ward.settle();
    QCOMPARE(alice.timers->interval(Timer::HeadCheck), DocumentSession::EmergencyHeadCheckMilliseconds);

    session->setEmergencyActive(false);
    QCOMPARE(alice.timers->interval(Timer::HeadCheck), DocumentSession::HeadCheckMilliseconds);
}

void DocumentSessionTest::destroyingASessionReleasesTheLeaseAndWritesItsSave()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(alice.timers->isRunning(Timer::Heartbeat));
    QVERIFY(leaseHolder(ward, alice) == alice.identity.fingerprint());

    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 62.0));
    session.reset();
    ward.settle();

    QVERIFY(!leaseHolder(ward, alice).has_value());
    QVERIFY(latitudeOf(alice.open(ward.documentId)->document(), ward.adams) == 62.0);
}

void DocumentSessionTest::aLeaseGrantedAfterTheSessionIsGoneIsReleased()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    session.reset();
    ward.settle();
    QVERIFY(!leaseHolder(ward, alice).has_value());

    // Opened, its lease request granted at the server, and destroyed before the
    // answer arrives: the release follows the request there.
    ward.server.holdReplies(true);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    QVERIFY(ward.server.waitingReplies() > 0);
    session.reset();
    ward.server.holdReplies(false);
    ward.settle();

    QVERIFY(!leaseHolder(ward, alice).has_value());
}

void DocumentSessionTest::aLaterFormatRequiresAnUpdateUntilTheServerServesOneThisBuildReads()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));

    // Bob holds the lease, so his directory edits are refused only for the reason
    // under test.
    aliceSession.reset();
    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();
    QVERIFY(bobSession->mayWriteDirectory());

    // A later build writes the next version.
    std::unique_ptr<DocumentSession> writer = alice.open(ward.documentId);
    QVERIFY(writer);
    ward.settle();
    writer->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::UnableToReach));
    writer->publishNow();
    ward.settle();
    const ServerVersion::Fetched later = ward.serverCopy(alice).value();
    DocumentHeader laterFormat = later.sealed.header;
    laterFormat.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    ward.server.substituteHeader(ward.documentId, later.version, laterFormat);

    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();
    QVERIFY(bobSession->requiresUpdate());

    const QString file = bob.paths(ward.documentId).file;
    const qint64 before = counterOnDisk(file);
    bool called = false;
    QCOMPARE(bobSession->edit(DocumentSession::Edit::Directory, noticeCall(called)),
             DocumentSession::Refusal::RequiresUpdate);
    QVERIFY(!called);

    QCOMPARE(bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(counterOnDisk(file), before + 1);
    QCOMPARE(headVersion(ward, alice), later.version);

    // The server moves on to a version this build reads: the update clears and
    // the saved record publishes.
    writer->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    writer->publishNow();
    ward.settle();

    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(!bobSession->requiresUpdate());
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, later.version + 2);
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::OK);
    QCOMPARE(statusOf(copy.document, ward.baker), ContactStatus::OK);
}

// --- Lifetime --------------------------------------------------------------------

void DocumentSessionTest::destroyingASessionWithRepliesHeldWritesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    session.reset();

    ward.server.holdReplies(true);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    session->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    session->publishNow();
    QVERIFY(ward.server.waitingReplies() > 0);

    const DocumentPaths paths = alice.paths(ward.documentId);
    const QByteArray file = bytesOf(paths.file);
    const QByteArray base = bytesOf(paths.base);
    const DocumentRecord record = alice.records.record(ward.documentId);

    session.reset();
    ward.server.holdReplies(false);
    ward.settle();

    QCOMPARE(bytesOf(paths.file), file);
    QCOMPARE(bytesOf(paths.base), base);
    QVERIFY(alice.records.record(ward.documentId) == record);
}

// --- Sharing ---------------------------------------------------------------------

void DocumentSessionTest::sharingKeepsTheIdAndPathsAndRecordsABase()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ChainTestSupport::newDocumentId();
    const DocumentPaths paths = alice.paths(documentId);
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    QVERIFY(!session->isShared());
    QVERIFY(!QFile::exists(paths.base));

    session->share({});
    ward.settle();

    QVERIFY(session->isShared());
    QCOMPARE(session->documentId(), documentId);
    QCOMPARE(session->header().documentId, documentId);
    QVERIFY(QFile::exists(paths.file));
    QVERIFY(QFile::exists(paths.base));
    QVERIFY(alice.records.record(documentId).baseVersion == qint64(1));
    QCOMPARE(session->syncState(), SyncState::UpToDate);

    ward.documentId = documentId;
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(1));
    QCOMPARE(copy.bytes(), bytesOf(paths.file));
}

void DocumentSessionTest::aSecondDeviceGrantedAccessOpensIt()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);

    QVERIFY(!bob.open(ward.documentId));
    QVERIFY(ward.grant(alice, bob));

    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 40.0);
    QVERIFY(bobSession->document().accessRoster().contains(bob.identity.member));
    QVERIFY(!bobSession->document().accessRoster().person(bob.identity.member)->isAdmin());
}

void DocumentSessionTest::aLinkQueuedWhileUnsharedJoinsTheFirstHeader()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));

    // Pairing needs the laptop to be a member somewhere.
    std::unique_ptr<DocumentSession> other = ward.createShared(laptop);
    QVERIFY(other);
    QVERIFY(ward.pair(phone, laptop));

    const QString documentId = ChainTestSupport::newDocumentId();
    std::unique_ptr<DocumentSession> session = laptop.create(documentId);
    QVERIFY(session);

    RosterChange link;
    link.kind = RosterChange::Kind::Link;
    link.device = phone.identity.asRosterDevice();
    std::unique_ptr<RosterChangeQueue> queue = laptop.queue();
    queue->enqueue(documentId, link);
    ward.settle();
    QCOMPARE(queue->pending(documentId), QList<RosterChange>{link});

    queue->setOpenSession(session.get());
    queue->shareOpenWard();
    ward.settle();

    QVERIFY(session->isShared());
    QVERIFY(session->header().entryFor(phone.identity.fingerprint()).has_value());
    QVERIFY(laptop.records.record(documentId).pendingChanges.isEmpty());

    ward.documentId = documentId;
    QVERIFY(downloadTo(ward, phone) == DocumentSession::Download::Done);
}
