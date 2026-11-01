#include "EmergencyCommandsTest.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "PdfExtractor.h"
#include "TestSession.h"
#include "TestWard.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace
{

EmergencyResponse stormIn(const WardSession& session)
{
    return session.document().findEmergencyResponseById(TestWard::stormId()).value();
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

/// An attempt Bob made by phone on the family at hour:minute on 1 October 2026.
ContactAttempt phoned(const QString& id, const FamilyId& familyId, int hour, int minute, ContactOutcome outcome,
                      const QString& notes)
{
    return TestWard::attempt(id, familyId, TestWard::octoberFirst(hour, minute), ContactMethod::Phone, TestWard::bobId(),
                             outcome, notes);
}

/// An attempt on the family made by a button at hour:minute: an outcome and nothing else.
ContactAttempt marked(const QString& id, const FamilyId& familyId, int hour, int minute, ContactOutcome outcome)
{
    return TestWard::attempt(id, familyId, TestWard::octoberFirst(hour, minute), std::nullopt, std::nullopt, outcome,
                             QString());
}

TaskId basementId()
{
    return TaskId::fromString("task-basement");
}

/// The Flooding task on the Smith family, created at 10:30.
ResponseTask floodedBasement()
{
    return TestWard::task(basementId().toString(), TestWard::smithId(), TestWard::octoberFirst(10, 30), "Flooding",
                          "Water in the basement");
}

/// The Storm response with another id and name.
EmergencyResponse otherResponse(const QString& id, const QString& name)
{
    QJsonObject json = TestWard::storm().toJson();
    json["id"] = id;
    json["name"] = name;
    return EmergencyResponse::fromJson(json);
}

/// The family's status in the Storm response, from the session's document.
EffectiveContactStatus statusIn(const WardSession& session, const FamilyId& familyId)
{
    return session.document().emergencyStatus(TestWard::stormId(), familyId);
}

CommitResult addAttempt(WardSession& session, const ContactAttempt& attempt)
{
    return session.commit(AddContactAttemptCommand::fromNew(TestWard::stormId(), attempt, session.document()));
}

CommitResult commitTaskEdit(WardSession& session, const ResponseTask& opened, const ResponseTask& saved)
{
    return session.commit(UpdateTaskCommand::fromEdit(TestWard::stormId(), opened, saved, session.document()));
}

/// The Smith family with Alice removed through the family edit, as another user would.
CommitResult removeAlice(WardSession& session)
{
    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    return session.commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice));
}

void openBoth(WardSession& mine, WardSession& theirs, const Document& document)
{
    mine.open(document);
    theirs.open(document);
}

