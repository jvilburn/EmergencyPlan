#include "DocumentManagerTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FamilyCommands.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace SessionTestSupport;
using Timer = SessionTimers::Timer;

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

/// Joins link in app, as opening the link there does, and says whether the ward
/// opened.
bool joinInApp(TestWard& ward, App& app, const InviteLink& link)
{
    QSignalSpy finished(&app.manager, &DocumentManager::fetchFinished);
    app.manager.joinWard(link);
    ward.settle();
    return finished.count() == 1
        && finished.first().at(1).value<DocumentManager::Open>() == DocumentManager::Open::Opened
        && app.manager.documentId() == link.documentId;
}

/// An invite made in app's open ward, its version published.
std::optional<DocumentManager::Invitation> inviteFrom(TestWard& ward,
                                                      App& app,
                                                      Invite::For purpose,
                                                      Role role,
                                                      const QString& label)
{
    const std::optional<DocumentManager::Invitation> made = app.manager.invite(purpose, role, label);
    ward.settle();
    return made;
}

QStringList sorted(QStringList list)
{
    list.sort();
    return list;
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
    DocumentManager manager(alice.identity, alice.records, std::move(session), nullptr);
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
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));
    aliceSession->checkServer();
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();

    DocumentManager manager(alice.identity, alice.records, std::move(aliceSession), nullptr);
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
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));
    aliceSession->checkServer();
    std::unique_ptr<DocumentSession> bobSession = bob.open(ward.documentId);
    QVERIFY(bobSession);
    ward.settle();

    DocumentManager manager(alice.identity, alice.records, std::move(aliceSession), nullptr);
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

    DocumentManager manager(alice.identity, alice.records, std::move(session), nullptr);
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
    DocumentManager manager(alice.identity, alice.records, std::move(session), nullptr);

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
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));
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

    DocumentManager manager(bob.identity, bob.records, bob.open(ward.documentId), nullptr);
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
    QVERIFY(shareFrom(ward, app));
    QVERIFY(app.manager.unseenRosterChanges().isEmpty());

    // Alice's own invite is news like any other change; Bob joining with it shows at
    // the next head, as what changed since the roster was last dismissed.
    const std::optional<DocumentManager::Invitation> made = inviteFrom(ward, app, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(made.has_value());
    QCOMPARE(app.manager.unseenRosterChanges(), QStringList{"Alice made an invite, for Bob"});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(ward.join(bob, made->link), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.unseenRosterChanges(), QStringList{"Bob joined, as a member"});
    QCOMPARE(changed.count(), 2);

    // A second head with nothing new shows the same, unannounced.
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.unseenRosterChanges().size(), 1);
    QCOMPARE(changed.count(), 2);

    // Dismissed: seen on the record, so later heads show nothing.
    app.manager.dismissRosterChanges();
    QVERIFY(app.manager.unseenRosterChanges().isEmpty());
    QCOMPARE(changed.count(), 3);
    QCOMPARE(SealingSet::fromJson(app.settings->record(ward.documentId).rosterShown).value(), app.manager.roster());
    app.manager.session()->checkServer();
    ward.settle();
    QVERIFY(app.manager.unseenRosterChanges().isEmpty());

    // A later change shows alone.
    app.manager.setRole(bob.identity.member, Role::Admin);
    ward.settle();
    QCOMPARE(app.manager.unseenRosterChanges(), QStringList{"Bob is now an admin"});
}

void DocumentManagerTest::aFetchThatFindsAccessLostSaysWhy()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Laptop"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.addUser(*aliceSession, bob, Role::Member));

    // Bob's app knows the ward but has no file of it, so opening it fetches.
    App app(bob);
    app.library.add(ward.documentId);
    DocumentRecord record;
    record.baseVersion = 1;
    app.settings->setRecord(ward.documentId, record);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    // Alice removes Bob.
    aliceSession->checkServer();
    ward.settle();
    changeRoster(*aliceSession, removePerson(bob.identity.member));
    aliceSession->publishNow();
    ward.settle();
    QCOMPARE(ward.recipientsOnServer(), QStringList{alice.identity.fingerprint().toHex()});

    QCOMPARE(app.manager.openDocument(ward.documentId, nullptr), DocumentManager::Open::Fetching);
    ward.settle();

    // A fetch holds no roster to say who: removed, by nobody named.
    QCOMPARE(lost.count(), 1);
    const ServerVersion::LostAccess how = lost.first().at(1).value<ServerVersion::LostAccess>();
    QCOMPARE(how.how, ServerVersion::LostAccess::How::Removed);
    QCOMPARE(how.byWhom, QString());
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

