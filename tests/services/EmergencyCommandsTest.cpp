#include "EmergencyCommandsTest.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "PdfText.h"
#include "TestClient.h"
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

/// Creates a ward from document in mine, and theirs takes it as it stands.
void shareWard(TestClient& mine, TestClient& theirs, const Document& document)
{
    mine.createWard(document);
    theirs.takeWardOf(mine);
}

}  // namespace

EmergencyCommandsTest::EmergencyCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void EmergencyCommandsTest::twoRespondersOfflineLandEveryAttempt()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));

    QVERIFY(addAttempt(theirs.session(), phoned("attempt-their-smith", TestWard::smith(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(theirs.session(), phoned("attempt-their-jones", TestWard::jones(), 10, 10, ContactOutcome::OK, "Carol is fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-my-smith", TestWard::smith(), 10, 20, ContactOutcome::OK, "Visited, roof damage"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-my-jones", TestWard::jones(), 10, 30, ContactOutcome::OK, "Texted back"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine.session());
    QCOMPARE(storm.contactAttempts().size(), qsizetype(4));
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"Visited, roof damage", "No answer"}));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Texted back", "Carol is fine"}));
}

void EmergencyCommandsTest::twoRespondersFirstAttemptsOnOneFamilyBothLand()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(emergencyIn(mine.session()).contactAttempts().isEmpty());

    QVERIFY(addAttempt(theirs.session(), phoned("attempt-theirs", TestWard::jones(), 10, 0, ContactOutcome::UnableToReach, "Phoned Carol"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-mine", TestWard::jones(), 10, 5, ContactOutcome::OK, "Visited Carol"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine.session());
    QCOMPARE(storm.contactAttempts().size(), qsizetype(2));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Visited Carol", "Phoned Carol"}));
    QVERIFY(storm.statusByFamily().value(TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(statusIn(mine.session(), TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(statusIn(theirs.session(), TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs.session(), phoned("attempt-ok", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine at ten"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-unreachable", TestWard::smith(), 10, 5, ContactOutcome::UnableToReach,
                                    "No answer at five past"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.session().document();
    Document mineAlone = mine.session().document();

    QVERIFY(mine.session().replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.session().replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.session().document() == theirs.session().document());
    EmergencyResponse storm = emergencyIn(mine.session());
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"No answer at five past", "Fine at ten"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(statusIn(mine.session(), TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::attemptsAtOneTimeAreOrderedByIdWhicheverReplaysFirst()
{
    // Same timestamp, so the greater id comes first: "attempt-b" before "attempt-a".
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs.session(), phoned("attempt-a", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-b", TestWard::smith(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.session().document();
    Document mineAlone = mine.session().document();

    QVERIFY(mine.session().replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.session().replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.session().document() == theirs.session().document());
    EmergencyResponse storm = emergencyIn(mine.session());
    QCOMPARE(attemptIdsOf(storm, TestWard::smithId()), QStringList({"attempt-b", "attempt-a"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::undoingAnAddedAttemptRestoresTheEmergencyAndRedoAddsItAgain()
{
    TestClient client("Laptop");
    client.createWard(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jones(), 9, 0, ContactOutcome::OK, "Fine")}, {})));
    EmergencyResponse before = emergencyIn(client.session());
    ContactAttempt first = phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe");

    QVERIFY(addAttempt(client.session(), first).status == CommitStatus::Applied);
    QCOMPARE(attemptIdsOf(emergencyIn(client.session()), TestWard::smithId()), QStringList({"attempt-smith"}));

    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(emergencyIn(client.session()) == before);

    QVERIFY(client.session().redo().status == CommitStatus::Applied);
    EmergencyResponse redone = emergencyIn(client.session());
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
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::document());
    ContactAttempt dry = phoned("attempt-flood", TestWard::smith(), 10, 0, ContactOutcome::OK, "Dry");
    ContactAttempt roof = phoned("attempt-storm", TestWard::smith(), 10, 5, ContactOutcome::OK, "Roof intact");

    QVERIFY(theirs.session().commit(StartEmergencyCommand::fromNew(TestWard::flood())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(theirs.session(), dry).status == CommitStatus::Applied);
    QVERIFY(mine.session().commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), roof).status == CommitStatus::Applied);
    QCOMPARE(emergencyIn(mine.session()).name(), QString("Storm"));

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse expected = TestWard::flood();
    expected.insertContactAttempt(dry);
    expected.insertContactAttempt(roof);
    QVERIFY(emergencyIn(mine.session()) == expected);
    QCOMPARE(notesOf(emergencyIn(mine.session()), TestWard::smithId()), QStringList({"Roof intact", "Dry"}));
    QVERIFY(statusIn(mine.session(), TestWard::smithId()) == EffectiveContactStatus::OK);
}

void EmergencyCommandsTest::attemptWithNoEmergencyInProgressConflicts()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Document created = client.session().document();
    ContactAttempt attempt = phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe");

    QVERIFY(addAttempt(client.session(), attempt).status == CommitStatus::Conflicted);

    QVERIFY(client.session().document() == created);
    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!client.session().canUndo());

    // The same attempt lands once an emergency is in progress
    QVERIFY(client.session().commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(client.session(), attempt).status == CommitStatus::Applied);
    QCOMPARE(attemptIdsOf(emergencyIn(client.session()), TestWard::smithId()), QStringList({"attempt-smith"}));
}

void EmergencyCommandsTest::startAndNewCategoryAreNotUndoable()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());

    QVERIFY(client.session().commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(emergencyIn(client.session()) == TestWard::storm());
    QVERIFY(!client.session().canUndo());

    QVERIFY(client.session().commit(AddTaskCategoryCommand::fromNew("Power lines")).status == CommitStatus::Applied);
    QStringList categories = emergencyIn(client.session()).taskCategories();
    QCOMPARE(categories.size(), TestWard::storm().taskCategories().size() + 1);
    QCOMPARE(categories.last(), QString("Power lines"));
    QVERIFY(!client.session().canUndo());
    QCOMPARE(client.session().journal().entries().size(), qsizetype(2));
}

void EmergencyCommandsTest::endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheEmergency()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestClient client("Laptop");
    Document ward = TestWard::withFamily(TestWard::withTeam(TestWard::document(), TestWard::setupTeam()), TestWard::lee(2010));
    client.createWard(TestWard::withResponse(ward, TestWard::storm()));

    // Smith: two phone attempts, the later with a note long enough to wrap, and a task for
    // the Setup team. Jones: a button mark, a phone attempt by Alice, who is removed before
    // the end, and a task given to Carol, notified and resolved. Lee: nothing, so not
    // contacted.
    QString longNote = "Answered at eleven. Everyone is safe, the roof lost a few shingles over the porch, the "
                       "basement has standing water a few inches deep, and they asked that anyone coming by use "
                       "the side entrance because the front steps are blocked by a fallen limb near the gatepost";
    QVERIFY(addAttempt(client.session(), phoned("attempt-early", TestWard::smith(), 9, 0, ContactOutcome::UnableToReach, "No answer at nine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(client.session(), phoned("attempt-late", TestWard::smith(), 11, 0, ContactOutcome::OK, longNote)).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(client.session(), marked("attempt-marked", TestWard::jones(), 9, 30, ContactOutcome::OK)).status
            == CommitStatus::Applied);
    ContactAttempt byAlice = TestWard::attempt("attempt-alice", TestWard::jones(), TestWard::octoberFirst(9, 45),
                                               ContactMethod::Phone, TestWard::alice(), std::nullopt, "Left a message");
    QVERIFY(addAttempt(client.session(), byAlice).status == CommitStatus::Applied);
    ResponseTask basement = floodedBasement();
    basement.setAssignment(TaskAssignment::toTeam(TestWard::setupTeam()));
    QVERIFY(client.session().commit(AddTaskCommand::fromNew(basement)).status == CommitStatus::Applied);
    ResponseTask tree = TestWard::task("task-tree", TestWard::jones(), TestWard::octoberFirst(10, 45), "Tree removal",
                                       "Oak across the driveway");
    TaskAssignment toCarol = TaskAssignment::toPerson(TestWard::carol());
    toCarol.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 50)));
    tree.setAssignment(toCarol);
    tree.setResolution(TestWard::resolution("Cut and cleared", TestWard::octoberFirst(12, 0)));
    QVERIFY(client.session().commit(AddTaskCommand::fromNew(tree)).status == CommitStatus::Applied);
    QVERIFY(removeAlice(client.session()).status == CommitStatus::Applied);

    EndResult result = client.session().endEmergency(path);

    QVERIFY(result == EndResult::Ended);
    QVERIFY(!client.session().document().emergencyResponse().has_value());
    QCOMPARE(client.session().journal().entries().last().command["type"].toString(), QString(EndEmergencyCommand::TypeName));
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
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(theirs.session().commit(EndEmergencyCommand::fromReported(emergencyIn(theirs.session()))).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe")).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(mine.session(), marked("attempt-jones", TestWard::jones(), 10, 5, ContactOutcome::UnableToReach)).status
            == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

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
    QVERIFY(!mine.session().document().emergencyResponse().has_value());
}

void EmergencyCommandsTest::endReplayedAfterAnotherAttemptConflictsAndLeavesTheEmergency()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs.session(), phoned("attempt-theirs", TestWard::smith(), 10, 0, ContactOutcome::OK, "Phoned the Smiths"))
                .status == CommitStatus::Applied);
    QVERIFY(mine.session().commit(EndEmergencyCommand::fromReported(emergencyIn(mine.session()))).status == CommitStatus::Applied);
    QVERIFY(!mine.session().document().emergencyResponse().has_value());

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("End emergency: Storm (stopped here)"));
    QVERIFY(emergencyIn(mine.session()) == emergencyIn(theirs.session()));
    QCOMPARE(notesOf(emergencyIn(mine.session()), TestWard::smithId()), QStringList({"Phoned the Smiths"}));
}

void EmergencyCommandsTest::undoingAnEndRestoresAnEqualEmergencyKeepsTheReportAndRedoEndsItAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestClient client("Laptop");
    EmergencyResponse storm = TestWard::stormWith({phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "All safe")},
                                                  {floodedBasement()});
    client.createWard(TestWard::withResponse(TestWard::document(), storm));

    QVERIFY(client.session().endEmergency(path) == EndResult::Ended);
    QVERIFY(!client.session().document().emergencyResponse().has_value());
    QCOMPARE(client.session().undoHeading(), QString("End emergency: Storm"));

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QVERIFY(emergencyIn(client.session()) == storm);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(QFile::exists(path));

    QVERIFY(client.session().redo().status == CommitStatus::Applied);

    QVERIFY(!client.session().document().emergencyResponse().has_value());
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NotContacted);
    QCOMPARE(client.session().undoHeading(), QString("End emergency: Storm"));
}

void EmergencyCommandsTest::restoreConflictsWhenADifferentEmergencyIsInProgress()
{
    // Storm ends, then Flood starts. Undoing the end would put Storm back, but Flood is in
    // progress, so it conflicts and the end stays on the undo stack.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TestClient client("Laptop");
    client.createWard(TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(client.session().endEmergency(dir.filePath("storm.pdf")) == EndResult::Ended);
    QVERIFY(client.session().commit(StartEmergencyCommand::fromNew(TestWard::flood())).status == CommitStatus::Applied);
    QCOMPARE(client.session().undoHeading(), QString("End emergency: Storm"));
    Document before = client.session().document();

    CommitResult result = client.session().undo();

    QVERIFY(result.status == CommitStatus::Conflicted);
    QVERIFY(client.session().document() == before);
    QVERIFY(emergencyIn(client.session()) == TestWard::flood());
    QCOMPARE(client.session().undoHeading(), QString("End emergency: Storm"));
    QVERIFY(!client.session().canRedo());
}

void EmergencyCommandsTest::endWhoseReportCannotBeWrittenEndsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("missing/storm.pdf");
    TestClient client("Laptop");
    client.createWard(TestWard::withResponse(TestWard::document(), TestWard::storm()));

    QVERIFY(client.session().endEmergency(path) == EndResult::ReportFailed);

    QVERIFY(emergencyIn(client.session()) == TestWard::storm());
    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!client.session().canUndo());
    QVERIFY(!QFile::exists(path));
}

void EmergencyCommandsTest::endingWithNoEmergencyInProgressWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Document created = client.session().document();

    QVERIFY(client.session().endEmergency(path) == EndResult::AlreadyEnded);

    QVERIFY(client.session().document() == created);
    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!QFile::exists(path));
}