/// The text of every page of the PDF at path, a line per text field, in reading order.
QString pdfText(const QString& path)
{
    PdfExtractor extractor;
    if (!extractor.open(path))
    {
        return QString();
    }
    QStringList lines;
    for (int page = 0; page < extractor.pageCount(); ++page)
    {
        for (const PdfTextField& field : extractor.textFields(page))
        {
            lines.append(field.text());
        }
    }
    return lines.join("\n");
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

    QVERIFY(addAttempt(theirs, phoned("attempt-their-smith", TestWard::smithId(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(theirs, phoned("attempt-their-jones", TestWard::jonesId(), 10, 10, ContactOutcome::OK, "Carol is fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-my-smith", TestWard::smithId(), 10, 20, ContactOutcome::OK, "Visited, roof damage"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-my-jones", TestWard::jonesId(), 10, 30, ContactOutcome::OK, "Texted back"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(storm.contactAttempts().size(), qsizetype(4));
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"Visited, roof damage", "No answer"}));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Texted back", "Carol is fine"}));
}

void EmergencyCommandsTest::twoRespondersFirstAttemptsOnOneFamilyBothLand()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(stormIn(mine).contactAttempts().isEmpty());

    QVERIFY(addAttempt(theirs, phoned("attempt-theirs", TestWard::jonesId(), 10, 0, ContactOutcome::UnableToReach, "Phoned Carol"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-mine", TestWard::jonesId(), 10, 5, ContactOutcome::OK, "Visited Carol"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(storm.contactAttempts().size(), qsizetype(2));
    QCOMPARE(notesOf(storm, TestWard::jonesId()), QStringList({"Visited Carol", "Phoned Carol"}));
    QVERIFY(storm.statusByFamily().value(TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(mine.document().emergencyStatus(TestWard::stormId(), TestWard::jonesId()) == EffectiveContactStatus::OK);
    QVERIFY(theirs.document().emergencyStatus(TestWard::stormId(), TestWard::jonesId())
            == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-ok", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Fine at ten"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-unreachable", TestWard::smithId(), 10, 5, ContactOutcome::UnableToReach,
                                    "No answer at five past"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.document();
    Document mineAlone = mine.document();

    QVERIFY(mine.replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.document() == theirs.document());
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(notesOf(storm, TestWard::smithId()), QStringList({"No answer at five past", "Fine at ten"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(mine.document().emergencyStatus(TestWard::stormId(), TestWard::smithId())
            == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::attemptsAtOneTimeAreOrderedByIdWhicheverReplaysFirst()
{
    // Same timestamp, so the greater id comes first: "attempt-b" before "attempt-a".
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-a", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Fine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-b", TestWard::smithId(), 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.document();
    Document mineAlone = mine.document();

    QVERIFY(mine.replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.document() == theirs.document());
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(attemptIdsOf(storm, TestWard::smithId()), QStringList({"attempt-b", "attempt-a"}));
    QVERIFY(storm.statusByFamily().value(TestWard::smithId()) == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::undoingAnAddedAttemptRestoresTheResponseAndRedoAddsItAgain()
{
    TestSession session;
    session.open(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jonesId(), 9, 0, ContactOutcome::OK, "Fine")}, {})));
    EmergencyResponse before = stormIn(session);
    ContactAttempt first = phoned("attempt-smith", TestWard::smithId(), 10, 0, ContactOutcome::OK, "All safe");

    QVERIFY(addAttempt(session, first).status == CommitStatus::Applied);
    QCOMPARE(attemptIdsOf(stormIn(session), TestWard::smithId()), QStringList({"attempt-smith"}));

    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(stormIn(session) == before);

    QVERIFY(session.redo().status == CommitStatus::Applied);
    EmergencyResponse redone = stormIn(session);
    const ContactAttempt* again = redone.findContactAttempt(first.id());
    QVERIFY(again);
    QVERIFY(*again == first);
    QCOMPARE(redone.contactAttempts().size(), qsizetype(2));
}

void EmergencyCommandsTest::twoOfflineStartsMakeTwoEvents()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::document());
    EmergencyResponse flood = otherResponse("response-flood", "Flood");

    QVERIFY(theirs.commit(StartEmergencyCommand::fromNew(flood)).status == CommitStatus::Applied);
    ContactAttempt dry = phoned("attempt-flood", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Dry");
    QVERIFY(theirs.commit(AddContactAttemptCommand::fromNew(flood.id(), dry, theirs.document())).status == CommitStatus::Applied);
    QVERIFY(mine.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-storm", TestWard::smithId(), 10, 5, ContactOutcome::OK, "Roof intact"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(mine.document().emergencyResponses().size(), qsizetype(2));
    QCOMPARE(notesOf(stormIn(mine), TestWard::smithId()), QStringList({"Roof intact"}));
    std::optional<EmergencyResponse> floodAfter = mine.document().findEmergencyResponseById(flood.id());
    QVERIFY(floodAfter.has_value());
    QCOMPARE(floodAfter->name(), QString("Flood"));
    QCOMPARE(notesOf(*floodAfter, TestWard::smithId()), QStringList({"Dry"}));
}

void EmergencyCommandsTest::startAndNewCategoryAreNotUndoable()
{
    TestSession session;
    session.open(TestWard::document());

    QVERIFY(session.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(stormIn(session) == TestWard::storm());
    QVERIFY(!session.canUndo());

    QVERIFY(session.commit(AddTaskCategoryCommand::fromNew(TestWard::stormId(), "Power lines")).status == CommitStatus::Applied);
    QStringList categories = stormIn(session).taskCategories();
    QCOMPARE(categories.size(), TestWard::storm().taskCategories().size() + 1);
    QCOMPARE(categories.last(), QString("Power lines"));
    QVERIFY(!session.canUndo());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
}

void EmergencyCommandsTest::endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheResponse()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    Document ward = TestWard::withFamily(TestWard::withTeam(TestWard::document(), TestWard::setupTeam()), TestWard::lee(2010));
    session.open(TestWard::withResponse(ward, TestWard::storm()));

    // Smith: two phone attempts, the later with a note long enough to wrap, and a task for
    // the Setup team. Jones: a button mark, and a task given to Alice, notified and
    // resolved. Lee: nothing, so not contacted.
    QString longNote = "Answered at eleven. Everyone is safe, the roof lost a few shingles over the porch, the "
                       "basement has standing water a few inches deep, and they asked that anyone coming by use "
                       "the side entrance because the front steps are blocked by a fallen limb near the gatepost";
    QVERIFY(addAttempt(session, phoned("attempt-early", TestWard::smithId(), 9, 0, ContactOutcome::UnableToReach, "No answer at nine"))
                .status == CommitStatus::Applied);
    QVERIFY(addAttempt(session, phoned("attempt-late", TestWard::smithId(), 11, 0, ContactOutcome::OK, longNote)).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(session, marked("attempt-marked", TestWard::jonesId(), 9, 30, ContactOutcome::OK)).status
            == CommitStatus::Applied);
    ResponseTask basement = floodedBasement();
    basement.setAssignment(TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));
    QVERIFY(session.commit(AddTaskCommand::fromNew(TestWard::stormId(), basement, session.document())).status
            == CommitStatus::Applied);
    ResponseTask tree = TestWard::task("task-tree", TestWard::jonesId(), TestWard::octoberFirst(10, 45), "Tree removal",
                                       "Oak across the driveway");
    TaskAssignment toAlice = TaskAssignment::toPerson(TestWard::aliceId(), QString());
    toAlice.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 50)));
    tree.setAssignment(toAlice);
    tree.setResolution(TestWard::resolution("Cut and cleared", TestWard::octoberFirst(12, 0)));
    QVERIFY(session.commit(AddTaskCommand::fromNew(TestWard::stormId(), tree, session.document())).status == CommitStatus::Applied);
    QVERIFY(removeAlice(session).status == CommitStatus::Applied);

    EndResult result = session.endEmergency(TestWard::stormId(), path);

    QVERIFY(result == EndResult::Ended);
    QVERIFY(!session.document().findEmergencyResponseById(TestWard::stormId()).has_value());
    QCOMPARE(session.journal().entries().last().command["type"].toString(), QString(EndEmergencyCommand::TypeName));
    QString text = pdfText(path);
    QVERIFY2(text.contains("No answer at nine"), qPrintable(text));
    QVERIFY(text.contains("gatepost"));
    QVERIFY(text.indexOf("No answer at nine") < text.indexOf("gatepost"));
    QVERIFY(text.contains("Smith, Bob"));
    QVERIFY(text.contains("Marked OK: 1"));
    QVERIFY(text.contains("Water in the basement"));
    QVERIFY(text.contains("Bring the pump"));
    QVERIFY(text.contains("Oak across the driveway"));
    QVERIFY(text.contains("Sent the address"));
    QVERIFY(text.contains("Cut and cleared"));
    QVERIFY(text.contains("(removed)"));
    QVERIFY(text.contains("Families Not Contacted (1)"));
    QVERIFY(text.contains("Lee, Ken & Mia"));
}

void EmergencyCommandsTest::attemptsReplayedOntoAnEndedResponseAreInTheToRedoText()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(theirs.commit(EndEmergencyCommand::fromReported(stormIn(theirs))).status == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, phoned("attempt-smith", TestWard::smithId(), 10, 0, ContactOutcome::OK, "All safe")).status
            == CommitStatus::Applied);
    QVERIFY(addAttempt(mine, marked("attempt-jones", TestWard::jonesId(), 10, 5, ContactOutcome::UnableToReach)).status
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
    QVERIFY(mine.document().emergencyResponses().isEmpty());
}

void EmergencyCommandsTest::endReplayedAfterAnotherAttemptConflictsAndLeavesTheResponse()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(addAttempt(theirs, phoned("attempt-theirs", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Phoned the Smiths"))
                .status == CommitStatus::Applied);
    QVERIFY(mine.commit(EndEmergencyCommand::fromReported(stormIn(mine))).status == CommitStatus::Applied);
    QVERIFY(mine.document().emergencyResponses().isEmpty());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("End emergency: Storm (stopped here)"));
    QVERIFY(stormIn(mine) == stormIn(theirs));
    QCOMPARE(notesOf(stormIn(mine), TestWard::smithId()), QStringList({"Phoned the Smiths"}));
}

void EmergencyCommandsTest::undoingAnEndRestoresAnEqualResponseKeepsTheReportAndRedoEndsItAgain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    EmergencyResponse storm = TestWard::stormWith({phoned("attempt-smith", TestWard::smithId(), 10, 0, ContactOutcome::OK, "All safe")},
                                                  {floodedBasement()});
    session.open(TestWard::withResponse(TestWard::document(), storm));

    QVERIFY(session.endEmergency(TestWard::stormId(), path) == EndResult::Ended);
    QVERIFY(session.document().emergencyResponses().isEmpty());
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(stormIn(session) == storm);
    QVERIFY(QFile::exists(path));

    QVERIFY(session.redo().status == CommitStatus::Applied);

    QVERIFY(session.document().emergencyResponses().isEmpty());
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));
}

void EmergencyCommandsTest::endWhoseReportCannotBeWrittenEndsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("missing/storm.pdf");
    TestSession session;
    session.open(TestWard::withResponse(TestWard::document(), TestWard::storm()));

    QVERIFY(session.endEmergency(TestWard::stormId(), path) == EndResult::ReportFailed);

    QVERIFY(stormIn(session) == TestWard::storm());
    QVERIFY(session.journal().isEmpty());
    QVERIFY(!session.canUndo());
    QVERIFY(!QFile::exists(path));
}

void EmergencyCommandsTest::endingAResponseAlreadyGoneWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    session.open(TestWard::document());

    QVERIFY(session.endEmergency(TestWard::stormId(), path) == EndResult::AlreadyEnded);

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
    QVERIFY(mine.commit(DeleteTaskCommand::fromSeen(TestWard::stormId(), task, mine.document())).status
            == CommitStatus::Applied);
    QVERIFY(!stormIn(mine).findTask(basementId()));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete Flooding task for Smith, Bob & Alice (stopped here)"));
    EmergencyResponse storm = stormIn(mine);
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

    QVERIFY(theirs.commit(DeleteTaskCommand::fromSeen(TestWard::stormId(), task, theirs.document())).status
            == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, described).status == CommitStatus::Applied);
    QVERIFY(stormIn(mine).findTask(basementId()));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n"
                                   "- description: Water in the basement and the garage"));
    QVERIFY(!stormIn(mine).findTask(basementId()));
    QVERIFY(stormIn(mine) == stormIn(theirs));
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
    assigned.setAssignment(TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));

    QVERIFY(commitTaskEdit(theirs, task, described).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = stormIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QCOMPARE(after->description(), QString("Water in the basement and the garage"));
    QVERIFY(after->assignment() == TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));
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
    EmergencyResponse storm = stormIn(mine);
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
    assigned.setAssignment(TaskAssignment::toPerson(TestWard::aliceId(), QString()));

    QVERIFY(removeAlice(theirs).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);
    EmergencyResponse beforeReplay = stormIn(mine);
    const ResponseTask* mineAssigned = beforeReplay.findTask(basementId());
    QVERIFY(mineAssigned);
    QVERIFY(mineAssigned->assignment() == assigned.assignment());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.document().findPersonById(TestWard::aliceId()).has_value());
    EmergencyResponse storm = stormIn(mine);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(!after->isAssigned());
}

