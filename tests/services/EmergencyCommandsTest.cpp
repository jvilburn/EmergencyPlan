#include "EmergencyCommandsTest.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "PdfText.h"
#include "TestSession.h"
#include "TestWard.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace
{

/// The emergency in progress in the session's document; throws std::bad_optional_access
/// when none is.
EmergencyResponse emergencyIn(const WardSession& session)
{
    return session.document().emergencyResponse().value();
}

/// The notes of each of the family's attempts in response, in the response's order.
QStringList notesOf(const EmergencyResponse& response, const FamilyId& familyId)
{
    QStringList notes;
    for (const ContactAttempt& attempt : response.contactAttemptsFor(familyId))
    {
        notes.append(attempt.notes());
    }
    return notes;
}

/// The ids of the family's attempts in response, in the response's order.
QStringList attemptIdsOf(const EmergencyResponse& response, const FamilyId& familyId)
{
    QStringList ids;
    for (const ContactAttempt& attempt : response.contactAttemptsFor(familyId))
    {
        ids.append(attempt.id().toString());
    }
    return ids;
}

/// An attempt Bob made by phone on family at hour:minute on 1 October 2026.
ContactAttempt phoned(const QString& id, const Family& family, int hour, int minute, ContactOutcome outcome,
                      const QString& notes)
{
    return TestWard::attempt(id, family, TestWard::octoberFirst(hour, minute), ContactMethod::Phone, TestWard::bob(),
                             outcome, notes);
}

/// An attempt on family made by a button at hour:minute: an outcome and nothing else.
ContactAttempt marked(const QString& id, const Family& family, int hour, int minute, ContactOutcome outcome)
{
    return TestWard::attempt(id, family, TestWard::octoberFirst(hour, minute), std::nullopt, std::nullopt, outcome,
                             QString());
}

TaskId basementId()
{
    return TaskId::fromString("task-basement");
}

/// The Flooding task on the Smith family, created at 10:30.
ResponseTask floodedBasement()
{
    return TestWard::task(basementId().toString(), TestWard::smith(), TestWard::octoberFirst(10, 30), "Flooding",
                          "Water in the basement");
}

/// The family's status in the emergency in progress, from the session's document.
EffectiveContactStatus statusIn(const WardSession& session, const FamilyId& familyId)
{
    return session.document().emergencyStatus(familyId);
}

CommitResult addAttempt(WardSession& session, const ContactAttempt& attempt)
{
    return session.commit(AddContactAttemptCommand::fromNew(attempt));
}

CommitResult commitTaskEdit(WardSession& session, const ResponseTask& opened, const ResponseTask& saved)
{
    return session.commit(UpdateTaskCommand::fromEdit(opened, saved));
}

/// The Smith family with Alice removed through the family edit, as another user would.
CommitResult removeAlice(WardSession& session)
{
    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    return session.commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice, session.document()));
}

void openBoth(WardSession& mine, WardSession& theirs, const Document& document)
{
    mine.open(document);
    theirs.open(document);
}

}  // namespace

