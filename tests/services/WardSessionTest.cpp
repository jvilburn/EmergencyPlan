#include "WardSessionTest.h"
#include "CommandRegistry.h"
#include "FamilyCommands.h"
#include "TestCommands.h"
#include "TestClient.h"
#include "TestWard.h"
#include "WardSession.h"

#include <QSignalSpy>
#include <QTest>

namespace
{

CommitResult commitEdit(WardSession& session, const Family& opened, const Family& saved)
{
    return session.commit(UpdateFamilyCommand::fromEdit(opened, saved, session.document()));
}

Family familyIn(const WardSession& session, const FamilyId& id)
{
    return TestWard::family(session.document(), id);
}

QString phoneOf(const Document& document, const PersonId& id)
{
    return TestWard::person(document, id).phone();
}

/// Commits a phone edit to the member, opened on the session's current family.
CommitResult commitPhone(WardSession& session, const FamilyId& familyId, const PersonId& personId, const QString& phone)
{
    Family opened = familyIn(session, familyId);
    return commitEdit(session, opened, TestWard::withPhone(opened, personId, phone));
}

/// Commits an address edit to the family, opened on the session's current family.
CommitResult commitAddress(WardSession& session, const FamilyId& familyId, const Address& address)
{
    Family opened = familyIn(session, familyId);
    Family saved = opened;
    saved.setAddress(address);
    return commitEdit(session, opened, saved);
}

/// Commits an email edit to Alice, opened on the session's current Smith family.
CommitResult commitAliceEmail(WardSession& session, const QString& email)
{
    Family opened = familyIn(session, TestWard::smithId());
    Person alice = TestWard::person(session.document(), TestWard::aliceId());
    alice.setEmail(email);
    return commitEdit(session, opened, TestWard::withMember(opened, alice));
}

/// Creates a ward from the test ward in mine and gives theirs the same ward, then commits
/// three edits to mine - Carol's phone, Bob's phone, the Smith address - and moves Bob's
/// phone to 555-9999 in theirs.
void makeThreeEditsAgainstAMovedPhone(TestClient& mine, TestClient& theirs)
{
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(mine.session(), TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitPhone(theirs.session(), TestWard::smithId(), TestWard::bobId(), "555-9999");
}

/// A client with three entries in its journal abandoned onto a version where Bob's phone
/// moved; true when the replay was abandoned.
bool abandonThreeEdits(TestClient& mine)
{
    TestClient theirs("Phone");
    makeThreeEditsAgainstAMovedPhone(mine, theirs);
    return mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Abandoned;
}

DocumentChange changeAt(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).value<DocumentChange>();
}

bool isSmithUpdate(const DocumentChange& change)
{
    return change.action == ChangeAction::Updated
        && change.familyId == TestWard::smithId();
}

bool isFullChange(const DocumentChange& change)
{
    return change.action == ChangeAction::Full
        && !change.familyId.has_value();
}

}  // namespace

WardSessionTest::WardSessionTest(QObject* parent)
    : QObject(parent)
{
}

void WardSessionTest::commitAppliesAndJournalsTheSerialisedCommand()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Family opened = familyIn(client.session(), TestWard::smithId());
    CommandPtr command = UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"), client.session().document());
    QJsonObject expected = CommandRegistry::toJson(*command);

    CommitResult result = client.session().commit(command);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(client.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(client.session().journal().entries()[0].sequence, qint64(1));
    QVERIFY(client.session().journal().entries()[0].command == expected);
    QVERIFY(client.session().canUndo());
    QCOMPARE(client.session().undoHeading(), QString("Smith family"));
}

void WardSessionTest::editThatChangesNothingIsNothingToDo()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Document created = client.session().document();
    QSignalSpy documentSpy(&client.session(), &WardSession::documentChanged);
    QSignalSpy undoSpy(&client.session(), &WardSession::undoStackChanged);

    CommitResult result = commitEdit(client.session(), TestWard::smith(), TestWard::smith());

    QVERIFY(result.status == CommitStatus::NothingToDo);
    QVERIFY(client.session().document() == created);
    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!client.session().canUndo());
    QCOMPARE(documentSpy.count(), 0);
    QCOMPARE(undoSpy.count(), 0);
}