void EmergencyCommandsTest::reassigningReplacesTheWholeAssignmentAndUndoRestoresTheNotification()
{
    TestSession session;
    ResponseTask notified = floodedBasement();
    TaskAssignment toSetup = TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump");
    toSetup.setNotification(TestWard::notification(ContactMethod::Text, "Sent the address", TestWard::octoberFirst(10, 40)));
    notified.setAssignment(toSetup);
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    session.open(TestWard::withResponse(withTeam, TestWard::stormWith({}, {notified})));
    EmergencyResponse opened = stormIn(session);
    const ResponseTask* before = opened.findTask(basementId());
    QVERIFY(before);
    QVERIFY(before->isNotified());
    ResponseTask reassigned = notified;
    reassigned.setAssignment(TaskAssignment::toPerson(TestWard::carolId(), QString()));

    CommitResult result = commitTaskEdit(session, notified, reassigned);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Flooding task for Smith, Bob & Alice", {"assigned to: Jones, Carol"}));
    EmergencyResponse storm = stormIn(session);
    const ResponseTask* after = storm.findTask(basementId());
    QVERIFY(after);
    QVERIFY(after->assignment()->personId() == TestWard::carolId());
    QVERIFY(!after->assignment()->teamId().has_value());
    QVERIFY(!after->isNotified());

    QVERIFY(session.undo().status == CommitStatus::Applied);

    EmergencyResponse undone = stormIn(session);
    const ResponseTask* restored = undone.findTask(basementId());
    QVERIFY(restored);
    QVERIFY(*restored == notified);
}

