#include "DocumentManagerTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FamilyCommands.h"
#include "IncomingWards.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace SessionTestSupport;
using Timer = SessionTimers::Timer;

const std::optional<DocumentSession::Download> Downloaded = DocumentSession::Download::Done;

namespace
{

qint64 counterOnDisk(const QString& path)
{
    const std::optional<DocumentHeader> header = headerOnDisk(path);
    return header.has_value() ? header.value().saveCounter : -1;
}

/// A command that moves a family, as the edit panel makes one.
CommandPtr moveCommand(const Document& document, const FamilyId& family, double latitude)
{
    const Family before = document.findFamilyById(family).value();
    Family after = before;
    after.setLocation(latitude, -111.0);
    return std::make_unique<UpdateFamilyCommand>(before, after);
}

/// Someone else takes the lease, and the session learns of it.
void loseTheLease(TestWard& ward, DocumentSession& session)
{
    ward.server.holdLeaseAs(ward.documentId, newIdentity("Someone", "Laptop").fingerprint());
    session.checkServer();
    ward.settle();
}

/// A family with a street address and no coordinates: one geocoding would look up.
void addUnmappedFamily(Document& document, const FamilyId& family)
{
    Address address;
    address.addLine("12 Elm Street");
    address.addLine("Provo, UT 84604");
    document.addFamily(Family::createWithId(family, std::nullopt, std::nullopt, address, {}));
}

/// One device's app, as main.cpp builds it: settings and a library in a folder of
/// their own, the device's connection, and the manager over them.
struct App
{
    explicit App(Device& device)
        : settings(AppSettings::open(folder.filePath("settings.json"), nullptr))
        , library(*settings, folder.filePath("wards"), device.identity)
        , manager(device.identity, *settings, device.connection, library, nullptr)
    {
    }

    QTemporaryDir folder;
    std::unique_ptr<AppSettings> settings;
    DocumentLibrary library;
    DocumentManager manager;
    EmergencyManager emergency{nullptr};
};

ServerContract::Session sessionOf(TestWard& ward, Device& device)
{
    return ward.sessionFor(device).value();
}

void keepRequests(QList<ServerContract::AccessRequest>& out, const QList<ServerContract::AccessRequest>& requests)
{
    out = requests;
}

/// Alice's app with a shared ward of her making, and the ward's id on the test ward.
bool shareFrom(TestWard& ward, App& app)
{
    if (!app.manager.newDocument(nullptr))
    {
        return false;
    }
    app.manager.share();
    ward.settle();
    ward.documentId = app.manager.documentId();
    return app.manager.session()->isShared();
}

/// Bob asks for Alice's ward; Alice's app reads the request.
std::optional<ServerContract::AccessRequest> askAndRead(TestWard& ward, App& app, Device& bob)
{
    if (!ServerTestSupport::wait<ServerContract::Done>(std::bind(&ServerContract::ServerApi::requestAccess,
                                                                 &ward.server,
                                                                 sessionOf(ward, bob),
                                                                 ward.documentId,
                                                                 std::placeholders::_1))
             .ok())
    {
        return std::nullopt;
    }
    QObject receiver;
    QList<ServerContract::AccessRequest> requests;
    app.manager.fetchRequests(&receiver, std::bind(&keepRequests, std::ref(requests), std::placeholders::_1));
    ward.settle();
    if (requests.size() != 1)
    {
        return std::nullopt;
    }
    return requests.first();
}

QList<ServerContract::Member> membersAt(TestWard& ward, Device& reader)
{
    return ServerTestSupport::wait<ServerContract::Head>(std::bind(&ServerContract::ServerApi::head,
                                                                   &ward.server,
                                                                   sessionOf(ward, reader),
                                                                   ward.documentId,
                                                                   std::placeholders::_1))
        .value.value_or(ServerContract::Head())
        .members;
}

std::optional<ServerContract::Member> memberIn(const QList<ServerContract::Member>& members, const MemberId& id)
{
    for (const ServerContract::Member& member : members)
    {
        if (member.person.member == id)
        {
            return member;
        }
    }
    return std::nullopt;
}

ServerContract::RequestStatus statusOf(TestWard& ward, Device& asker)
{
    return ServerTestSupport::wait<ServerContract::RequestStatus>(
               std::bind(&ServerContract::ServerApi::myRequestStatus,
                         &ward.server,
                         sessionOf(ward, asker),
                         ward.documentId,
                         std::placeholders::_1))
        .value.value();
}

/// Every head and download of the ward serves its stored header with a blob no
/// device can open, as a broken server would; uploads still build on the stored
/// version.
void serveUnreadable(TestWard& ward, Device& reader)
{
    const ServerContract::Result<ServerContract::Snapshot> stored =
        ServerTestSupport::wait<ServerContract::Snapshot>(std::bind(&ServerContract::ServerApi::download,
                                                                    &ward.server,
                                                                    sessionOf(ward, reader),
                                                                    ward.documentId,
                                                                    std::placeholders::_1));
    ward.server.serveInstead(ward.documentId, stored.value->header, "garbage");
}

} // namespace

void DocumentManagerTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentManagerTest::aForeignLeaseRefusesCommandsUndoAndRedoButNotResponses()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    DocumentSession* open = session.get();
    DocumentManager manager(alice.identity, std::move(session), nullptr);
    EmergencyManager emergency(nullptr);

    // Two moves and one undo, while Alice holds the lease: one to undo, one to redo.
    manager.executeCommand(moveCommand(manager.document(), ward.adams, 50.0));
    manager.executeCommand(moveCommand(manager.document(), ward.adams, 51.0));
    manager.undo();
    QVERIFY(latitudeOf(manager.document(), ward.adams) == 50.0);
    QVERIFY(manager.canUndo());
    QVERIFY(manager.canRedo());
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    loseTheLease(ward, *open);
    QVERIFY(!manager.mayWriteDirectory());

    const QString file = alice.paths(ward.documentId).file;
    const qint64 before = counterOnDisk(file);
    const Document content = manager.document();
    const QString undoText = manager.undoDescription();
    const QString redoText = manager.redoDescription();
    QSignalSpy refused(&manager, &DocumentManager::editRefused);

    manager.executeCommand(moveCommand(manager.document(), ward.adams, 52.0));
    manager.undo();
    manager.redo();

    QCOMPARE(refused.count(), 3);
    QCOMPARE(refused.at(0).at(0).toString(), QString("Another member is editing the directory."));
    QVERIFY(manager.document() == content);
    QVERIFY(manager.canUndo());
    QVERIFY(manager.canRedo());
    QCOMPARE(manager.undoDescription(), undoText);
    QCOMPARE(manager.redoDescription(), redoText);
    QVERIFY(!alice.timers->isRunning(Timer::Save));

    // A response change is any member's to make, and it saves.
    emergency.setContactStatus(ward.baker, ContactStatus::OK);
    QCOMPARE(statusOf(manager.document(), ward.baker), ContactStatus::OK);
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    QCOMPARE(counterOnDisk(file), before + 1);
    QCOMPARE(refused.count(), 3);
}

void DocumentManagerTest::aFastForwardClearsTheUndoStack()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession->checkServer();
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();

    DocumentManager manager(alice.identity, std::move(aliceSession), nullptr);
    manager.executeCommand(moveCommand(manager.document(), ward.adams, 50.0));
    QVERIFY(manager.canUndo());
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();

    // Bob records a contact and publishes; Alice, with nothing unpublished, takes it.
    QSignalSpy changed(&manager, &DocumentManager::documentChanged);
    QCOMPARE(bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();
    manager.session()->checkServer();
    ward.settle();

    QCOMPARE(statusOf(manager.document(), ward.adams), ContactStatus::OK);
    QVERIFY(!manager.canUndo());
    QVERIFY(!manager.canRedo());
    QVERIFY(changed.count() >= 1);
    QCOMPARE(changed.last().at(0).value<DocumentChange>().action, ChangeAction::Full);
}

