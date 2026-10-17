#include "WardSessionTest.h"
#include "CommandRegistry.h"
#include "FamilyCommands.h"
#include "TestCommands.h"
#include "TestWard.h"
#include "WardSession.h"

#include <QTest>

namespace
{

CommitResult commitEdit(WardSession& session, const Family& opened, const Family& saved)
{
    return session.commit(UpdateFamilyCommand::fromEdit(opened, saved));
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

/// A session with three entries in its journal - Carol's phone, Bob's phone, the Smith
/// address - abandoned onto a version where Bob's phone moved to 555-9999.
void abandonThreeEdits(WardSession& mine)
{
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999");
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Abandoned);
}

}  // namespace

WardSessionTest::WardSessionTest(QObject* parent)
    : QObject(parent)
{
}

void WardSessionTest::commitAppliesAndJournalsTheSerialisedCommand()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    Family opened = familyIn(session, TestWard::smithId());
    std::unique_ptr<UpdateFamilyCommand> command =
        UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));
    QJsonObject expected = CommandRegistry::toJson(*command);

    CommitResult result = session.commit(std::move(command));

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
    QCOMPARE(session.journal().entries()[0].sequence, qint64(1));
    QVERIFY(session.journal().entries()[0].command == expected);
    QVERIFY(session.canUndo());
    QCOMPARE(session.undoHeading(), QString("Smith family"));
}

void WardSessionTest::editConfirmedAfterARemoteChangeToItsFamilyConflicts()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());

    Family opened = familyIn(mine, TestWard::smithId());
    QVERIFY(commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999").status == CommitStatus::Applied);
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::FastForwarded);

    CommitResult result = commitEdit(mine, opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(!mine.canUndo());
}

void WardSessionTest::refusedCommandChangesNothingAndJournalsNothing()
{
    WardSession session(nullptr);
    session.open(TestWard::document());

    CommitResult refused = session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-2222", "You are the only admin."));

    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(refused.refusal, QString("You are the only admin."));
    QVERIFY(session.document() == TestWard::document());
    QVERIFY(session.journal().isEmpty());

    CommitResult allowed = session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-2222", QString()));

    QVERIFY(allowed.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
}

void WardSessionTest::emptyJournalFastForwards()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(theirs, TestWard::jonesId(), TestWard::carolId(), "555-5555");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::FastForwarded);

    QVERIFY(mine.document() == theirs.document());
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-5555"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(mine.toRedo().isEmpty());
}

void WardSessionTest::offlineEditsToDifferentMembersBothApply()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAliceEmail(theirs, "alice@new.example.com");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(TestWard::person(mine.document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QVERIFY(mine.toRedo().isEmpty());
    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
}

void WardSessionTest::noteAndPhoneEditsToOnePersonBothApply()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    Family opened = familyIn(theirs, TestWard::smithId());
    Person bob = TestWard::bob();
    bob.setSpecialNeedNote("Oxygen at night");
    commitEdit(theirs, opened, TestWard::withMember(opened, bob));

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    Person replayed = TestWard::person(mine.document(), TestWard::bobId());
    QCOMPARE(QString(replayed.phone()), QString("555-2222"));
    QCOMPARE(replayed.specialNeedNote(), QString("Oxygen at night"));
}

void WardSessionTest::journalThatFitsReplaysInFull()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitAliceEmail(theirs, "alice@new.example.com");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-5555"));
    QCOMPARE(familyIn(mine, TestWard::smithId()).address().full(), QString("14 Elm St, Provo, UT"));
    QCOMPARE(TestWard::person(mine.document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QCOMPARE(mine.journal().entries().size(), qsizetype(3));
}

void WardSessionTest::oneConflictAbandonsTheWholeReplay()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999");
    QVERIFY(mine.canUndo());

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Abandoned);

    QVERIFY(mine.document() == theirs.document());
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-4444"));
    QCOMPARE(familyIn(mine, TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(!mine.canUndo());
    QVERIFY(!mine.canRedo());
    QCOMPARE(mine.toRedo().size(), qsizetype(1));
    const ToRedoGroup& group = mine.toRedo()[0];
    QCOMPARE(group.descriptions.size(), qsizetype(3));
    QCOMPARE(group.descriptions[0].heading(), QString("Jones family"));
    QCOMPARE(group.descriptions[0].lines(), QStringList({"Carol, phone: 555-5555"}));
    QCOMPARE(group.descriptions[1].heading(), QString("Smith family"));
    QCOMPARE(group.descriptions[1].lines(), QStringList({"Bob, phone: 555-2222"}));
    QCOMPARE(group.descriptions[2].heading(), QString("Smith family"));
    QCOMPARE(group.descriptions[2].lines(), QStringList({"address: 14 Elm St, Provo, UT"}));
    QCOMPARE(group.clashed, 1);
    QVERIFY(group.stop.status == CommitStatus::Conflicted);
}

void WardSessionTest::commandWhoseTargetWasDeletedConflicts()
{
    WardSession mine(nullptr);
    mine.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    Document version = TestWard::document();
    version.removeFamily(TestWard::jonesId());

    QVERIFY(mine.replayOnto(version) == ReplayStatus::Abandoned);

    QVERIFY(!mine.document().findFamilyById(TestWard::jonesId()).has_value());
    QCOMPARE(mine.toRedo().size(), qsizetype(1));
    QCOMPARE(mine.toRedo()[0].clashed, 0);
    QCOMPARE(mine.toRedo()[0].descriptions[0].heading(), QString("Jones family"));
}

void WardSessionTest::toRedoDescriptionsStayAsWrittenAfterARename()
{
    WardSession mine(nullptr);
    mine.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    Person bob = TestWard::bob();
    bob.setName(Name("Smyth", "Bob"));
    bob.setPhone(Phone("555-9999"));
    Person alice = TestWard::alice();
    alice.setName(Name("Smyth", "Alice"));
    Family renamed = TestWard::smith();
    renamed.setMembers({bob, alice});

    QVERIFY(mine.replayOnto(TestWard::withFamily(TestWard::document(), renamed)) == ReplayStatus::Abandoned);

    QCOMPARE(familyIn(mine, TestWard::smithId()).surname(), QString("Smyth"));
    const CommandDescription& description = mine.toRedo()[0].descriptions[0];
    QCOMPARE(description.heading(), QString("Smith family"));
    QCOMPARE(description.lines(), QStringList({"Bob, phone: 555-2222"}));
}

void WardSessionTest::laterAbandonmentAddsItsOwnGroup()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-6666");
    Document version = mine.document();
    version.removeFamily(TestWard::jonesId());

    QVERIFY(mine.replayOnto(version) == ReplayStatus::Abandoned);

    QCOMPARE(mine.toRedo().size(), qsizetype(2));
    QCOMPARE(mine.toRedo()[0].descriptions.size(), qsizetype(3));
    QCOMPARE(mine.toRedo()[1].descriptions.size(), qsizetype(1));
    QCOMPARE(mine.toRedo()[1].descriptions[0].lines(), QStringList({"Carol, phone: 555-6666"}));
    QCOMPARE(mine.toRedo()[1].clashed, 0);
}

