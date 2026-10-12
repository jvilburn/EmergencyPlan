#include "DocumentSessionTest.h"

#include "DocumentCrypto.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QtTest>

using ServerContract::Done;
using ServerContract::Head;
using ServerContract::ServerApi;
using ServerVersion::LostAccess;
using namespace SessionTestSupport;
using namespace std::placeholders;

using Timer = SessionTimers::Timer;

namespace
{

/// What a session told its window.
struct Recorder
{
    int replaced = 0;
    int rosterChanges = 0;
    QList<DocumentSession::Conflict> conflicts;
    QStringList saveFailures;
    std::optional<LostAccess> lost;
    QList<ServerContract::ServerError> leaveFailures;

    void watch(DocumentSession& session);

    void onReplaced() { ++replaced; }
    void onRosterChanged() { ++rosterChanges; }
    void onConflict(const DocumentSession::Conflict& conflict) { conflicts.append(conflict); }
    void onSaveFailed(const QString& message) { saveFailures.append(message); }
    void onLost(const LostAccess& how) { lost = how; }
    void onLeaveFailed(ServerContract::ServerError error) { leaveFailures.append(error); }
};

void Recorder::watch(DocumentSession& session)
{
    QObject::connect(&session, &DocumentSession::documentReplaced, &session, std::bind(&Recorder::onReplaced, this));
    QObject::connect(&session, &DocumentSession::conflictRaised, &session, std::bind(&Recorder::onConflict, this, _1));
    QObject::connect(&session, &DocumentSession::saveFailed, &session, std::bind(&Recorder::onSaveFailed, this, _1));
    QObject::connect(&session, &DocumentSession::accessLost, &session, std::bind(&Recorder::onLost, this, _1));
    QObject::connect(&session, &DocumentSession::rosterChanged, &session, std::bind(&Recorder::onRosterChanged, this));
    QObject::connect(&session, &DocumentSession::leaveFailed, &session, std::bind(&Recorder::onLeaveFailed, this, _1));
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

/// Alice's shared ward with Bob joined through her invite: both sessions open and
/// current, and Alice holding the lease.
bool openAliceAndBob(TestWard& ward,
                     Device& alice,
                     Device& bob,
                     std::unique_ptr<DocumentSession>& aliceSession,
                     std::unique_ptr<DocumentSession>& bobSession)
{
    aliceSession = ward.createShared(alice);
    if (!aliceSession || !ward.addUser(*aliceSession, bob, Role::Member))
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
qint64 headVersion(TestWard& ward)
{
    return ward.head().value().version;
}

/// Who holds the lease, as the server tells anyone.
std::optional<Fingerprint> leaseHolder(TestWard& ward)
{
    return ward.head().value().leaseHolder;
}

/// A roster edit made on by, which catches up first, and published. True once the
/// version without what change took out has landed.
bool editRosterOn(TestWard& ward, DocumentSession& by, const std::function<void(Document&)>& change)
{
    by.checkServer();
    ward.settle();
    by.edit(DocumentSession::Edit::Roster, change);
    by.publishNow();
    ward.settle();
    return by.syncState() == SyncState::UpToDate;
}

/// Joins link on device and opens the ward there.
std::unique_ptr<DocumentSession> joinAndOpen(TestWard& ward, Device& device, const FileOpenRouter::InviteLink& link)
{
    if (ward.join(device, link) != DocumentSession::Join::Done)
    {
        return nullptr;
    }
    std::unique_ptr<DocumentSession> session = device.open(ward.documentId);
    ward.settle();
    return session;
}

/// The fingerprints the server's current version seals to, as sorted hex.
QStringList recipientsOnServer(TestWard& ward)
{
    return ServerTestSupport::recipientsOf(ward.head().value());
}

QStringList hexOf(const QList<const DeviceIdentity*>& devices)
{
    QStringList fingerprints;
    for (const DeviceIdentity* device : devices)
    {
        fingerprints.append(device->fingerprint().toHex());
    }
    fingerprints.sort();
    return fingerprints;
}

/// Whether a device can open the data key of a version.
bool canUnseal(const DocumentHeader& header, const DeviceIdentity& device)
{
    const std::optional<RecipientEntry> entry = header.entryFor(device.fingerprint());
    return entry.has_value() && DocumentCrypto::unsealDataKey(entry.value().sealedKey, device.sealing).has_value();
}

/// The data key a device opens from the server's current version.
DataKey keyOf(TestWard& ward, Device& reader)
{
    return ward.serverCopy(reader).value().dataKey;
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

void DocumentSessionTest::aCreatedWardReopensSealedToThisDeviceWithTheSameContent()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ServerTestSupport::newDocumentId();

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
    QVERIFY(reopened->document().sealingSet()
            == SealingSet::of(alice.identity.member, alice.identity.displayName, alice.identity.device()));
    QCOMPARE(reopened->header().recipients.size(), qsizetype(1));
    QVERIFY(reopened->header().recipients.first().fingerprint == alice.identity.fingerprint());
}

void DocumentSessionTest::openReportsEachFailure()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    const QString documentId = ServerTestSupport::newDocumentId();
    const DocumentPaths paths = alice.paths(documentId);
    QVERIFY(alice.create(documentId));
    const QByteArray good = bytesOf(paths.file);

    LoadFailure failure = LoadFailure::None;
    QVERIFY(!alice.open(ServerTestSupport::newDocumentId(), failure));
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

void DocumentSessionTest::aSharedWardWhoseLocalFileWasDeletedIsDownloadedThenOpens()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));

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
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));

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
    const QString documentId = ServerTestSupport::newDocumentId();
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
    const QString documentId = ServerTestSupport::newDocumentId();
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
    const QString documentId = ServerTestSupport::newDocumentId();
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
    const QString documentId = ServerTestSupport::newDocumentId();
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
    QCOMPARE(recorder.replaced, 0);
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
        QCOMPARE(DocumentCrypto::payloadHash(base.value().sealed.ciphertext),
                 device->records.record(ward.documentId).baseHash);
        QCOMPARE(base.value().sealed.header.saveCounter, device->records.record(ward.documentId).baseSaveCounter);
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
    OtherDocument,
    DisagreesWithItsSet,
    NotDescended
};