void DocumentManagerTest::anInviteJoinsAUserOrAnotherDevice()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& tablet = ward.add(newIdentity("Alice", "Tablet"));
    App app(alice);

    // An unshared ward makes no invite.
    QVERIFY(app.manager.newDocument(nullptr));
    QVERIFY(!app.manager.invite(Invite::For::User, Role::Member, "for Bob").has_value());
    QVERIFY(app.manager.roster().invites().isEmpty());
    QVERIFY(shareFrom(ward, app));

    // A user invite: a new admin, named by the joiner. The message names the ward as
    // its directory does.
    QCOMPARE(app.manager.session()->edit(DocumentSession::Edit::Directory, &nameTheWard), DocumentSession::Refusal::None);
    const std::optional<DocumentManager::Invitation> forBob = inviteFrom(ward, app, Invite::For::User, Role::Admin, "for Bob");
    QVERIFY(forBob.has_value());
    QCOMPARE(forBob->link.documentId, ward.documentId);
    QCOMPARE(ward.head()->version, 2);
    QCOMPARE(app.manager.roster().invites().size(), 1);
    QVERIFY(app.manager.roster().invites().first().role == Role::Admin);
    QCOMPARE(forBob->message,
             "Alice has invited you to Tanglewood Ward in Ward Planning. Install it from "
             "https://github.com/jvilburn/EmergencyPlan/releases/latest, then open this link:\n"
                 + forBob->link.toString());
    QCOMPARE(ward.join(bob, forBob->link), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
    QCOMPARE(ward.head()->version, 3);

    // A device invite: another of Alice's devices, an admin as she is. Made before
    // her session has seen Bob join: the two roster edits cross without trouble.
    const std::optional<DocumentManager::Invitation> forTablet =
        inviteFrom(ward, app, Invite::For::Device, Role::Member, "for Tablet");
    QVERIFY(forTablet.has_value());
    QCOMPARE(ward.head()->version, 4);
    QCOMPARE(ward.join(tablet, forTablet->link), std::optional<DocumentSession::Join>(DocumentSession::Join::Done));
    QCOMPARE(ward.head()->version, 5);

    app.manager.session()->checkServer();
    ward.settle();
    const SealingSet roster = app.manager.roster();
    QCOMPARE(roster.people().size(), 2);
    QCOMPARE(roster.person(bob.identity.member)->name, QString("Bob"));
    QVERIFY(roster.person(bob.identity.member)->role == Role::Admin);
    QCOMPARE(roster.person(alice.identity.member)->devices.size(), 2);
    QVERIFY(roster.personOf(tablet.identity.fingerprint())->id == alice.identity.member);
    QVERIFY(roster.invites().isEmpty());
    QCOMPARE(ward.recipientsOnServer(),
             sorted({alice.identity.fingerprint().toHex(),
                     bob.identity.fingerprint().toHex(),
                     tablet.identity.fingerprint().toHex()}));
}

void DocumentManagerTest::aLinkForAWardAlreadyHereOpensItAndWritesNothing()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const std::optional<InviteLink> forBob =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(forBob.has_value());
    App app(bob);
    QVERIFY(joinInApp(ward, app, forBob.value()));
    const std::optional<InviteLink> forCarol =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Carol");
    QVERIFY(forCarol.has_value());

    // Bob records a contact while out of reach, then opens the second link: the ward
    // he holds is opened as it is, his unpublished work untouched.
    const qint64 head = ward.head()->version;
    ward.server.setOffline(true);
    QCOMPARE(app.manager.session()->edit(DocumentSession::Edit::Response, recordContact(ward.adams, ContactStatus::OK)),
             DocumentSession::Refusal::None);
    QVERIFY(app.manager.session()->flush(nullptr));
    const QString file = app.library.pathsFor(ward.documentId).file;
    QFile written(file);
    QVERIFY(written.open(QIODevice::ReadOnly));
    const QByteArray before = written.readAll();
    written.close();

    QSignalSpy finished(&app.manager, &DocumentManager::fetchFinished);
    QSignalSpy failed(&app.manager, &DocumentManager::joinFailed);
    app.manager.joinWard(forCarol.value());
    ward.settle();
    ward.server.setOffline(false);

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(1).value<DocumentManager::Open>(), DocumentManager::Open::Opened);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(app.manager.documentId(), ward.documentId);
    QCOMPARE(statusOf(app.manager.document(), ward.adams), ContactStatus::OK);
    QCOMPARE(app.manager.session()->syncState(), SyncState::PendingUpload);
    QFile after(file);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), before);
    QCOMPARE(ward.head()->version, head);
}