void DocumentManagerTest::aContactChangeAfterAMergeKeepsTheOtherSidesRecords()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession->checkServer();
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();

    DocumentManager manager(alice.identity, std::move(aliceSession), nullptr);
    EmergencyManager emergency(nullptr);
    QVERIFY(emergency.isActive());

    // Alice records Baker while the server is out of reach, so hers is unpublished.
    ward.server.setOffline(true);
    emergency.setContactStatus(ward.baker, ContactStatus::OK);
    QVERIFY(alice.timers->fire(Timer::Save));
    ward.settle();
    ward.server.setOffline(false);

    // Bob records Adams and publishes first.
    QCOMPARE(bobSession->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::UnableToReach)),
             DocumentSession::Refusal::None);
    QVERIFY(bob.timers->fire(Timer::Save));
    ward.settle();

    // Alice's publish is refused and merges.
    QVERIFY(alice.timers->fire(Timer::Retry));
    ward.settle();
    QCOMPARE(statusOf(manager.document(), ward.adams), ContactStatus::UnableToReach);
    QCOMPARE(statusOf(manager.document(), ward.baker), ContactStatus::OK);

    // A change made now builds on the merged response, not one held from before.
    emergency.setContactStatus(ward.baker, ContactStatus::NotContacted);
    QCOMPARE(statusOf(manager.document(), ward.adams), ContactStatus::UnableToReach);
    QCOMPARE(statusOf(manager.document(), ward.baker), ContactStatus::NotContacted);
}

void DocumentManagerTest::batchGeocodingDoesNotStartWithoutTheLease()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    const FamilyId unmapped = FamilyId::generate();
    QCOMPARE(session->edit(DocumentSession::Edit::Directory,
                           [&](Document& document) { addUnmappedFamily(document, unmapped); }),
             DocumentSession::Refusal::None);
    DocumentSession* open = session.get();
    loseTheLease(ward, *open);

    DocumentManager manager(alice.identity, std::move(session), nullptr);
    QVERIFY(!manager.mayWriteDirectory());

    // Anchor: a family geocoding would look up.
    const Family family = manager.document().findFamilyById(unmapped).value();
    QVERIFY(!family.isMapped());
    QVERIFY(!family.address().noStreetAddress());

    QSignalSpy progress(&manager, &DocumentManager::geocodingProgressChanged);
    manager.startBatchGeocoding();
    QVERIFY(!manager.isGeocoding());
    QCOMPARE(progress.count(), 0);
}

void DocumentManagerTest::aGeocodingResultAfterTheLeaseIsLostChangesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    DocumentSession* open = session.get();
    DocumentManager manager(alice.identity, std::move(session), nullptr);

    // Anchor: with the lease, a result moves the family.
    QVERIFY(QMetaObject::invokeMethod(&manager,
                                      "onFamilyGeocoded",
                                      Qt::DirectConnection,
                                      Q_ARG(FamilyId, ward.adams),
                                      Q_ARG(double, 45.0),
                                      Q_ARG(double, -112.0)));
    QVERIFY(latitudeOf(manager.document(), ward.adams) == 45.0);

    loseTheLease(ward, *open);
    const Document content = manager.document();
    QVERIFY(QMetaObject::invokeMethod(&manager,
                                      "onFamilyGeocoded",
                                      Qt::DirectConnection,
                                      Q_ARG(FamilyId, ward.baker),
                                      Q_ARG(double, 46.0),
                                      Q_ARG(double, -112.0)));
    QVERIFY(manager.document() == content);
}