EmergencyCommandsTest::EmergencyCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void EmergencyCommandsTest::twoRespondersOfflineLandEveryAttempt()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));

    QVERIFY(addAttempt(theirs, phoned("attempt-their-smith", TestWard::smith(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(theirs, phoned("attempt-their-jones", TestWard::jones(), 10, 10, ContactOutcome::OK, "Carol is fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-my-smith", TestWard::smith(), 10, 20, ContactOutcome::OK, "Visited, roof damage"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-my-jones", TestWard::jones(), 10, 30, ContactOutcome::OK, "Texted back"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine);
    QCOMPARE(storm.contactAttempts().size(), qsizetype(4));
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"Visited, roof damage", "No answer"}));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Texted back", "Carol is fine"}));
}

void EmergencyCommandsTest::twoRespondersFirstAttemptsOnOneFamilyBothLand()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(emergencyIn(mine).contactAttempts().isEmpty());

    QVERIFY(addAttempt(theirs, phoned("attempt-theirs", TestWard::jones(), 10, 0, ContactOutcome::UnableToReach, "Phoned Carol"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-mine", TestWard::jones(), 10, 5, ContactOutcome::OK, "Visited Carol"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine);
    QCOMPARE(storm.contactAttempts().size(), qsizetype(2));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Visited Carol", "Phoned Carol"}));
    QVERIFY(storm.statusByFamily().value(TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(statusIn(mine, TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(statusIn(theirs, TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-ok", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine at ten"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-unreachable", TestWard::smith(), 10, 5, ContactOutcome::UnableToReach,
                                    "No answer at five past"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.document();
    Document mineAlone = mine.document();

    QVERIFY(mine.replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.document() == theirs.document());
    EmergencyResponse storm = emergencyIn(mine);
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"No answer at five past", "Fine at ten"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(statusIn(mine, TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::attemptsAtOneTimeAreOrderedByIdWhicheverReplaysFirst()
{
    // Same timestamp, so the greater id comes first: "attempt-b" before "attempt-a".
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-a", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-b", TestWard::smith(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.document();
    Document mineAlone = mine.document();

    QVERIFY(mine.replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.document() == theirs.document());
    EmergencyResponse storm = emergencyIn(mine);
    QCOMPARE(attemptIdsOf(storm, TestWard::smithId()), QStringList({"attempt-b", "attempt-a"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::undoingAnAddedAttemptRestoresTheEmergencyAndRedoAddsItAgain()
{
    TestSession session;
    session.open(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jones(), 9, 0, ContactOutcome::OK, "Fine")}, {})));
    EmergencyResponse before = emergencyIn(session);
    ContactAttempt first = phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe");

    QVERIFY(addAttempt(session, first).status == CommitStatus::Applied);
    QCOMPARE(attemptIdsOf(emergencyIn(session), TestWard::smithId()), QStringList({"attempt-smith"}));

    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(emergencyIn(session) == before);

    QVERIFY(session.redo().status == CommitStatus::Applied);
    EmergencyResponse redone = emergencyIn(session);
    const ContactAttempt* again = redone.findContactAttempt(first.id());
    QVERIFY(again);
    QVERIFY(*again == first);
    QCOMPARE(redone.contactAttempts().size(), qsizetype(2));
}

void EmergencyCommandsTest::twoOfflineStartsBecomeOneEmergency()
{
    // Each responder starts an emergency offline and records an attempt in it. Theirs
    // reaches the ward first, so mine replays onto it: my start's result holds, and my
    // attempt lands in their emergency.
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::document());
    ContactAttempt dry = phoned("attempt-flood", TestWard::smith(), 10, 0, ContactOutcome::OK, "Dry");
    ContactAttempt roof = phoned("attempt-storm", TestWard::smith(), 10, 5, ContactOutcome::OK, "Roof intact");

    QVERIFY(theirs.commit(StartEmergencyCommand::fromNew(TestWard::flood())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(theirs, dry).status == CommitStatus::Applied);
    QVERIFY(mine.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, roof).status == CommitStatus::Applied);
    QCOMPARE(emergencyIn(mine).name(), QString("Storm"));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse expected = TestWard::flood();
    expected.insertContactAttempt(dry);
    expected.insertContactAttempt(roof);
    QVERIFY(emergencyIn(mine) == expected);
    QCOMPARE(notesOf(emergencyIn(mine), TestWard::smithId()), QStringList({"Roof intact", "Dry"}));
    QVERIFY(statusIn(mine, TestWard::smithId()) == EffectiveContactStatus::OK);
}

void EmergencyCommandsTest::attemptWithNoEmergencyInProgressConflicts()
{
    TestSession session;
    session.open(TestWard::document());
    ContactAttempt attempt = phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe");

    QVERIFY(addAttempt(session, attempt).status == CommitStatus::Conflicted);

    QVERIFY(session.document() == TestWard::document());
    QVERIFY(session.journal().isEmpty());
    QVERIFY(!session.canUndo());

    // The same attempt lands once an emergency is in progress
    QVERIFY(session.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(session, attempt).status == CommitStatus::Applied);
    QCOMPARE(attemptIdsOf(emergencyIn(session), TestWard::smithId()), QStringList({"attempt-smith"}));
}

void EmergencyCommandsTest::startAndNewCategoryAreNotUndoable()
{
    TestSession session;
    session.open(TestWard::document());

    QVERIFY(session.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(emergencyIn(session) == TestWard::storm());
    QVERIFY(!session.canUndo());

    QVERIFY(session.commit(AddTaskCategoryCommand::fromNew("Power lines")).status == CommitStatus::Applied);
    QStringList categories = emergencyIn(session).taskCategories();
    QCOMPARE(categories.size(), TestWard::storm().taskCategories().size() + 1);
    QCOMPARE(categories.last(), QString("Power lines"));
    QVERIFY(!session.canUndo());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
}

void EmergencyCommandsTest::endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheEmergency()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    Document ward = TestWard::withFamily(TestWard::withTeam(TestWard::document(), TestWard::setupTeam()), TestWard::lee(2010));
    session.open(TestWard::withResponse(ward, TestWard::storm()));

    // Smith: two phone attempts, the later with a note long enough to wrap, and a task for
    // the Setup team. Jones: a button mark, a phone attempt by Alice, who is removed before
    // the end, and a task given to Carol, notified and resolved. Lee: nothing, so not
    // contacted.
    QString longNote = "Answered at eleven. Everyone is safe, the roof lost a few shingles over the porch, the "
                       "basement has standing water a few inches deep, and they asked that anyone coming by use "
                       "the side entrance because the front steps are blocked by a fallen limb near the gatepost";
    QVERIFY(addAttempt(session, phoned("attempt-early", TestWard::smith(), 9, 0, ContactOutcome::UnableToReach, "No answer at nine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(session, phoned("attempt-late", TestWard::smith(), 11, 0, ContactOutcome::OK, longNote)).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(session, marked("attempt-marked", TestWard::jones(), 9, 30, ContactOutcome::OK)).status
            == CommitStatus::Applied);
    ContactAttempt byAlice = TestWard::attempt("attempt-alice", TestWard::jones(), TestWard::octoberFirst(9, 45),
                                               ContactMethod::Phone, TestWard::alice(), std::nullopt, "Left a message");
    QVERIFY(addAttempt(session, byAlice).status == CommitStatus::Applied);
    ResponseTask basement = floodedBasement();
    basement.setAssignment(TaskAssignment::toTeam(TestWard::setupTeam()));
    QVERIFY(session.commit(AddTaskCommand::fromNew(basement)).status == CommitStatus::Applied);
    ResponseTask tree = TestWard::task("task-tree", TestWard::jones(), TestWard::octoberFirst(10, 45), "Tree removal",
                                       "Oak across the driveway");
    TaskAssignment toCarol = TaskAssignment::toPerson(TestWard::carol());
    toCarol.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 50)));
    tree.setAssignment(toCarol);
    tree.setResolution(TestWard::resolution("Cut and cleared", TestWard::octoberFirst(12, 0)));
    QVERIFY(session.commit(AddTaskCommand::fromNew(tree)).status == CommitStatus::Applied);
    QVERIFY(removeAlice(session).status == CommitStatus::Applied);

    EndResult result = session.endEmergency(path);

    QVERIFY(result == EndResult::Ended);
    QVERIFY(!session.document().emergencyResponse().has_value());
    QCOMPARE(session.journal().entries().last().command["type"].toString(), QString(EndEmergencyCommand::TypeName));
    QString text = pdfText(path);
    QVERIFY2(text.contains("No answer at nine"), qPrintable(text));
    QVERIFY(text.contains("gatepost"));
    QVERIFY(text.indexOf("No answer at nine") < text.indexOf("gatepost"));
    // An attempt's fields in their one order: time, method, who, outcome, notes
    QVERIFY2(text.contains("Phone - Smith, Bob - Unable to Reach - No answer at nine"), qPrintable(text));
    QVERIFY(text.contains("Marked OK: 1"));
    QVERIFY(text.contains("Water in the basement"));
    QVERIFY(text.contains("Assigned to: Setup"));
    QVERIFY(text.contains("Oak across the driveway"));
    QVERIFY(text.contains("Assigned to: Jones, Carol"));
    QVERIFY(text.contains("Sent the address"));
    QVERIFY(text.contains("Cut and cleared"));
    QVERIFY2(text.contains("Phone - Smith, Alice - Left a message"), qPrintable(text));
    QVERIFY(text.contains("Families Not Contacted (1)"));
    QVERIFY(text.contains("Lee, Ken & Mia"));
}

void EmergencyCommandsTest::attemptsReplayedOntoAnEndedEmergencyAreInTheToRedoText()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(theirs.commit(EndEmergencyCommand::fromReported(emergencyIn(theirs))).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe")).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, marked("attempt-jones", TestWard::jones(), 10, 5, ContactOutcome::UnableToReach)).status
            == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QCOMPARE(result.toRedo, QString("Your changes could not be applied, because the ward changed first.\n"
                                    "These need doing again:\n"
                                    "\n"
                                    "Contact with Smith, Bob & Alice (stopped here)\n"
                                    "- method: Phone\n"
                                    "- who: Smith, Bob\n"
                                    "- outcome: OK\n"
                                    "- notes: All safe\n"
                                    "\n"
                                    "Contact with Jones, Carol\n"
                                    "- outcome: Unable to Reach\n"));
    QVERIFY(!mine.document().emergencyResponse().has_value());
}

void EmergencyCommandsTest::endReplayedAfterAnotherAttemptConflictsAndLeavesTheEmergency()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-theirs", TestWard::smith(), 10, 0, ContactOutcome::OK, "Phoned the Smiths"))
                .status == CommitStatus::Applied);
    QVERIFY(mine.commit(EndEmergencyCommand::fromReported(emergencyIn(mine))).status == CommitStatus::Applied);
    QVERIFY(!mine.document().emergencyResponse().has_value());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("End emergency: Storm (stopped here)"));
    QVERIFY(emergencyIn(mine) == emergencyIn(theirs));
    QCOMPARE(notesOf(emergencyIn(mine), TestWard::smithId()), QStringList({"Phoned the Smiths"}));
}

void EmergencyCommandsTest::undoingAnEndRestoresAnEqualEmergencyKeepsTheReportAndRedoEndsItAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    EmergencyResponse storm = TestWard::stormWith({phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe")},
                                                  {floodedBasement()});
    session.open(TestWard::withResponse(TestWard::document(), storm));

    QVERIFY(session.endEmergency(path) == EndResult::Ended);
    QVERIFY(!session.document().emergencyResponse().has_value());
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(emergencyIn(session) == storm);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(QFile::exists(path));

    QVERIFY(session.redo().status == CommitStatus::Applied);

    QVERIFY(!session.document().emergencyResponse().has_value());
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NotContacted);
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));
}

void EmergencyCommandsTest::restoreConflictsWhenADifferentEmergencyIsInProgress()
{
    // Storm ends, then Flood starts. Undoing the end would put Storm back, but Flood is in
    // progress, so it conflicts and the end stays on the undo stack.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TestSession session;
    session.open(TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(session.endEmergency(dir.filePath("storm.pdf")) == EndResult::Ended);
    QVERIFY(session.commit(StartEmergencyCommand::fromNew(TestWard::flood())).status == CommitStatus::Applied);
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));
    Document before = session.document();

    CommitResult result = session.undo();

    QVERIFY(result.status == CommitStatus::Conflicted);
    QVERIFY(session.document() == before);
    QVERIFY(emergencyIn(session) == TestWard::flood());
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));
    QVERIFY(!session.canRedo());
}

void EmergencyCommandsTest::endWhoseReportCannotBeWrittenEndsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("missing/storm.pdf");
    TestSession session;
    session.open(TestWard::withResponse(TestWard::document(), TestWard::storm()));

    QVERIFY(session.endEmergency(path) == EndResult::ReportFailed);

    QVERIFY(emergencyIn(session) == TestWard::storm());
    QVERIFY(session.journal().isEmpty());
    QVERIFY(!session.canUndo());
    QVERIFY(!QFile::exists(path));
}

void EmergencyCommandsTest::endingWithNoEmergencyInProgressWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    session.open(TestWard::document());

    QVERIFY(session.endEmergency(path) == EndResult::AlreadyEnded);

    QVERIFY(session.document() == TestWard::document());
    QVERIFY(session.journal().isEmpty());
    QVERIFY(!QFile::exists(path));
}

void EmergencyCommandsTest::taskDeletionReplayedAfterAnotherResolvedItConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask resolved = task;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));

    QVERIFY(commitTaskEdit(theirs, task, resolved).status == CommitStatus::Applied);
    QVERIFY(mine.commit(DeleteTaskCommand::fromSeen(task)).status == CommitStatus::Applied);
    QVERIFY(!emergencyIn(mine).findTask(basementId()));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete Flooding task for Smith, Bob & Alice (stopped here)"));
    EmergencyResponse storm = emergencyIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(*after == resolved);
}