void DocumentManagerTest::anAdminRemovesWithdrawsAndChangesRoles()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QVERIFY(ward.addUser(*app.manager.session(), bob, Role::Member));
    QVERIFY(ward.addUser(*app.manager.session(), carol, Role::Member));
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(app.manager.roster().people().size(), 3);
    const qint64 before = ward.head()->version;
    QCOMPARE(before, 5);

    app.manager.setRole(bob.identity.member, Role::Admin);
    ward.settle();
    QCOMPARE(ward.head()->version, 6);
    QVERIFY(ward.serverCopy(bob)->document.sealingSet().person(bob.identity.member)->role == Role::Admin);

    app.manager.removePerson(carol.identity.member);
    ward.settle();
    QCOMPARE(ward.head()->version, 7);
    QVERIFY(!ward.recipientsOnServer().contains(carol.identity.fingerprint().toHex()));
    QCOMPARE(ward.recipientsOnServer().size(), 2);

    const std::optional<DocumentManager::Invitation> forDave = inviteFrom(ward, app, Invite::For::User, Role::Member, "for Dave");
    QVERIFY(forDave.has_value());
    QCOMPARE(ward.recipientsOnServer().size(), 3);
    app.manager.withdrawInvite(app.manager.roster().invites().first().fingerprint());
    ward.settle();
    QCOMPARE(ward.head()->version, 9);
    QCOMPARE(ward.recipientsOnServer().size(), 2);
    Device& dave = ward.add(newIdentity("Dave", "Laptop"));
    QCOMPARE(ward.join(dave, forDave->link), std::optional<DocumentSession::Join>(DocumentSession::Join::InviteUsed));

    // Neither takes out this person or this device: that is leave. Alice has a second
    // device by then, so only the manager's own-device rule refuses the unlink.
    Device& tablet = ward.add(newIdentity("Alice", "Tablet"));
    QVERIFY(ward.addDevice(*app.manager.session(), tablet));
    app.manager.session()->checkServer();
    ward.settle();
    QCOMPARE(ward.head()->version, 11);
    app.manager.removePerson(alice.identity.member);
    app.manager.removeDevice(alice.identity.fingerprint());
    ward.settle();
    QCOMPARE(ward.head()->version, 11);
    QCOMPARE(app.manager.roster().person(alice.identity.member)->devices.size(), 2);
}

void DocumentManagerTest::theLastAdminCannotStepDown()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(alice);
    QVERIFY(shareFrom(ward, app));
    QVERIFY(ward.addUser(*app.manager.session(), bob, Role::Member));
    app.manager.session()->checkServer();
    ward.settle();
    const QString documentId = ward.documentId;
    const QString file = app.library.pathsFor(documentId).file;
    QCOMPARE(ward.head()->version, 3);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    // The only admin, with Bob still here: stepping down is refused.
    app.manager.setRole(alice.identity.member, Role::Member);
    ward.settle();
    QVERIFY(app.manager.self()->role == Role::Admin);
    QCOMPARE(ward.head()->version, 3);

    // Anchor: with Bob made an admin first, she steps down, then leaves.
    app.manager.setRole(bob.identity.member, Role::Admin);
    ward.settle();
    app.manager.setRole(alice.identity.member, Role::Member);
    ward.settle();
    QVERIFY(app.manager.self()->role == Role::Member);
    QCOMPARE(ward.head()->version, 5);
    app.manager.leave();
    ward.settle();
    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerVersion::LostAccess>().how, ServerVersion::LostAccess::How::Left);
    QVERIFY(!QFile::exists(file));
    QVERIFY(!app.library.contains(documentId));
    QCOMPARE(ward.recipientsOnServer(), QStringList{bob.identity.fingerprint().toHex()});
}