void WardSessionTest::newCommandAfterAnAbandonedReplayJournalsAfresh()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    QVERIFY(mine.journal().isEmpty());

    QVERIFY(commitAliceEmail(mine, "alice@new.example.com").status == CommitStatus::Applied);

    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
    QCOMPARE(mine.journal().entries()[0].sequence, qint64(4));
    QVERIFY(mine.canUndo());
}

void WardSessionTest::undoRunsTheInverseThroughTheCommitStep()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");

    CommitResult undone = session.undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(session.document() == TestWard::document());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
    CommandDescription inverse = CommandRegistry::descriptionOf(session.journal().entries()[1].command);
    QCOMPARE(inverse.heading(), QString("Undo: Smith family"));
    QCOMPARE(inverse.lines(), QStringList({"Bob, phone: 555-2222"}));
    QVERIFY(!session.canUndo());
    QVERIFY(session.canRedo());
    QCOMPARE(session.redoHeading(), QString("Smith family"));

    CommitResult redone = session.redo();

    QVERIFY(redone.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(3));
    QVERIFY(session.journal().entries()[2].command == session.journal().entries()[0].command);
    QCOMPARE(session.journal().entries()[2].sequence, qint64(3));
    QVERIFY(session.canUndo());
    QVERIFY(!session.canRedo());
}

void WardSessionTest::undoAfterARemoteEditToAnotherFamilyApplies()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(theirs, TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT"));
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QVERIFY(mine.undo().status == CommitStatus::Applied);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-1111"));
    QCOMPARE(familyIn(mine, TestWard::jonesId()).address().full(), QString("6 Oak Ave, Provo, UT"));
}

void WardSessionTest::undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");
    QVERIFY(session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);
    QCOMPARE(session.journal().entries().size(), qsizetype(2));

    CommitResult result = session.undo();

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(session.canUndo());
    QCOMPARE(session.undoHeading(), QString("Smith family"));
    QVERIFY(!session.canRedo());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
}

void WardSessionTest::commandWithoutAnInverseNeverEntersTheUndoStack()
{
    WardSession session(nullptr);
    session.open(TestWard::document());

    QVERIFY(session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);

    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-9999"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
    QVERIFY(!session.canUndo());
}

void WardSessionTest::newCommandClearsTheRedoSide()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");
    session.undo();
    QVERIFY(session.canRedo());

    commitPhone(session, TestWard::jonesId(), TestWard::carolId(), "555-5555");

    QVERIFY(!session.canRedo());
    QCOMPARE(session.undoHeading(), QString("Jones family"));
}

void WardSessionTest::undoStackHoldsAtMostOneHundred()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    for (int i = 1; i <= 101; ++i)
    {
        QVERIFY(commitPhone(session, TestWard::smithId(), TestWard::bobId(), QString("555-%1").arg(1000 + i)).status
                == CommitStatus::Applied);
    }

    for (int i = 0; i < 100; ++i)
    {
        QVERIFY(session.canUndo());
        QVERIFY(session.undo().status == CommitStatus::Applied);
    }

    QVERIFY(!session.canUndo());
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-1001"));
}

void WardSessionTest::dismissingTheToRedoListEmptiesIt()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    QCOMPARE(mine.toRedo().size(), qsizetype(1));

    mine.dismissToRedo();

    QVERIFY(mine.toRedo().isEmpty());
}