void WardSessionTest::editConfirmedAfterARemoteChangeToItsFamilyConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);

    Family opened = familyIn(mine.session(), TestWard::smithId());
    QVERIFY(commitPhone(theirs.session(), TestWard::smithId(), TestWard::bobId(), "555-9999").status == CommitStatus::Applied);
    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::FastForwarded);

    CommitResult result = commitEdit(mine.session(), opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(result.description, CommandDescription("Smith family", QStringList{"Bob, phone: 555-2222"}));
    QCOMPARE(phoneOf(mine.session().document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(mine.session().journal().isEmpty());
    QVERIFY(!mine.session().canUndo());
}

void WardSessionTest::refusedCommandChangesNothingAndJournalsNothing()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Document created = client.session().document();

    CommitResult refused = client.session().commit(std::make_shared<ForcePhoneCommand>(TestWard::bobId(), "555-2222", "You are the only admin."));

    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(refused.refusal, QString("You are the only admin."));
    QVERIFY(client.session().document() == created);
    QVERIFY(client.session().journal().isEmpty());

    CommitResult allowed = client.session().commit(std::make_shared<ForcePhoneCommand>(TestWard::bobId(), "555-2222", QString()));

    QVERIFY(allowed.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(client.session().journal().entries().size(), qsizetype(1));
}

void WardSessionTest::appliedCommitSignalsOneFamilyChangeAndOneUndoStackChange()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    QSignalSpy documentSpy(&client.session(), &WardSession::documentChanged);
    QSignalSpy undoSpy(&client.session(), &WardSession::undoStackChanged);
    QVERIFY(documentSpy.isValid());

    QVERIFY(commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);

    QCOMPARE(documentSpy.count(), 1);
    QVERIFY(isSmithUpdate(changeAt(documentSpy, 0)));
    QCOMPARE(undoSpy.count(), 1);
}

void WardSessionTest::conflictedOrRefusedCommitSignalsNothing()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Family opened = familyIn(client.session(), TestWard::smithId());
    QVERIFY(commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-9999").status == CommitStatus::Applied);
    QSignalSpy documentSpy(&client.session(), &WardSession::documentChanged);
    QSignalSpy undoSpy(&client.session(), &WardSession::undoStackChanged);

    CommitResult conflicted = commitEdit(client.session(), opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));
    CommitResult refused = client.session().commit(std::make_shared<ForcePhoneCommand>(TestWard::bobId(), "555-2222", "You are the only admin."));

    QVERIFY(conflicted.status == CommitStatus::Conflicted);
    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(documentSpy.count(), 0);
    QCOMPARE(undoSpy.count(), 0);
}

void WardSessionTest::emptyJournalFastForwards()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(theirs.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::FastForwarded);
    QVERIFY(result.toRedo.isEmpty());
    QVERIFY(mine.session().document() == theirs.session().document());
    QCOMPARE(phoneOf(mine.session().document(), TestWard::carolId()), QString("555-5555"));
    QVERIFY(mine.session().journal().isEmpty());
}