void DocumentManagerTest::aConflictWaitsUntilItIsSettled()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession->checkServer();
    ward.settle();

    // Bob moves Adams while out of reach; Alice moves Adams and publishes.
    ward.server.setOffline(true);
    std::unique_ptr<DocumentSession> offline = bob.open(ward.documentId);
    ward.settle();
    QVERIFY(offline);
    QCOMPARE(offline->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 50.0)), DocumentSession::Refusal::None);
    offline.reset();
    ward.server.setOffline(false);
    aliceSession->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 60.0));
    aliceSession->publishNow();
    ward.settle();

    DocumentManager manager(bob.identity, bob.open(ward.documentId), nullptr);
    QSignalSpy raised(&manager, &DocumentManager::conflictRaised);
    QSignalSpy settled(&manager, &DocumentManager::conflictSettled);
    ward.settle();

    QCOMPARE(raised.count(), 1);
    QVERIFY(manager.pendingConflict().has_value());
    QCOMPARE(manager.pendingConflict()->writerName, QString("Alice"));

    // Without a library there is nowhere to save a copy; the conflict still waits.
    QString error;
    QVERIFY(!manager.saveRecoveryCopy(nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(manager.pendingConflict().has_value());
    QCOMPARE(settled.count(), 0);

    QVERIFY(manager.resolveConflict(DocumentSession::Resolution::KeepTheirs, &error));
    QVERIFY(!manager.pendingConflict().has_value());
    QCOMPARE(settled.count(), 1);
    QVERIFY(latitudeOf(manager.document(), ward.adams) == 60.0);
}

void DocumentManagerTest::accessChangesShowOnceAndStaySeenOnceDismissed()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(alice);
    QSignalSpy changed(&app.manager, &DocumentManager::accessChanged);

    QVERIFY(app.manager.newDocument(nullptr));
    app.manager.share();
    ward.settle();
    ward.documentId = app.manager.documentId();
    QVERIFY(app.manager.session()->isShared());
    QVERIFY(app.manager.unseenHistory().isEmpty());

    // Alice grants Bob: the next head shows it, once.
    const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, bob);
    QVERIFY(request.has_value());
    app.manager.grant(request.value());
    ward.settle();
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.unseenHistory().size(), 1);
    QCOMPARE(app.manager.unseenHistory().first().kind, ServerContract::HistoryEntry::Kind::Added);
    QCOMPARE(app.manager.unseenHistory().first().actor, QString("Alice"));
    QCOMPARE(app.manager.unseenHistory().first().subject, QString("Bob"));
    QCOMPARE(changed.count(), 1);

    // A second head with nothing new fetches nothing and shows the same.
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.unseenHistory().size(), 1);
    QCOMPARE(changed.count(), 1);

    // Dismissed: seen on the record, so later heads show nothing.
    app.manager.dismissHistory();
    QVERIFY(app.manager.unseenHistory().isEmpty());
    QCOMPARE(app.settings->record(ward.documentId).historySeen, 1);
    app.manager.session()->checkServer();
    ward.settle();
    QVERIFY(app.manager.unseenHistory().isEmpty());

    // A later change shows alone.
    QVERIFY(ServerTestSupport::wait<ServerContract::Done>(std::bind(&ServerContract::ServerApi::setRole,
                                                                    &ward.server,
                                                                    sessionOf(ward, alice),
                                                                    ward.documentId,
                                                                    bob.identity.member,
                                                                    ServerContract::Role::Admin,
                                                                    std::placeholders::_1))
                .ok());
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.unseenHistory().size(), 1);
    QCOMPARE(app.manager.unseenHistory().first().kind, ServerContract::HistoryEntry::Kind::MadeAdmin);
    QCOMPARE(app.manager.unseenHistory().first().index, 2);
}

void DocumentManagerTest::aFetchThatFindsAccessLostSaysWhy()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Laptop"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));

    // Bob's app knows the ward but has no file of it, so opening it fetches.
    App app(bob);
    app.library.add(ward.documentId);
    DocumentRecord record;
    record.baseVersion = 1;
    app.settings->setRecord(ward.documentId, record);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    // Bob leaves from another of his devices' sessions, here his own on the server.
    QVERIFY(ServerTestSupport::wait<ServerContract::Done>(std::bind(&ServerContract::ServerApi::removeMember,
                                                                    &ward.server,
                                                                    sessionOf(ward, bob),
                                                                    ward.documentId,
                                                                    bob.identity.member,
                                                                    std::placeholders::_1))
                .ok());

    QCOMPARE(app.manager.openDocument(ward.documentId, nullptr), DocumentManager::Open::Fetching);
    ward.settle();

    QCOMPARE(lost.count(), 1);
    const ServerContract::LostAccess how = lost.first().at(1).value<ServerContract::LostAccess>();
    QCOMPARE(how.how, ServerContract::LostAccess::How::Left);
    QVERIFY(!app.library.contains(ward.documentId));
}