void EmergencyCommandsTest::taskEditReplayedAfterTheTaskWasDeletedConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask described = task;
    described.setDescription("Water in the basement and the garage");

    QVERIFY(theirs.commit(DeleteTaskCommand::fromSeen(task)).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, described).status == CommitStatus::Applied);
    QVERIFY(emergencyIn(mine).findTask(basementId()));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n"
                                   "- description: Water in the basement and the garage"));
    QVERIFY(!emergencyIn(mine).findTask(basementId()));
    QVERIFY(emergencyIn(mine) == emergencyIn(theirs));
}

void EmergencyCommandsTest::taskEditsToDifferentValuesBothApply()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    openBoth(mine, theirs, TestWard::withResponse(withTeam, TestWard::stormWith({}, {task})));
    ResponseTask described = task;
    described.setDescription("Water in the basement and the garage");
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toTeam(TestWard::setupTeam()));

    QVERIFY(commitTaskEdit(theirs, task, described).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QCOMPARE(after->description(), QString("Water in the basement and the garage"));
    QVERIFY(after->assignment() == TaskAssignment::toTeam(TestWard::setupTeam()));
    QCOMPARE(after->category(), QString("Flooding"));
    QVERIFY(!after->isResolved());
}

void EmergencyCommandsTest::oneTaskValueSetDifferentlyConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask garage = task;
    garage.setDescription("Water in the garage");
    ResponseTask cellar = task;
    cellar.setDescription("Water in the cellar");

    QVERIFY(commitTaskEdit(theirs, task, garage).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, cellar).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n- description: Water in the cellar"));
    EmergencyResponse storm = emergencyIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QCOMPARE(after->description(), QString("Water in the garage"));
}