void WardSessionTest::offlineEditsToDifferentMembersBothApply()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAliceEmail(theirs.session(), "alice@new.example.com");

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QVERIFY(result.toRedo.isEmpty());
    QCOMPARE(phoneOf(mine.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(TestWard::person(mine.session().document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QCOMPARE(mine.session().journal().entries().size(), qsizetype(1));
}

void WardSessionTest::noteAndPhoneEditsToOnePersonBothApply()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    Family opened = familyIn(theirs.session(), TestWard::smithId());
    Person bob = TestWard::bob();
    bob.setSpecialNeedNote("Oxygen at night");
    commitEdit(theirs.session(), opened, TestWard::withMember(opened, bob));

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);

    Person replayed = TestWard::person(mine.session().document(), TestWard::bobId());
    QCOMPARE(QString(replayed.phone()), QString("555-2222"));
    QCOMPARE(replayed.specialNeedNote(), QString("Oxygen at night"));
}

void WardSessionTest::journalThatFitsReplaysInFull()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitPhone(mine.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitAddress(mine.session(), TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitAliceEmail(theirs.session(), "alice@new.example.com");

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);

    QCOMPARE(phoneOf(mine.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(mine.session().document(), TestWard::carolId()), QString("555-5555"));
    QCOMPARE(familyIn(mine.session(), TestWard::smithId()).address().full(), QString("14 Elm St, Provo, UT"));
    QCOMPARE(TestWard::person(mine.session().document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QCOMPARE(mine.session().journal().entries().size(), qsizetype(3));
}

void WardSessionTest::oneConflictAbandonsTheWholeReplay()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    makeThreeEditsAgainstAMovedPhone(mine, theirs);
    QVERIFY(mine.session().canUndo());

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(mine.session().document() == theirs.session().document());
    QCOMPARE(phoneOf(mine.session().document(), TestWard::carolId()), QString("555-4444"));
    QCOMPARE(familyIn(mine.session(), TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
    QVERIFY(mine.session().journal().isEmpty());
    QVERIFY(!mine.session().canUndo());
    QVERIFY(!mine.session().canRedo());
    QCOMPARE(result.toRedo,
             QString("Your changes could not be applied, because the ward changed first.\n"
                     "These need doing again:\n"
                     "\n"
                     "Jones family\n"
                     "- Carol, phone: 555-5555\n"
                     "\n"
                     "Smith family (stopped here)\n"
                     "- Bob, phone: 555-2222\n"
                     "\n"
                     "Smith family\n"
                     "- address: 14 Elm St, Provo, UT\n"));
}

void WardSessionTest::abandonedReplaySignalsOneFullDocumentChange()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    makeThreeEditsAgainstAMovedPhone(mine, theirs);
    QSignalSpy documentSpy(&mine.session(), &WardSession::documentChanged);

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Abandoned);

    QCOMPARE(documentSpy.count(), 1);
    QVERIFY(isFullChange(changeAt(documentSpy, 0)));
}

void WardSessionTest::commandWhoseTargetWasDeletedConflicts()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    commitPhone(mine.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");
    Document version = TestWard::document();
    version.removeFamily(TestWard::jonesId());

    ReplayResult result = mine.session().replayOnto(version);

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.session().document().findFamilyById(TestWard::jonesId()).has_value());
    QCOMPARE(result.toRedo,
             QString("Your changes could not be applied, because the ward changed first.\n"
                     "These need doing again:\n"
                     "\n"
                     "Jones family (stopped here)\n"
                     "- Carol, phone: 555-5555\n"));
}

void WardSessionTest::toRedoTextStaysAsWrittenAfterARename()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    Person bob = TestWard::bob();
    bob.setName(Name("Smyth", "Bob"));
    bob.setPhone(Phone("555-9999"));
    Person alice = TestWard::alice();
    alice.setName(Name("Smyth", "Alice"));
    Family renamed = TestWard::smith();
    renamed.setMembers({bob, alice});

    ReplayResult result = mine.session().replayOnto(TestWard::withFamily(TestWard::document(), renamed));

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QCOMPARE(familyIn(mine.session(), TestWard::smithId()).surname(), QString("Smyth"));
    QCOMPARE(result.toRedo,
             QString("Your changes could not be applied, because the ward changed first.\n"
                     "These need doing again:\n"
                     "\n"
                     "Smith family (stopped here)\n"
                     "- Bob, phone: 555-2222\n"));
}

void WardSessionTest::eachAbandonmentGivesItsOwnText()
{
    TestClient mine("Laptop");
    QVERIFY(abandonThreeEdits(mine));
    commitPhone(mine.session(), TestWard::jonesId(), TestWard::carolId(), "555-6666");
    Document version = mine.session().document();
    version.removeFamily(TestWard::jonesId());

    ReplayResult result = mine.session().replayOnto(version);

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QCOMPARE(result.toRedo,
             QString("Your changes could not be applied, because the ward changed first.\n"
                     "These need doing again:\n"
                     "\n"
                     "Jones family (stopped here)\n"
                     "- Carol, phone: 555-6666\n"));
}

void WardSessionTest::newCommandAfterAnAbandonedReplayJournalsAfresh()
{
    TestClient mine("Laptop");
    QVERIFY(abandonThreeEdits(mine));
    QVERIFY(mine.session().journal().isEmpty());

    QVERIFY(commitAliceEmail(mine.session(), "alice@new.example.com").status == CommitStatus::Applied);

    QCOMPARE(mine.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(mine.session().journal().entries()[0].sequence, qint64(4));
    QVERIFY(mine.session().canUndo());
}

void WardSessionTest::openEmptiesTheJournalAndTheUndoStack()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitPhone(client.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(!client.session().journal().isEmpty());
    QVERIFY(client.session().canUndo());
    QVERIFY(client.session().canRedo());

    client.createWard(TestWard::document());

    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!client.session().canUndo());
    QVERIFY(!client.session().canRedo());
    Document expected = TestWard::document();
    expected.setAccess(client.session().document().access());
    QVERIFY(client.session().document() == expected);
}

void WardSessionTest::undoRunsTheInverseThroughTheCommitStep()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Document created = client.session().document();
    commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);
    QCOMPARE(client.session().journal().entries().size(), qsizetype(2));
    CommandDescription inverse = CommandRegistry::descriptionOf(client.session().journal().entries()[1].command);
    QCOMPARE(inverse.heading(), QString("Undo: Smith family"));
    QCOMPARE(inverse.lines(), QStringList({"Bob, phone: 555-2222"}));
    QVERIFY(!client.session().canUndo());
    QVERIFY(client.session().canRedo());
    QCOMPARE(client.session().redoHeading(), QString("Smith family"));

    CommitResult redone = client.session().redo();

    QVERIFY(redone.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(client.session().journal().entries().size(), qsizetype(3));
    QVERIFY(client.session().journal().entries()[2].command == client.session().journal().entries()[0].command);
    QCOMPARE(client.session().journal().entries()[2].sequence, qint64(3));
    QVERIFY(client.session().canUndo());
    QVERIFY(!client.session().canRedo());
}