void DocumentSessionTest::aServerVersionFailingValidationTouchesNothing_data()
{
    QTest::addColumn<int>("sabotage");
    QTest::newRow("another document") << int(Sabotage::OtherDocument);
    QTest::newRow("header disagrees with its sealing set") << int(Sabotage::DisagreesWithItsSet);
    QTest::newRow("a ward of the server's own making") << int(Sabotage::NotDescended);
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

    // What the server serves Bob instead of Alice's version, each built as a server
    // could build it: from public keys and a key of its own.
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    const DeviceIdentity stranger = newIdentity("Stranger", "Laptop");
    Document forgedDocument = copy.document;
    DocumentHeader previous = copy.sealed.header;
    DataKey key = DocumentCrypto::generateDataKey();
    SealingSet set;
    switch (Sabotage(sabotage))
    {
    case Sabotage::OtherDocument:
        // Alice's own ward under another id: sealed to Bob, his key, but not this ward.
        previous.documentId = ServerTestSupport::newDocumentId();
        key = copy.dataKey;
        set = copy.document.sealingSet();
        break;
    case Sabotage::DisagreesWithItsSet:
        key = copy.dataKey;
        set = copy.document.sealingSet();
        break;
    case Sabotage::NotDescended:
        // Everything checks but descent: sealed to Bob and to the server's own device.
        set = rosterOf({bob.identity, stranger});
        break;
    }
    forgedDocument.setSealingSet(set);
    DocumentFile::Sealed forged = DocumentFile::seal(forgedDocument, previous, key).value();
    if (Sabotage(sabotage) == Sabotage::DisagreesWithItsSet)
    {
        // A header naming a device its own payload does not seal to.
        DocumentFile::Sealed wider = DocumentFile::seal(forgedDocument, previous, key).value();
        wider.header.recipients.append(
            RecipientEntry{stranger.fingerprint(), DocumentCrypto::sealDataKey(key, stranger.sealing.publicKey)});
        const QByteArray plaintext = QJsonDocument(forgedDocument.toJson()).toJson(QJsonDocument::Compact);
        wider.ciphertext = DocumentCrypto::encryptPayload(plaintext, key, wider.header);
        forged = wider;
    }
    ward.server.serveInstead(ward.documentId, forged.header.toJson(), forged.ciphertext);

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
    QVERIFY(!bobSession->faultReason().isEmpty());
    QCOMPARE(bytesOf(paths.file), file);
    QCOMPARE(bytesOf(paths.base), base);
    QVERIFY(bob.records.record(ward.documentId) == record);
    QCOMPARE(recorder.replaced, 0);
    QVERIFY(!recorder.lost.has_value());
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 40.0);
}