void DocumentManagerTest::aMemberLeavesAndTheWardGoes()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    const std::optional<InviteLink> link =
        ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
    QVERIFY(link.has_value());
    aliceSession.reset();

    App app(bob);
    QVERIFY(joinInApp(ward, app, link.value()));
    const DocumentPaths paths = app.library.pathsFor(ward.documentId);
    QVERIFY(QFile::exists(paths.file));
    QVERIFY(QFile::exists(paths.base));
    QCOMPARE(ward.recipientsOnServer().size(), 2);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    app.manager.leave();
    ward.settle();
    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerVersion::LostAccess>().how, ServerVersion::LostAccess::How::Left);
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
    QVERIFY(app.manager.documentId().isEmpty());
    QCOMPARE(ward.recipientsOnServer(), QStringList{alice.identity.fingerprint().toHex()});
}

namespace
{

/// Alice's ward with Bob joined in his app through her invite, and Alice's session
/// current with it.
struct BobsWard
{
    BobsWard(TestWard& ward, Device& alice, App& app)
    {
        aliceSession = ward.createShared(alice);
        if (!aliceSession)
        {
            return;
        }
        const std::optional<InviteLink> link =
            ward.invite(*aliceSession, Invite::For::User, Role::Member, "for Bob");
        opened = link.has_value() && joinInApp(ward, app, link.value());
        paths = app.library.pathsFor(ward.documentId);
        aliceSession->checkServer();
        ward.settle();
    }

    /// Alice writes Bob out.
    bool removeBob(TestWard& ward, const MemberId& bob)
    {
        changeRoster(*aliceSession, removePerson(bob));
        aliceSession->publishNow();
        ward.settle();
        return aliceSession->syncState() == SyncState::UpToDate && ward.recipientsOnServer().size() == 1;
    }

    std::unique_ptr<DocumentSession> aliceSession;
    bool opened = false;
    DocumentPaths paths;
};

} // namespace

void DocumentManagerTest::aClosedWardTakenAwayGoesWhenItIsNextOpened()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(bob);
    BobsWard bobs(ward, alice, app);
    QVERIFY(bobs.opened);
    QVERIFY(app.manager.close(nullptr));
    QVERIFY(app.manager.documentId().isEmpty());
    QVERIFY(QFile::exists(bobs.paths.file));
    QVERIFY(QFile::exists(bobs.paths.base));
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    QVERIFY(bobs.removeBob(ward, bob.identity.member));
    ward.settle();
    QCOMPARE(lost.count(), 0);
    QVERIFY(QFile::exists(bobs.paths.file));

    // Opened, its local file opens, and the head it checks leaves this device out.
    QCOMPARE(app.manager.openDocument(ward.documentId, nullptr), DocumentManager::Open::Opened);
    ward.settle();

    QCOMPARE(lost.count(), 1);
    const ServerVersion::LostAccess how = lost.first().at(1).value<ServerVersion::LostAccess>();
    QCOMPARE(how.how, ServerVersion::LostAccess::How::Removed);
    QCOMPARE(how.byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(bobs.paths.file));
    QVERIFY(!QFile::exists(bobs.paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
    QVERIFY(!app.settings->record(ward.documentId).baseVersion.has_value());
    QVERIFY(app.manager.documentId().isEmpty());
}

void DocumentManagerTest::anOpenWardTakenAwayClosesAtTheNextCheck()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    App app(bob);
    BobsWard bobs(ward, alice, app);
    QVERIFY(bobs.opened);
    QCOMPARE(app.manager.documentId(), ward.documentId);
    QSignalSpy lost(&app.manager, &DocumentManager::accessLost);

    QVERIFY(bobs.removeBob(ward, bob.identity.member));
    QCOMPARE(lost.count(), 0);
    app.manager.session()->checkServer();
    ward.settle();

    QCOMPARE(lost.count(), 1);
    QCOMPARE(lost.first().at(1).value<ServerVersion::LostAccess>().how, ServerVersion::LostAccess::How::Removed);
    QCOMPARE(lost.first().at(1).value<ServerVersion::LostAccess>().byWhom, QString("Alice"));
    QVERIFY(app.manager.documentId().isEmpty());
    QVERIFY(!QFile::exists(bobs.paths.file));
    QVERIFY(!QFile::exists(bobs.paths.base));
    QVERIFY(!app.library.contains(ward.documentId));
}