void EmergencyCommandsTest::assignmentToAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toPerson(TestWard::alice()));

    QVERIFY(removeAlice(theirs).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);
    EmergencyResponse beforeReplay = emergencyIn(mine);
    const ResponseTask* mineAssigned = beforeReplay.findTask(basementId());
    QVERIFY(mineAssigned);
    QVERIFY(mineAssigned->assignment() == assigned.assignment());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.document().findPersonById(TestWard::aliceId()).has_value());
    EmergencyResponse storm = emergencyIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(!after->isAssigned());
}

void EmergencyCommandsTest::reassigningReplacesTheWholeAssignmentAndUndoRestoresTheNotification()
{
    TestSession session;
    ResponseTask notified = floodedBasement();
    TaskAssignment toSetup = TaskAssignment::toTeam(TestWard::setupTeam());
    toSetup.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 40)));
    notified.setAssignment(toSetup);
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    session.open(TestWard::withResponse(withTeam, TestWard::stormWith({}, {notified})));
    EmergencyResponse opened = emergencyIn(session);
    const ResponseTask* before = opened.findTask(basementId());
    QVERIFY(before);
    QVERIFY(before->isNotified());
    ResponseTask reassigned = notified;
    reassigned.setAssignment(TaskAssignment::toPerson(TestWard::carol()));

    CommitResult result = commitTaskEdit(session, notified, reassigned);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Flooding task for Smith, Bob & Alice", {"assigned to: Jones, Carol"}));
    EmergencyResponse storm = emergencyIn(session);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(after->assignment()->personId() == TestWard::carolId());
    QVERIFY(!after->assignment()->teamId().has_value());
    QVERIFY(!after->isNotified());

    QVERIFY(session.undo().status == CommitStatus::Applied);

    EmergencyResponse undone = emergencyIn(session);
    const ResponseTask* restored = undone.findTask(basementId());
    QVERIFY(restored);
    QVERIFY(*restored == notified);
}