// --- The refused-publish route ---------------------------------------------------

void DocumentSessionTest::aRemovalRotatesTheKeyPastWhoeverWasRemoved()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    QCOMPARE(headVersion(ward), 5);
    const DataKey before = keyOf(ward, alice);
    QVERIFY(canUnseal(ward.serverCopy(alice)->sealed.header, carol.identity));

    QVERIFY(editRosterOn(ward, *aliceSession, removePerson(carol.identity.member)));

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(6));
    QCOMPARE(copy.published.device, alice.identity.fingerprint());
    QVERIFY(!copy.document.sealingSet().person(carol.identity.member).has_value());
    QVERIFY(!copy.document.sealingSet().contains(carol.identity.fingerprint()));
    QVERIFY(!canUnseal(copy.sealed.header, carol.identity));
    QVERIFY(canUnseal(copy.sealed.header, alice.identity));
    QVERIFY(canUnseal(copy.sealed.header, bob.identity));
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity, &bob.identity}));
    QVERIFY(copy.dataKey != before);
    QVERIFY(copy.document.sealingSet().descendsFrom(before));

    // Bob takes the rotated version in: it descends from the key he holds.
    Recorder recorder;
    recorder.watch(*bobSession);
    bobSession->checkServer();
    ward.settle();
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
    QVERIFY(bobSession->document().sealingSet() == copy.document.sealingSet());
    QCOMPARE(recorder.rosterChanges, 1);
    QCOMPARE(recorder.replaced, 0);
}

void DocumentSessionTest::aPublishThatPrunesNobodyKeepsTheKey()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    const DataKey before = keyOf(ward, alice);

    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 56.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(4));
    QVERIFY(latitudeOf(copy.document, ward.adams) == 56.0);
    QVERIFY(copy.dataKey == before);
    QCOMPARE(copy.document.sealingSet().recipients().size(), 2);

    // An invite adds a recipient and keeps the key too.
    QVERIFY(ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Carol").has_value());
    QCOMPARE(keyOf(ward, alice), before);
    QCOMPARE(recipientsOnServer(ward).size(), 3);
}

void DocumentSessionTest::aRefusalAfterSomeoneLeftTakesTheirRosterAndPublishes()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    std::unique_ptr<DocumentSession> carolSession = carol.open(ward.documentId);
    QVERIFY(carolSession);
    bobSession->checkServer();
    ward.settle();
    QVERIFY(bobSession->document().sealingSet().contains(carol.identity.fingerprint()));

    // Carol leaves while Bob's last head still lists her; his upload is refused, and
    // the merge takes the roster without her.
    carolSession->leave();
    ward.settle();
    QVERIFY(!carol.records.contains(ward.documentId));
    const qint64 version = headVersion(ward);
    QCOMPARE(version, 6);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, version + 1);
    QCOMPARE(copy.published.device, bob.identity.fingerprint());
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::OK);
    QVERIFY(!canUnseal(copy.sealed.header, carol.identity));
    QVERIFY(!copy.document.sealingSet().person(carol.identity.member).has_value());
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
    QVERIFY(!bob.timers->isRunning(Timer::Retry));
}