void EmergencyCommandsTest::statisticsCountEveryDirectoryFamilyAndEveryRemovedFamilyTheResponseNames()
{
    // Smith: OK, then a visit with no outcome. Jones: OK, with an unresolved task. A family
    // no longer in the directory: Unable to Reach. Lee: no attempt or task.
    FamilyId gone = FamilyId::fromString("family-gone");
    EmergencyResponse response = TestWard::stormWith(
        {phoned("attempt-smith-ok", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Fine"),
         TestWard::attempt("attempt-smith-visit", TestWard::smithId(), TestWard::octoberFirst(11, 0), ContactMethod::Visit,
                           TestWard::bobId(), std::nullopt, "Left a note"),
         phoned("attempt-jones-ok", TestWard::jonesId(), 10, 5, ContactOutcome::OK, "Fine"),
         phoned("attempt-gone", gone, 10, 15, ContactOutcome::UnableToReach, "Moved away")},
        {TestWard::task("task-jones", TestWard::jonesId(), TestWard::octoberFirst(10, 10), "Generator", "Needs fuel")});
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
    QVERIFY(withStorm.emergencyStatus(TestWard::stormId(), TestWard::jonesId()) == EffectiveContactStatus::NeedsHelp);
    QVERIFY(withStorm.emergencyStatus(TestWard::stormId(), FamilyId::fromString("family-lee"))
            == EffectiveContactStatus::NotContacted);
    QVERIFY(withStorm.emergencyStatus(EmergencyResponseId::fromString("response-other"), TestWard::jonesId())
            == EffectiveContactStatus::NotContacted);
}

void EmergencyCommandsTest::statusTableFollowsEachChangeToOneFamily()
{
    TestSession session;
    session.open(TestWard::withResponse(
        TestWard::document(),
        TestWard::stormWith({phoned("attempt-jones", TestWard::jonesId(), 9, 0, ContactOutcome::UnableToReach, "No answer")},
                            {})));
    QVERIFY(statusIn(session, TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::NotContacted);

    QVERIFY(addAttempt(session, phoned("attempt-smith", TestWard::smithId(), 10, 0, ContactOutcome::OK, "Fine")).status
            == CommitStatus::Applied);
    QVERIFY(statusIn(session, TestWard::smithId()) == EffectiveContactStatus::OK);

    QVERIFY(session.commit(AddTaskCommand::fromNew(TestWard::stormId(), floodedBasement(), session.document())).status
            == CommitStatus::Applied);
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

    // The other family was left alone, and the table equals a rebuild from the response.
    QVERIFY(statusIn(session, TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
    QHash<FamilyId, EffectiveContactStatus> rebuilt = stormIn(session).statusByFamily();
    QCOMPARE(rebuilt.size(), qsizetype(1));
    QVERIFY(rebuilt.value(TestWard::jonesId()) == EffectiveContactStatus::UnableToReach);
}
