#include "DocumentManagerTest.h"

#include "DocumentCrypto.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FamilyCommands.h"
#include "SessionTestSupport.h"

#include <QSignalSpy>
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