void DocumentSessionTest::aRefusedPublishAdoptsTheirKeyAndSealingSet()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    bobSession->checkServer();
    ward.settle();

    // Alice removes Carol and moves Adams in one rotated version.
    aliceSession->checkServer();
    ward.settle();
    aliceSession->edit(DocumentSession::Edit::Roster, removePerson(carol.identity.member));
    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 58.0));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    const DataKey rotated = keyOf(ward, alice);
    QVERIFY(!canUnseal(ward.serverCopy(alice)->sealed.header, carol.identity));

    Recorder recorder;
    recorder.watch(*bobSession);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    QVERIFY(!bobSession->document().sealingSet().contains(carol.identity.fingerprint()));
    QVERIFY(!bobSession->header().entryFor(carol.identity.fingerprint()).has_value());

    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QCOMPARE(statusOf(copy.document, ward.adams), ContactStatus::OK);
    QVERIFY(latitudeOf(copy.document, ward.adams) == 58.0);
    QVERIFY(copy.dataKey == rotated);
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
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
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
    QCOMPARE(carolRecorder.replaced, 2);

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
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));

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
    QVERIFY(copy.document.sealingSet().contains(carol.identity.fingerprint()));
    QVERIFY(canUnseal(copy.sealed.header, carol.identity));
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
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    bobSession->checkServer();
    ward.settle();
    QVERIFY(bobSession->document().sealingSet().contains(carol.identity.fingerprint()));
    const DataKey carolKnows = keyOf(ward, alice);

    QVERIFY(editRosterOn(ward, *aliceSession, removePerson(carol.identity.member)));
    QVERIFY(divergeOnAdams(ward, bob, aliceSession, bobSession));

    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();
    QCOMPARE(recorder.conflicts.size(), qsizetype(1));

    // An edit while the conflict waits, then Keep mine: still never under Carol's key.
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    QVERIFY(bobSession->flush(nullptr));
    QVERIFY(bobSession->resolve(DocumentSession::Resolution::KeepMine, nullptr));
    ward.settle();

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QVERIFY(latitudeOf(copy.document, ward.adams) == 50.0);
    QCOMPARE(statusOf(copy.document, ward.baker), ContactStatus::OK);
    QVERIFY(!copy.document.sealingSet().contains(carol.identity.fingerprint()));
    QVERIFY(!canUnseal(copy.sealed.header, carol.identity));
    QVERIFY(copy.dataKey != carolKnows);
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

// --- Lost access -----------------------------------------------------------------

void DocumentSessionTest::aRemovalMidSessionDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);
    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(QFile::exists(paths.file) && QFile::exists(paths.base));

    QVERIFY(editRosterOn(ward, *aliceSession, removePerson(bob.identity.member)));
    QVERIFY(!recorder.lost.has_value());
    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Removed);
    QCOMPARE(recorder.lost->byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!bob.records.contains(ward.documentId));

    // Finished: nothing more is written.
    QCOMPARE(bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(bobSession->flush(nullptr));
    QVERIFY(!QFile::exists(paths.file));
}

void DocumentSessionTest::aRemovalOnOpenDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    bobSession.reset();

    QVERIFY(editRosterOn(ward, *aliceSession, removePerson(bob.identity.member)));
    bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    Recorder recorder;
    recorder.watch(*bobSession);
    ward.settle();

    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Removed);
    QCOMPARE(recorder.lost->byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
}

void DocumentSessionTest::aRemovalOnAPublishDeletesEverything()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);

    QVERIFY(editRosterOn(ward, *aliceSession, removePerson(bob.identity.member)));
    const qint64 version = headVersion(ward);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    // Refused as no key holder, and the head that follows says who.
    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Removed);
    QCOMPARE(recorder.lost->byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
    QCOMPARE(headVersion(ward), version);
    QCOMPARE(statusOf(ward.serverCopy(alice)->document, ward.adams), ContactStatus::NotContacted);
}