void EmergencyCommandsTest::taskDeletionReplayedAfterAnotherResolvedItConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    ResponseTask task = floodedBasement();
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask resolved = task;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));

    QVERIFY(commitTaskEdit(theirs.session(), task, resolved).status == CommitStatus::Applied);
    QVERIFY(mine.session().commit(DeleteTaskCommand::fromSeen(task)).status == CommitStatus::Applied);
    QVERIFY(!emergencyIn(mine.session()).findTask(basementId()));

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete Flooding task for Smith, Bob & Alice (stopped here)"));
    EmergencyResponse storm = emergencyIn(mine.session());
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(*after == resolved);
}

void EmergencyCommandsTest::taskEditReplayedAfterTheTaskWasDeletedConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    ResponseTask task = floodedBasement();
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask described = task;
    described.setDescription("Water in the basement and the garage");

    QVERIFY(theirs.session().commit(DeleteTaskCommand::fromSeen(task)).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine.session(), task, described).status == CommitStatus::Applied);
    QVERIFY(emergencyIn(mine.session()).findTask(basementId()));

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n"
                                   "- description: Water in the basement and the garage"));
    QVERIFY(!emergencyIn(mine.session()).findTask(basementId()));
    QVERIFY(emergencyIn(mine.session()) == emergencyIn(theirs.session()));
}