void WardSessionTest::appliedUndoSignalsADocumentChangeAndAnUndoStackChange()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    QSignalSpy documentSpy(&client.session(), &WardSession::documentChanged);
    QSignalSpy undoSpy(&client.session(), &WardSession::undoStackChanged);

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QCOMPARE(documentSpy.count(), 1);
    QVERIFY(isSmithUpdate(changeAt(documentSpy, 0)));
    QCOMPARE(undoSpy.count(), 1);
}

void WardSessionTest::removingTheFirstMemberAndUndoingRestoresTheFamilyExactly()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Family opened = familyIn(client.session(), TestWard::smithId());
    Family saved = opened;
    saved.setMembers({TestWard::alice()});
    QVERIFY(commitEdit(client.session(), opened, saved).status == CommitStatus::Applied);
    QVERIFY(familyIn(client.session(), TestWard::smithId()) == saved);

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QVERIFY(familyIn(client.session(), TestWard::smithId()) == TestWard::smith());
}

void WardSessionTest::undoAfterARemoteEditToAnotherFamilyApplies()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(theirs.session(), TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT"));
    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);

    QVERIFY(mine.session().undo().status == CommitStatus::Applied);

    QCOMPARE(phoneOf(mine.session().document(), TestWard::bobId()), QString("555-1111"));
    QCOMPARE(familyIn(mine.session(), TestWard::jonesId()).address().full(), QString("6 Oak Ave, Provo, UT"));
}

void WardSessionTest::undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    QVERIFY(client.session().commit(std::make_shared<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);
    QCOMPARE(client.session().journal().entries().size(), qsizetype(2));

    CommitResult result = client.session().undo();

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(result.description, CommandDescription("Undo: Smith family", QStringList{"Bob, phone: 555-2222"}));
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(client.session().canUndo());
    QCOMPARE(client.session().undoHeading(), QString("Smith family"));
    QVERIFY(!client.session().canRedo());
    QCOMPARE(client.session().journal().entries().size(), qsizetype(2));
}

void WardSessionTest::commandThatIsNotUndoableNeverEntersTheUndoStack()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());

    QVERIFY(client.session().commit(std::make_shared<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);

    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-9999"));
    QCOMPARE(client.session().journal().entries().size(), qsizetype(1));
    QVERIFY(!client.session().canUndo());
}

void WardSessionTest::newCommandClearsTheRedoSide()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    client.session().undo();
    QVERIFY(client.session().canRedo());

    commitPhone(client.session(), TestWard::jonesId(), TestWard::carolId(), "555-5555");

    QVERIFY(!client.session().canRedo());
    QCOMPARE(client.session().undoHeading(), QString("Jones family"));
}

void WardSessionTest::undoBeforeAnAbandonedReplayLeavesNothingToRedo()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222");
    QVERIFY(mine.session().undo().status == CommitStatus::Applied);
    QVERIFY(mine.session().canRedo());
    commitPhone(theirs.session(), TestWard::smithId(), TestWard::bobId(), "555-9999");

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Abandoned);

    QVERIFY(!mine.session().canRedo());
    QVERIFY(!mine.session().canUndo());
    QCOMPARE(phoneOf(mine.session().document(), TestWard::bobId()), QString("555-9999"));
}

void WardSessionTest::undoStackHoldsAtMostOneHundred()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    for (int i = 1; i <= 101; ++i)
    {
        QVERIFY(commitPhone(client.session(), TestWard::smithId(), TestWard::bobId(), QString("555-%1").arg(1000 + i)).status
                == CommitStatus::Applied);
    }

    for (int i = 0; i < 100; ++i)
    {
        QVERIFY(client.session().canUndo());
        QVERIFY(client.session().undo().status == CommitStatus::Applied);
    }

    QVERIFY(!client.session().canUndo());
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-1001"));
}