void DocumentSessionTest::aLeaveReportsLeftOnceTheServerHasIt()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    Recorder recorder;
    recorder.watch(*bobSession);
    const DocumentPaths paths = bob.paths(ward.documentId);
    const QByteArray file = bytesOf(paths.file);

    // Offline, the leave is refused and the ward stays, its file untouched.
    ward.server.setOffline(true);
    bobSession->leave();
    ward.settle();
    QCOMPARE(recorder.leaveFailures, QList<ServerContract::ServerError>{ServerContract::ServerError::Unreachable});
    QVERIFY(!recorder.lost.has_value());
    QCOMPARE(bytesOf(paths.file), file);

    ward.server.setOffline(false);
    bobSession->leave();
    ward.settle();
    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Left);
    QVERIFY(recorder.lost->byWhom.isEmpty());
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!bob.records.contains(ward.documentId));

    // The server holds the version without him, published by him, under a fresh key.
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(4));
    QCOMPARE(copy.published.device, bob.identity.fingerprint());
    QVERIFY(!copy.document.sealingSet().person(bob.identity.member).has_value());
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity}));
    QVERIFY(!canUnseal(copy.sealed.header, bob.identity));

    // Alice, now alone, cannot leave: a ward with nobody to read it is not a state.
    Recorder aliceRecorder;
    aliceRecorder.watch(*aliceSession);
    aliceSession->checkServer();
    ward.settle();
    aliceSession->leave();
    ward.settle();
    QCOMPARE(aliceRecorder.leaveFailures.size(), 1);
    QVERIFY(!aliceRecorder.lost.has_value());
    QCOMPARE(headVersion(ward), 4);
}

void DocumentSessionTest::aLeaveFromThisPersonsOtherDeviceReportsLeft()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Laptop"));
    Device& bobsPhone = ward.add(newDeviceOf(bob.identity, "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addDevice(*bobSession, bobsPhone));
    std::unique_ptr<DocumentSession> phoneSession = bobsPhone.open(ward.documentId);
    QVERIFY(phoneSession);
    ward.settle();
    bobSession->checkServer();
    ward.settle();
    QCOMPARE(bobSession->document().sealingSet().person(bob.identity.member)->devices.size(), 2);
    Recorder recorder;
    recorder.watch(*bobSession);

    // Bob leaves from his phone.
    phoneSession->leave();
    ward.settle();
    QVERIFY(!recorder.lost.has_value());
    QVERIFY(bob.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Left);
    QVERIFY(recorder.lost->byWhom.isEmpty());
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity}));
}

void DocumentSessionTest::aDeviceTakenOutByItsOwnPersonReportsUnlinked()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Laptop"));
    Device& bobsPhone = ward.add(newDeviceOf(bob.identity, "Phone"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addDevice(*bobSession, bobsPhone));
    std::unique_ptr<DocumentSession> phoneSession = bobsPhone.open(ward.documentId);
    QVERIFY(phoneSession);
    ward.settle();
    Recorder recorder;
    recorder.watch(*phoneSession);

    // Bob's laptop takes the phone out.
    QVERIFY(editRosterOn(ward, *bobSession, removeDevice(bobsPhone.identity.fingerprint())));
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity, &bob.identity}));
    QVERIFY(!canUnseal(ward.serverCopy(alice)->sealed.header, bobsPhone.identity));
    QVERIFY(bobsPhone.timers->fire(Timer::HeadCheck));
    ward.settle();

    QVERIFY(recorder.lost.has_value());
    QCOMPARE(recorder.lost->how, LostAccess::How::Unlinked);
    QCOMPARE(recorder.lost->byWhom, QString("Laptop"));
    QVERIFY(!QFile::exists(bobsPhone.paths(ward.documentId).file));

    // Bob himself keeps the ward.
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
    QVERIFY(QFile::exists(bob.paths(ward.documentId).file));
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
    QVERIFY(ServerTestSupport::wait<Done>(
                std::bind(&ServerApi::advanceClock, &ward.server, FakeServer::Limits().leaseSeconds + 1, _1))
                .ok());

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
    QVERIFY(leaseHolder(ward) == alice.identity.fingerprint());

    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 62.0));
    session.reset();
    ward.settle();

    QVERIFY(!leaseHolder(ward).has_value());
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
    QVERIFY(!leaseHolder(ward).has_value());

    // Opened, its lease request granted at the server, and destroyed before the
    // answer arrives: the release follows the request there.
    ward.server.holdReplies(true);
    session = alice.open(ward.documentId);
    QVERIFY(session);
    QVERIFY(ward.server.waitingReplies() > 0);
    session.reset();
    ward.server.holdReplies(false);
    ward.settle();

    QVERIFY(!leaseHolder(ward).has_value());
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
    QJsonObject laterFormat = later.sealed.header.toJson();
    laterFormat["formatVersion"] = DocumentHeader::CurrentFormatVersion + 1;
    ward.server.serveInstead(ward.documentId, laterFormat, later.sealed.ciphertext);

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
    QCOMPARE(headVersion(ward), later.version);

    // The server moves on to a version this build reads: the update clears and
    // the saved record publishes.
    ward.server.serveInstead(ward.documentId, QJsonObject(), QByteArray());
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
    const QString documentId = ServerTestSupport::newDocumentId();
    const DocumentPaths paths = alice.paths(documentId);
    std::unique_ptr<DocumentSession> session = alice.create(documentId);
    QVERIFY(session);
    QVERIFY(!session->isShared());
    QVERIFY(!QFile::exists(paths.base));

    session->share();
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