void EmergencyCommandsTest::taskEditsToDifferentValuesBothApply()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    ResponseTask task = floodedBasement();
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    shareWard(mine, theirs, TestWard::withResponse(withTeam, TestWard::stormWith({}, {task})));
    ResponseTask described = task;
    described.setDescription("Water in the basement and the garage");
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toTeam(TestWard::setupTeam()));

    QVERIFY(commitTaskEdit(theirs.session(), task, described).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine.session(), task, assigned).status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = emergencyIn(mine.session());
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QCOMPARE(after->description(), QString("Water in the basement and the garage"));
    QVERIFY(after->assignment() == TaskAssignment::toTeam(TestWard::setupTeam()));
    QCOMPARE(after->category(), QString("Flooding"));
    QVERIFY(!after->isResolved());
}

void EmergencyCommandsTest::oneTaskValueSetDifferentlyConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    ResponseTask task = floodedBasement();
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask garage = task;
    garage.setDescription("Water in the garage");
    ResponseTask cellar = task;
    cellar.setDescription("Water in the cellar");

    QVERIFY(commitTaskEdit(theirs.session(), task, garage).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine.session(), task, cellar).status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n- description: Water in the cellar"));
    EmergencyResponse storm = emergencyIn(mine.session());
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QCOMPARE(after->description(), QString("Water in the garage"));
}