void DocumentManagerTest::shareAnnouncesTheWardAndOfflineSaysWhy()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    App app(alice);
    QSignalSpy shared(&app.manager, &DocumentManager::shared);
    QSignalSpy failed(&app.manager, &DocumentManager::shareFailed);

    QVERIFY(app.manager.newDocument(nullptr));
    ward.server.setOffline(true);
    app.manager.share();
    ward.settle();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().first().value<ServerContract::ServerError>(), ServerContract::ServerError::Unreachable);
    QVERIFY(!app.manager.session()->isShared());

    ward.server.setOffline(false);
    app.manager.share();
    ward.settle();
    QCOMPARE(shared.count(), 1);
    QCOMPARE(shared.first().first().toString(), app.manager.documentId());
    QVERIFY(app.manager.session()->isShared());
    QCOMPARE(app.settings->record(app.manager.documentId()).baseVersion, 1);
}

void DocumentManagerTest::aGrantSealsTheDeviceThatAskedAndADeclineSaysSo()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QSignalSpy done(&app.manager, &DocumentManager::accessCallDone);

    const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, bob);
    QVERIFY(request.has_value());
    QCOMPARE(request->person.displayName, QString("Bob"));
    app.manager.grant(request.value());
    ward.settle();

    QCOMPARE(done.count(), 1);
    QVERIFY(memberIn(membersAt(ward, alice), bob.identity.member).has_value());

    // The version Bob reads went up with the grant: version 2, on top of the share's
    // version 1. His device downloads and opens it with no sealing step between.
    QCOMPARE(ward.serverCopy(alice)->version, qint64(2));
    QCOMPARE(ward.download(bob, ward.documentId), Downloaded);
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    QVERIFY(bobSession->header().entryFor(bob.identity.fingerprint()).has_value());
    QCOMPARE(bob.records.record(ward.documentId).baseVersion, std::optional<qint64>(2));

    // Alice's open session took the grant's version in at the head check that
    // follows a grant: her own publish of the server's version, now sealed to Bob.
    QVERIFY(app.manager.session()->header().entryFor(bob.identity.fingerprint()).has_value());
    QCOMPARE(app.settings->record(ward.documentId).baseVersion, std::optional<qint64>(2));

    const std::optional<ServerContract::AccessRequest> carols = askAndRead(ward, app, carol);
    QVERIFY(carols.has_value());
    app.manager.decline(carols.value());
    ward.settle();
    QCOMPARE(done.count(), 2);
    QCOMPARE(ServerTestSupport::wait<ServerContract::RequestStatus>(std::bind(&ServerContract::ServerApi::myRequestStatus,
                                                                              &ward.server,
                                                                              sessionOf(ward, carol),
                                                                              ward.documentId,
                                                                              std::placeholders::_1))
                 .value,
             std::optional<ServerContract::RequestStatus>(ServerContract::RequestStatus::Declined));
}

void DocumentManagerTest::aGrantRetriesOnceOnAConflict()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QSignalSpy done(&app.manager, &DocumentManager::accessCallDone);
    QSignalSpy failed(&app.manager, &DocumentManager::accessCallFailed);
    const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, bob);
    QVERIFY(request.has_value());

    // The first grant is refused as a conflict; the second, built again, lands. The
    // forced conflict leaves the version where it was, so a retry that resent the
    // first resealing unchanged would pass here too; what is pinned is that one
    // refusal is absorbed.
    ward.server.forceConflict(ward.documentId);
    app.manager.grant(request.value());
    ward.settle();

    QCOMPARE(done.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(statusOf(ward, bob), ServerContract::RequestStatus::Granted);
    QCOMPARE(ward.serverCopy(alice)->version, qint64(2));
    QCOMPARE(ward.download(bob, ward.documentId), Downloaded);
    QVERIFY(bob.open(ward.documentId));
}