// --- Invites and joins ------------------------------------------------------------

void DocumentSessionTest::anInviteAddsARecipientAndARosterEntry()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const DataKey before = keyOf(ward, alice);

    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Admin, "for Bob");
    QVERIFY(link.has_value());
    QCOMPARE(link->documentId, ward.documentId);
    QCOMPARE(link->seed.size(), qsizetype(FileOpenRouter::InviteLink::SeedSize));

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, qint64(2));
    QCOMPARE(copy.sealed.header.recipients.size(), qsizetype(2));
    QVERIFY(copy.dataKey == before);
    const QList<Invite> invites = copy.document.sealingSet().invites();
    QCOMPARE(invites.size(), qsizetype(1));
    QCOMPARE(invites.first().label, QString("for Bob"));
    QVERIFY(invites.first().purpose == Invite::For::User);
    QVERIFY(invites.first().role == Role::Admin);
    QVERIFY(invites.first().person == alice.identity.member);
    QVERIFY(aliceSession->document().sealingSet() == copy.document.sealingSet());

    // An unshared ward makes no invite.
    std::unique_ptr<DocumentSession> local = alice.create(ServerTestSupport::newDocumentId());
    QVERIFY(local);
    QVERIFY(!local->invite(Invite::For::User, Role::Member, "for Carol").has_value());
    QVERIFY(local->document().sealingSet().invites().isEmpty());
}

void DocumentSessionTest::aUserJoinsAndShowsInTheInvitersRosterAtTheNextHead()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    Recorder recorder;
    recorder.watch(*aliceSession);
    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(link.has_value());
    const DataKey invited = keyOf(ward, alice);
    const Fingerprint invite = aliceSession->document().sealingSet().invites().first().fingerprint();
    QVERIFY(!bob.open(ward.documentId));

    std::unique_ptr<DocumentSession> bobSession = joinAndOpen(ward, bob, link.value());
    QVERIFY(bobSession);
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
    QVERIFY(latitudeOf(bobSession->document(), ward.adams) == 40.0);
    QCOMPARE(bob.records.record(ward.documentId).baseVersion, std::optional<qint64>(3));

    // The joined version: Bob in the invite's place, published as the invite, under a
    // fresh key, so the used link opens nothing written from now on.
    const ServerVersion::Fetched copy = ward.serverCopy(bob).value();
    QCOMPARE(copy.version, qint64(3));
    const SealingSet roster = copy.document.sealingSet();
    QCOMPARE(roster.people().size(), qsizetype(2));
    QCOMPARE(roster.person(bob.identity.member)->name, QString("Bob"));
    QVERIFY(roster.person(bob.identity.member)->role == Role::Member);
    QCOMPARE(roster.person(bob.identity.member)->devices.first().name, QString("Phone"));
    QVERIFY(roster.invites().isEmpty());
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity, &bob.identity}));
    QCOMPARE(copy.published.device, invite);
    QVERIFY(copy.dataKey != invited);
    QVERIFY(roster.descendsFrom(invited));

    // Alice sees him at her next head: her own invite was an edit, the join is news.
    QCOMPARE(recorder.rosterChanges, 0);
    aliceSession->checkServer();
    ward.settle();
    QCOMPARE(recorder.rosterChanges, 1);
    QCOMPARE(recorder.replaced, 0);
    QVERIFY(aliceSession->document().sealingSet() == roster);
}