void EmergencyCommandsTest::assignmentToAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    ResponseTask task = floodedBasement();
    shareWard(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWith({}, {task})));
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toPerson(TestWard::alice()));

    QVERIFY(removeAlice(theirs.session()).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine.session(), task, assigned).status == CommitStatus::Applied);
    EmergencyResponse beforeReplay = emergencyIn(mine.session());
    const ResponseTask* mineAssigned = beforeReplay.findTask(basementId());
    QVERIFY(mineAssigned);
    QVERIFY(mineAssigned->assignment() == assigned.assignment());

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.session().document().findPersonById(TestWard::aliceId()).has_value());
    EmergencyResponse storm = emergencyIn(mine.session());
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(!after->isAssigned());
}

void EmergencyCommandsTest::reassigningReplacesTheWholeAssignmentAndUndoRestoresTheNotification()
{
    TestClient client("Laptop");
    ResponseTask notified = floodedBasement();
    TaskAssignment toSetup = TaskAssignment::toTeam(TestWard::setupTeam());
    toSetup.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 40)));
    notified.setAssignment(toSetup);
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    client.createWard(TestWard::withResponse(withTeam, TestWard::stormWith({}, {notified})));
    EmergencyResponse opened = emergencyIn(client.session());
    const ResponseTask* before = opened.findTask(basementId());
    QVERIFY(before);
    QVERIFY(before->isNotified());
    ResponseTask reassigned = notified;
    reassigned.setAssignment(TaskAssignment::toPerson(TestWard::carol()));

    CommitResult result = commitTaskEdit(client.session(), notified, reassigned);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Flooding task for Smith, Bob & Alice", {"assigned to: Jones, Carol"}));
    EmergencyResponse storm = emergencyIn(client.session());
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(after->assignment()->personId() == TestWard::carolId());
    QVERIFY(!after->assignment()->teamId().has_value());
    QVERIFY(!after->isNotified());

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    EmergencyResponse undone = emergencyIn(client.session());
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
    TestClient client("Laptop");
    client.createWard(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jones(), 9, 0, ContactOutcome::UnableToReach, "No answer")},
                            {})));
    QVERIFY(statusIn(client.session(), TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NotContacted);

    QVERIFY(addAttempt(client.session(), phoned("attempt-smith", TestWard::smith(), 10, 0, ContactOutcome::OK, "Fine")).status
            == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::OK);

    QVERIFY(client.session().commit(AddTaskCommand::fromNew(floodedBasement())).status == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);

    ResponseTask unresolved = floodedBasement();
    ResponseTask resolved = unresolved;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    QVERIFY(commitTaskEdit(client.session(), unresolved, resolved).status == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::OK);

    // Undo takes each change back: the task reopens, then goes, then the attempt goes.
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::OK);
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(statusIn(client.session(), TestWard::smithId()) == EffectiveContactStatus::NotContacted);

    // The other family was left alone, and the table equals a rebuild from the emergency.
    QVERIFY(statusIn(client.session(), TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QHash<FamilyId, EffectiveContactStatus> rebuilt = emergencyIn(client.session()).statusByFamily();
    QCOMPARE(rebuilt.size(), qsizetype(1));
    QVERIFY(rebuilt.value(TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
}