void EmergencyCommandsTest::statisticsCountEveryDirectoryFamilyAndEveryRemovedFamilyTheEmergencyNames()
{
    // Smith: OK, then a visit with no outcome. Jones: OK, with an unresolved task. A family
    // no longer in the directory: Unable to Reach. Lee: no attempt or task.
    Family goneFamily = Family::createWithId(FamilyId::fromString("family-gone"), TestWard::address("9 Birch Ln", "Provo, UT"),
                                             {TestWard::leeMember("person-gil", "Gil", Gender::Male, 1970, true)});
    FamilyId gone = goneFamily.id();
    EmergencyResponse response = TestWard::stormWith(
        {phoned("attempt-smith-ok", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine"),
         TestWard::attempt("attempt-smith-visit", TestWard::smith(), TestWard::octoberFirst(11, 0), ContactMethod::Visit,
                           TestWard::bob(), std::nullopt, "Left a note"),
         phoned("attempt-jones-ok", TestWard::jones(), 10, 5, ContactOutcome::OK, "Fine"),
         phoned("attempt-gone", goneFamily, 10, 15, ContactOutcome::UnableToReach, "Moved away")},
        {TestWard::task("task-jones", TestWard::jones(), TestWard::octoberFirst(10, 10), "Generator", "Needs fuel")});
    Document document = TestWard::withFamily(TestWard::document(), TestWard::lee(2010));
    const QHash<FamilyId, Family>& families = document.families();
    QCOMPARE(families.size(), qsizetype(3));
    QVERIFY(!families.contains(gone));

    QCOMPARE(response.totalFamilies(families), 4);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::OK, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::NeedsHelp, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::UnableToReach, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::NotContacted, families), 1);
    QHash<FamilyId, EffectiveContactStatus> statuses = response.statusByFamily();
    QCOMPARE(statuses.size(), qsizetype(3));
    QVERIFY(statuses.value(TestWard::smithId()) == EffectiveContactStatus::OK);
    QVERIFY(statuses.value(TestWard::jonesId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(statuses.value(gone) == EffectiveContactStatus::UnableToReach);
    QVERIFY(!statuses.contains(FamilyId::fromString("family-lee")));

    Document withStorm = TestWard::withResponse(document, response);
    QVERIFY(withStorm.emergencyStatus(TestWard::jonesId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(withStorm.emergencyStatus(FamilyId::fromString("family-lee")) == EffectiveContactStatus::NotContacted);

    // A change to the emergency as a whole rebuilds the table: with none in progress, every
    // family is Not Contacted
    Document ended = withStorm;
    ended.setEmergencyResponse(std::nullopt);
    ended.onDocumentChanged(DocumentChange::emergency().updated());
    QVERIFY(ended.emergencyStatus(TestWard::jonesId()) == EffectiveContactStatus::NotContacted);
}

void EmergencyCommandsTest::statusTableFollowsEachChangeToOneFamily()
{
    TestSession session;
    session.open(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jones(), 9, 0, ContactOutcome::UnableToReach, "No answer")},
                            {})));
    QVERIFY(statusIn(session, TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NotContacted);

    QVERIFY(addAttempt(session, phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine")).status
            == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::OK);

    QVERIFY(session.commit(AddTaskCommand::fromNew(floodedBasement())).status == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);

    ResponseTask unresolved = floodedBasement();
    ResponseTask resolved = unresolved;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    QVERIFY(commitTaskEdit(session, unresolved, resolved).status == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::OK);

    // Undo takes each change back: the task reopens, then goes, then the attempt goes.
    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::OK);
    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NotContacted);

    // The other family was left alone, and the table equals a rebuild from the emergency.
    QVERIFY(statusIn(session, TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QHash<FamilyId, EffectiveContactStatus> rebuilt = emergencyIn(session).statusByFamily();
    QCOMPARE(rebuilt.size(), qsizetype(1));
    QVERIFY(rebuilt.value(TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
}