void DocumentSessionTest::aDeviceJoinsAsAnotherDeviceOfItsPerson()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*session, Invite::For::Device, Role::Admin, "for Phone");
    QVERIFY(link.has_value());

    std::unique_ptr<DocumentSession> phoneSession = joinAndOpen(ward, phone, link.value());
    QVERIFY(phoneSession);
    QCOMPARE(phoneSession->syncState(), SyncState::UpToDate);

    session->checkServer();
    ward.settle();
    const SealingSet roster = session->document().sealingSet();
    QCOMPARE(roster.people().size(), qsizetype(1));
    const Holder alice = roster.person(laptop.identity.member).value();
    QCOMPARE(alice.name, QString("Alice"));
    QVERIFY(alice.role == Role::Admin);
    QCOMPARE(alice.devices.size(), qsizetype(2));
    QCOMPARE(roster.device(phone.identity.fingerprint())->name, QString("Phone"));
    QCOMPARE(recipientsOnServer(ward), hexOf({&laptop.identity, &phone.identity}));
    QVERIFY(roster == phoneSession->document().sealingSet());
}

void DocumentSessionTest::aUsedLinkGivesInviteUsedAndWritesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& mallory = ward.add(newIdentity("Mallory", "Laptop"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(link.has_value());
    QCOMPARE(ward.join(bob, link.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
    const qint64 version = headVersion(ward);
    QCOMPARE(version, 3);

    QCOMPARE(ward.join(mallory, link.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::InviteUsed));
    QCOMPARE(headVersion(ward), version);
    QVERIFY(!QFile::exists(mallory.paths(ward.documentId).file));
    QVERIFY(!QFile::exists(mallory.paths(ward.documentId).base));
    QVERIFY(!mallory.records.contains(ward.documentId));

    // A withdrawn invite is used up the same way.
    aliceSession->checkServer();
    ward.settle();
    const std::optional<FileOpenRouter::InviteLink> withdrawn =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Carol");
    QVERIFY(withdrawn.has_value());
    const Fingerprint open = aliceSession->document().sealingSet().invites().first().fingerprint();
    QVERIFY(editRosterOn(ward, *aliceSession, removeInvite(open)));
    QVERIFY(aliceSession->document().sealingSet().invites().isEmpty());
    QCOMPARE(ward.join(mallory, withdrawn.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::InviteUsed));
    QVERIFY(!mallory.records.contains(ward.documentId));
}

void DocumentSessionTest::aJoinWhoseAnswerWasLostFinishesWhenTheLinkIsOpenedAgain()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(link.has_value());

    // The joined version lands, but its answer and everything after it are lost.
    QObject receiver;
    std::optional<DocumentSession::Join> joined;
    ward.server.holdReplies(true);
    DocumentSession::join(bob.context(),
                          link.value(),
                          bob.paths(ward.documentId),
                          &receiver,
                          [&joined](DocumentSession::Join outcome, const QString&) { joined = outcome; });
    ward.server.loseReplies(true);
    QVERIFY(ward.server.deliverNext());
    ward.server.holdReplies(false);
    ward.settle();
    ward.server.loseReplies(false);
    QCOMPARE(joined, std::optional<DocumentSession::Join>(DocumentSession::Join::Unreachable));
    QCOMPARE(headVersion(ward), 3);
    QVERIFY(!bob.records.contains(ward.documentId));
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));

    // The same link again: the invite is spent, but the current version is Bob's.
    QCOMPARE(ward.join(bob, link.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
    QCOMPARE(headVersion(ward), 3);
    QCOMPARE(bob.records.record(ward.documentId).baseVersion, std::optional<qint64>(3));
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();
    QCOMPARE(bobSession->syncState(), SyncState::UpToDate);
    QCOMPARE(bobSession->document().sealingSet().person(bob.identity.member)->name, QString("Bob"));
}

void DocumentSessionTest::aJoinSaysWhyItCouldNotReachTheWard()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const std::optional<FileOpenRouter::InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(link.has_value());

    ward.server.setOffline(true);
    QCOMPARE(ward.join(bob, link.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::Unreachable));
    ward.server.setOffline(false);

    const FileOpenRouter::InviteLink nowhere{ServerTestSupport::newDocumentId(), link->seed};
    QCOMPARE(ward.join(bob, nowhere), std::optional<DocumentSession::Join>(DocumentSession::Join::UnknownWard));
    QVERIFY(!bob.records.contains(ward.documentId));
    QVERIFY(!bob.records.contains(nowhere.documentId));

    // Anchor: the same link, reachable, joins.
    QCOMPARE(ward.join(bob, link.value()), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
}

// --- The roster across a refused publish -------------------------------------------

void DocumentSessionTest::aRosterEditCarriesThroughARefusedPublish()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    aliceSession->checkServer();
    bobSession->checkServer();
    ward.settle();
    const DataKey carolKnows = keyOf(ward, alice);

    // Alice removes Carol offline; Bob records a contact and publishes first.
    ward.server.setOffline(true);
    aliceSession->edit(DocumentSession::Edit::Roster, removePerson(carol.identity.member));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    ward.server.setOffline(false);
    bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.baker, ContactStatus::OK));
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();
    const qint64 bobs = headVersion(ward);
    QCOMPARE(bobs, 6);

    // Alice's publish is refused, merges without asking, and goes up without Carol.
    Recorder recorder;
    recorder.watch(*aliceSession);
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();

    QVERIFY(recorder.conflicts.isEmpty());
    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QCOMPARE(copy.version, bobs + 1);
    QCOMPARE(copy.published.device, alice.identity.fingerprint());
    QCOMPARE(statusOf(copy.document, ward.baker), ContactStatus::OK);
    QVERIFY(!copy.document.sealingSet().person(carol.identity.member).has_value());
    QVERIFY(!canUnseal(copy.sealed.header, carol.identity));
    QVERIFY(copy.dataKey != carolKnows);
    QCOMPARE(recipientsOnServer(ward), hexOf({&alice.identity, &bob.identity}));
    QCOMPARE(aliceSession->syncState(), SyncState::UpToDate);
}