void DocumentManagerTest::aGrantOnAnUnreadableVersionIsBlocked()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QSignalSpy done(&app.manager, &DocumentManager::accessCallDone);
    QSignalSpy failed(&app.manager, &DocumentManager::accessCallFailed);
    QSignalSpy blocked(&app.manager, &DocumentManager::grantBlocked);
    const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, bob);
    QVERIFY(request.has_value());

    serveUnreadable(ward, alice);
    app.manager.grant(request.value());
    ward.settle();

    QCOMPARE(blocked.count(), 1);
    QCOMPARE(blocked.first().first().toString(), QString("The document could not be decrypted."));
    QCOMPARE(done.count(), 0);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(statusOf(ward, bob), ServerContract::RequestStatus::Pending);
    QVERIFY(!memberIn(membersAt(ward, alice), bob.identity.member).has_value());

    // Served as stored again, the same request is granted.
    ward.server.serveInstead(ward.documentId, QJsonObject(), QByteArray());
    app.manager.grant(request.value());
    ward.settle();
    QCOMPARE(done.count(), 1);
    QCOMPARE(blocked.count(), 1);
    QCOMPARE(statusOf(ward, bob), ServerContract::RequestStatus::Granted);
}

void DocumentManagerTest::anAdminRemovesAndChangesRoles()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QSignalSpy failed(&app.manager, &DocumentManager::accessCallFailed);
    for (Device* person : {&bob, &carol})
    {
        const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, *person);
        QVERIFY(request.has_value());
        app.manager.grant(request.value());
        ward.settle();
    }

    app.manager.setRole(bob.identity.member, ServerContract::Role::Admin);
    ward.settle();
    QCOMPARE(memberIn(membersAt(ward, alice), bob.identity.member)->role, ServerContract::Role::Admin);

    app.manager.remove(carol.identity.member);
    ward.settle();
    QVERIFY(!memberIn(membersAt(ward, alice), carol.identity.member).has_value());
    QCOMPARE(failed.count(), 0);

    // Alice steps down; then, a member, her removal is refused.
    app.manager.setRole(alice.identity.member, ServerContract::Role::Member);
    ward.settle();
    app.manager.remove(bob.identity.member);
    ward.settle();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().first().value<ServerContract::ServerError>(), ServerContract::ServerError::NotAdmin);
}

void DocumentManagerTest::theLastAdminLeavesThroughASuccessorAndTheWardGoes()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    const std::optional<ServerContract::AccessRequest> request = askAndRead(ward, app, bob);
    QVERIFY(request.has_value());
    app.manager.grant(request.value());
    ward.settle();
    const QString documentId = ward.documentId;
    const QString file = app.library.pathsFor(documentId).file;
    QVERIFY(QFile::exists(file));
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);
    QSignalSpy failed(&app.manager, &DocumentManager::accessCallFailed);

    // Without a successor the server refuses; with one, she leaves.
    app.manager.leave(std::nullopt);
    ward.settle();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().first().value<ServerContract::ServerError>(), ServerContract::ServerError::LastAdmin);
    QVERIFY(QFile::exists(file));

    app.manager.leave(bob.identity.member);
    ward.settle();
    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerContract::LostAccess>().how, ServerContract::LostAccess::How::Left);
    QVERIFY(!QFile::exists(file));
    QVERIFY(!app.library.contains(documentId));
    QVERIFY(app.manager.documentId().isEmpty());

    const QList<ServerContract::Member> members = membersAt(ward, bob);
    QCOMPARE(members.size(), 1);
    QCOMPARE(members.first().role, ServerContract::Role::Admin);
}