void DocumentSessionTest::theSamePersonChangedOnBothSidesMergesWithoutAsking()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> aliceSession;
    std::unique_ptr<DocumentSession> bobSession;
    QVERIFY(openAliceAndBob(ward, alice, bob, aliceSession, bobSession));
    QVERIFY(ward.addUser(*aliceSession, carol, Role::Member));
    aliceSession->checkServer();
    bobSession->checkServer();
    ward.settle();

    // Alice makes Carol an admin offline; Bob removes Carol and publishes first: the
    // same person, changed differently on each side, and the server's version stands.
    ward.server.setOffline(true);
    aliceSession->edit(DocumentSession::Edit::Roster, setRole(carol.identity.member, Role::Admin));
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    ward.server.setOffline(false);
    QVERIFY(editRosterOn(ward, *bobSession, removePerson(carol.identity.member)));
    const qint64 bobs = headVersion(ward);

    Recorder recorder;
    recorder.watch(*aliceSession);
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();

    // Bob's version, taken in as it stands: nothing of Alice's is left to publish.
    QVERIFY(recorder.conflicts.isEmpty());
    QCOMPARE(recorder.rosterChanges, 1);
    QCOMPARE(headVersion(ward), bobs);
    QVERIFY(aliceSession->document().sealingSet() == bobSession->document().sealingSet());
    QVERIFY(!aliceSession->document().sealingSet().person(carol.identity.member).has_value());
    QVERIFY(!canUnseal(aliceSession->header(), carol.identity));
    QCOMPARE(aliceSession->syncState(), SyncState::UpToDate);
}