void DocumentManagerTest::aMemberLeavesAndTheWardGoes()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession.reset();

    App app(bob);
    app.library.add(ward.documentId);
    DocumentRecord record;
    record.baseVersion = 1;
    app.settings->setRecord(ward.documentId, record);
    QCOMPARE(app.manager.openDocument(ward.documentId, nullptr), DocumentManager::Open::Fetching);
    ward.settle();
    QCOMPARE(app.manager.documentId(), ward.documentId);
    const DocumentPaths paths = app.library.pathsFor(ward.documentId);
    QVERIFY(QFile::exists(paths.file));
    QVERIFY(QFile::exists(paths.base));
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    app.manager.leave(std::nullopt);
    ward.settle();
    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerContract::LostAccess>().how, ServerContract::LostAccess::How::Left);
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
    QVERIFY(!memberIn(membersAt(ward, alice), bob.identity.member).has_value());
}

namespace
{

/// Bob's ward, fetched into his app and opened, with his incoming wards wired to his
/// manager as main.cpp wires them.
struct BobsWard
{
    BobsWard(TestWard& ward, Device& bob, App& app)
        : incoming(*app.settings, app.library, bob.connection, nullptr)
    {
        QObject::connect(&incoming, &IncomingWards::lostAccess, &app.manager, &DocumentManager::onWardLostElsewhere);
        app.library.add(ward.documentId);
        DocumentRecord record;
        record.baseVersion = 1;
        app.settings->setRecord(ward.documentId, record);
        opened = app.manager.openDocument(ward.documentId, nullptr) == DocumentManager::Open::Fetching;
        ward.settle();
        paths = app.library.pathsFor(ward.documentId);
    }

    IncomingWards incoming;
    bool opened = false;
    DocumentPaths paths;
};

bool removeOnServer(TestWard& ward, Device& admin, Device& person)
{
    return ServerTestSupport::wait<ServerContract::Done>(std::bind(&ServerContract::ServerApi::removeMember,
                                                                   &ward.server,
                                                                   sessionOf(ward, admin),
                                                                   ward.documentId,
                                                                   person.identity.member,
                                                                   std::placeholders::_1))
        .ok();
}

} // namespace

void DocumentManagerTest::aClosedWardTakenAwayGoesAtTheNextCheck()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession.reset();

    App app(bob);
    BobsWard bobs(ward, bob, app);
    QVERIFY(bobs.opened);
    QVERIFY(app.manager.close(nullptr));
    QVERIFY(app.manager.documentId().isEmpty());
    QVERIFY(QFile::exists(bobs.paths.file));
    QVERIFY(QFile::exists(bobs.paths.base));
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    QVERIFY(removeOnServer(ward, alice, bob));
    bobs.incoming.checkNow();
    ward.settle();

    QCOMPARE(lost.count(), 1);
    const ServerContract::LostAccess how = lost.first().at(1).value<ServerContract::LostAccess>();
    QCOMPARE(how.how, ServerContract::LostAccess::How::Removed);
    QCOMPARE(how.byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(bobs.paths.file));
    QVERIFY(!QFile::exists(bobs.paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
    QVERIFY(!app.settings->record(ward.documentId).baseVersion.has_value());
}

void DocumentManagerTest::anOpenWardTakenAwayClosesAtTheNextCheck()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    aliceSession.reset();

    App app(bob);
    BobsWard bobs(ward, bob, app);
    QVERIFY(bobs.opened);
    QCOMPARE(app.manager.documentId(), ward.documentId);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    // The removal reaches the incoming check before the session's own head check;
    // the open ward still closes through its session, once.
    QVERIFY(removeOnServer(ward, alice, bob));
    bobs.incoming.checkNow();
    ward.settle();

    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerContract::LostAccess>().how, ServerContract::LostAccess::How::Removed);
    QVERIFY(app.manager.documentId().isEmpty());
    QVERIFY(!QFile::exists(bobs.paths.file));
    QVERIFY(!QFile::exists(bobs.paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
}
