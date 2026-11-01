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

/// The family's record in response; throws std::bad_optional_access when there is none.
FamilyResponseRecord recordIn(const EmergencyResponse& response, const FamilyId& familyId)
{
    const FamilyResponseRecord* record = response.findRecord(familyId);
    return (record ? std::optional<FamilyResponseRecord>(*record) : std::nullopt).value();
}

/// Each attempt's notes, in the record's order.
QStringList notesOf(const FamilyResponseRecord& record)
{
    QStringList notes;
    for (const ContactAttempt& attempt : record.contactAttempts())
    {
        notes.append(attempt.notes());
    }
    return notes;
}

/// An attempt Bob made by phone at hour:minute on 1 October 2026.
ContactAttempt phoned(const QString& id, int hour, int minute, ContactOutcome outcome, const QString& notes)
{
    return TestWard::attempt(id, TestWard::octoberFirst(hour, minute), ContactMethod::Phone, TestWard::bobId(),
                             outcome, notes);
}

/// The Flooding task on the Smith family, created at 10:30.
ResponseTask floodedBasement()
{
    return TestWard::task("task-basement", TestWard::octoberFirst(10, 30), "Flooding", "Water in the basement");
}

/// The basement task in the Smith family's Storm record; throws std::bad_optional_access
/// when there is none.
ResponseTask basementTaskIn(const WardSession& session)
{
    FamilyResponseRecord record = recordIn(stormIn(session), TestWard::smithId());
    const ResponseTask* task = record.findTask(floodedBasement().id());
    return (task ? std::optional<ResponseTask>(*task) : std::nullopt).value();
}

/// The Storm response with another id and name.
EmergencyResponse otherResponse(const QString& id, const QString& name)
{
    QJsonObject json = TestWard::storm().toJson();
    json["id"] = id;
    json["name"] = name;
    return EmergencyResponse::fromJson(json);
}

CommitResult recordAttempt(WardSession& session, const Family& family, const ContactAttempt& attempt)
{
    FamilyRecordTarget target = FamilyRecordTarget::forFamily(TestWard::stormId(), family);
    return session.commit(RecordContactAttemptCommand::fromNew(target, attempt, session.document()));
}

CommitResult commitTaskEdit(WardSession& session, const ResponseTask& opened, const ResponseTask& saved)
{
    return session.commit(UpdateTaskCommand::fromEdit(TestWard::smithInStorm(), opened, saved, session.document()));
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

    QVERIFY(recordAttempt(theirs, TestWard::smith(), phoned("attempt-their-smith", 10, 0, ContactOutcome::UnableToReach, "No answer"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(theirs, TestWard::jones(), phoned("attempt-their-jones", 10, 10, ContactOutcome::OK, "Carol is fine"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::smith(), phoned("attempt-my-smith", 10, 20, ContactOutcome::OK, "Visited, roof damage"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::jones(), phoned("attempt-my-jones", 10, 30, ContactOutcome::OK, "Texted back"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(storm.familyRecords().size(), qsizetype(2));
    QCOMPARE(notesOf(recordIn(storm, TestWard::smithId())), QStringList({"Visited, roof damage", "No answer"}));
    QCOMPARE(notesOf(recordIn(storm, TestWard::jonesId())), QStringList({"Texted back", "Carol is fine"}));
}

void EmergencyCommandsTest::twoRespondersOnAFamilyWithNoRecordLandInOneRecord()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(stormIn(mine).familyRecords().isEmpty());

    QVERIFY(recordAttempt(theirs, TestWard::jones(), phoned("attempt-theirs", 10, 0, ContactOutcome::OK, "Phoned Carol"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::jones(), phoned("attempt-mine", 10, 5, ContactOutcome::OK, "Visited Carol"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    EmergencyResponse storm = stormIn(mine);
    QCOMPARE(storm.familyRecords().size(), qsizetype(1));
    FamilyResponseRecord record = recordIn(storm, TestWard::jonesId());
    QCOMPARE(record.displayName(), QString("Jones, Carol"));
    QCOMPARE(record.address(), QString("4 Oak Ave, Provo, UT"));
    QCOMPARE(notesOf(record), QStringList({"Visited Carol", "Phoned Carol"}));
}

void EmergencyCommandsTest::differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(recordAttempt(theirs, TestWard::smith(), phoned("attempt-ok", 10, 0, ContactOutcome::OK, "Fine at ten"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::smith(), phoned("attempt-unreachable", 10, 5, ContactOutcome::UnableToReach,
                                                          "No answer at five past"))
                .status == CommitStatus::Applied);
    Document theirsAlone = theirs.document();
    Document mineAlone = mine.document();

    QVERIFY(mine.replayOnto(theirsAlone).status == ReplayStatus::Replayed);
    QVERIFY(theirs.replayOnto(mineAlone).status == ReplayStatus::Replayed);

    QVERIFY(mine.document() == theirs.document());
    FamilyResponseRecord record = recordIn(stormIn(mine), TestWard::smithId());
    QCOMPARE(notesOf(record), QStringList({"No answer at five past", "Fine at ten"}));
    QVERIFY(record.effectiveStatus() == EffectiveContactStatus::UnableToReach);
}

void EmergencyCommandsTest::twoOfflineStartsMakeTwoEvents()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::document());
    EmergencyResponse flood = otherResponse("response-flood", "Flood");

    QVERIFY(theirs.commit(StartEmergencyCommand::fromNew(flood)).status == CommitStatus::Applied);
    FamilyRecordTarget smithInFlood = FamilyRecordTarget::forFamily(flood.id(), TestWard::smith());
    QVERIFY(theirs.commit(RecordContactAttemptCommand::fromNew(smithInFlood, phoned("attempt-flood", 10, 0, ContactOutcome::OK, "Dry"),
                                                               theirs.document()))
                .status == CommitStatus::Applied);
    QVERIFY(mine.commit(StartEmergencyCommand::fromNew(TestWard::storm())).status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::smith(), phoned("attempt-storm", 10, 5, ContactOutcome::OK, "Roof intact"))
                .status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(mine.document().emergencyResponses().size(), qsizetype(2));
    QCOMPARE(notesOf(recordIn(stormIn(mine), TestWard::smithId())), QStringList({"Roof intact"}));
    EmergencyResponse floodAfter = mine.document().findEmergencyResponseById(flood.id()).value();
    QCOMPARE(floodAfter.name(), QString("Flood"));
    QCOMPARE(notesOf(recordIn(floodAfter, TestWard::smithId())), QStringList({"Dry"}));
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
    session.open(TestWard::withResponse(TestWard::withTeam(TestWard::document(), TestWard::setupTeam()), TestWard::storm()));

    QVERIFY(recordAttempt(session, TestWard::smith(), phoned("attempt-early", 9, 0, ContactOutcome::UnableToReach, "No answer at nine"))
                .status == CommitStatus::Applied);
    QVERIFY(recordAttempt(session, TestWard::smith(), phoned("attempt-late", 11, 0, ContactOutcome::OK, "Answered at eleven"))
                .status == CommitStatus::Applied);
    ContactAttempt marked = TestWard::attempt("attempt-marked", TestWard::octoberFirst(9, 30), std::nullopt, std::nullopt,
                                              ContactOutcome::OK, QString());
    QVERIFY(recordAttempt(session, TestWard::jones(), marked).status == CommitStatus::Applied);
    ResponseTask basement = floodedBasement();
    basement.setAssignment(TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));
    QVERIFY(session.commit(AddTaskCommand::fromNew(TestWard::smithInStorm(), basement, session.document())).status
            == CommitStatus::Applied);
    ResponseTask tree = TestWard::task("task-tree", TestWard::octoberFirst(10, 45), "Tree removal", "Oak across the driveway");
    FamilyRecordTarget jonesInStorm = FamilyRecordTarget::forFamily(TestWard::stormId(), TestWard::jones());
    QVERIFY(session.commit(AddTaskCommand::fromNew(jonesInStorm, tree, session.document())).status == CommitStatus::Applied);

    EndResult result = session.endEmergency(TestWard::stormId(), path);

    QVERIFY(result == EndResult::Ended);
    QVERIFY(!session.document().findEmergencyResponseById(TestWard::stormId()).has_value());
    QCOMPARE(session.journal().entries().last().command["type"].toString(), QString(EndEmergencyCommand::TypeName));
    QString text = pdfText(path);
    QVERIFY2(text.contains("No answer at nine"), qPrintable(text));
    QVERIFY(text.contains("Answered at eleven"));
    QVERIFY(text.indexOf("No answer at nine") < text.indexOf("Answered at eleven"));
    QVERIFY(text.contains("Marked OK: 1"));
    QVERIFY(text.contains("Water in the basement"));
    QVERIFY(text.contains("Assigned to: Setup - Bring the pump"));
    QVERIFY(text.contains("Oak across the driveway"));
}

void EmergencyCommandsTest::attemptsReplayedOntoAnEndedResponseAreInTheToRedoText()
{
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::storm()));
    QVERIFY(theirs.commit(EndEmergencyCommand::fromReported(stormIn(theirs))).status == CommitStatus::Applied);
    QVERIFY(recordAttempt(mine, TestWard::smith(), phoned("attempt-smith", 10, 0, ContactOutcome::OK, "All safe")).status
            == CommitStatus::Applied);
    ContactAttempt marked = TestWard::attempt("attempt-jones", TestWard::octoberFirst(10, 5), std::nullopt, std::nullopt,
                                              ContactOutcome::UnableToReach, QString());
    QVERIFY(recordAttempt(mine, TestWard::jones(), marked).status == CommitStatus::Applied);

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
    QVERIFY(recordAttempt(theirs, TestWard::smith(), phoned("attempt-theirs", 10, 0, ContactOutcome::OK, "Phoned the Smiths"))
                .status == CommitStatus::Applied);
    QVERIFY(mine.commit(EndEmergencyCommand::fromReported(stormIn(mine))).status == CommitStatus::Applied);
    QVERIFY(mine.document().emergencyResponses().isEmpty());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("End emergency: Storm (stopped here)"));
    QVERIFY(stormIn(mine) == stormIn(theirs));
    QCOMPARE(notesOf(recordIn(stormIn(mine), TestWard::smithId())), QStringList({"Phoned the Smiths"}));
}

void EmergencyCommandsTest::undoingAnEndRestoresAnEqualResponseAndLeavesTheReportFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("storm.pdf");
    TestSession session;
    EmergencyResponse storm = TestWard::stormWithSmithRecord({phoned("attempt-smith", 10, 0, ContactOutcome::OK, "All safe")},
                                                             {floodedBasement()});
    session.open(TestWard::withResponse(TestWard::document(), storm));

    QVERIFY(session.endEmergency(TestWard::stormId(), path) == EndResult::Ended);
    QVERIFY(session.document().emergencyResponses().isEmpty());
    QCOMPARE(session.undoHeading(), QString("End emergency: Storm"));

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(stormIn(session) == storm);
    QVERIFY(QFile::exists(path));
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
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWithSmithRecord({}, {task})));
    ResponseTask resolved = task;
    resolved.setResolution(TaskResolution::create("Pumped out"));

    QVERIFY(commitTaskEdit(theirs, task, resolved).status == CommitStatus::Applied);
    QVERIFY(mine.commit(DeleteTaskCommand::fromSeen(TestWard::smithInStorm(), task, mine.document())).status
            == CommitStatus::Applied);
    QVERIFY(!recordIn(stormIn(mine), TestWard::smithId()).findTask(task.id()));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete Flooding task for Smith, Bob & Alice (stopped here)"));
    ResponseTask after = basementTaskIn(mine);
    QVERIFY(after.isResolved());
    QCOMPARE(after.resolution()->notes(), QString("Pumped out"));
}

void EmergencyCommandsTest::taskEditsToDifferentValuesBothApply()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    openBoth(mine, theirs, TestWard::withResponse(withTeam, TestWard::stormWithSmithRecord({}, {task})));
    ResponseTask described = task;
    described.setDescription("Water in the basement and the garage");
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));

    QVERIFY(commitTaskEdit(theirs, task, described).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    ResponseTask after = basementTaskIn(mine);
    QCOMPARE(after.description(), QString("Water in the basement and the garage"));
    QVERIFY(after.assignment() == TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump"));
    QCOMPARE(after.category(), QString("Flooding"));
    QVERIFY(!after.isResolved());
}

void EmergencyCommandsTest::oneTaskValueSetDifferentlyConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWithSmithRecord({}, {task})));
    ResponseTask garage = task;
    garage.setDescription("Water in the garage");
    ResponseTask cellar = task;
    cellar.setDescription("Water in the cellar");

    QVERIFY(commitTaskEdit(theirs, task, garage).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, cellar).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Flooding task for Smith, Bob & Alice (stopped here)\n- description: Water in the cellar"));
    QCOMPARE(basementTaskIn(mine).description(), QString("Water in the garage"));
}

void EmergencyCommandsTest::assignmentToAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestSession mine;
    TestSession theirs;
    ResponseTask task = floodedBasement();
    openBoth(mine, theirs, TestWard::withResponse(TestWard::document(), TestWard::stormWithSmithRecord({}, {task})));
    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    ResponseTask assigned = task;
    assigned.setAssignment(TaskAssignment::toPerson(TestWard::aliceId(), QString()));

    QVERIFY(theirs.commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice)).status == CommitStatus::Applied);
    QVERIFY(commitTaskEdit(mine, task, assigned).status == CommitStatus::Applied);
    QVERIFY(basementTaskIn(mine).assignment() == assigned.assignment());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.document().findPersonById(TestWard::aliceId()).has_value());
    QVERIFY(!basementTaskIn(mine).isAssigned());
}

void EmergencyCommandsTest::changingTheAssigneeClearsTheNotification()
{
    TestSession session;
    ResponseTask notified = floodedBasement();
    TaskAssignment toSetup = TaskAssignment::toTeam(TestWard::setupTeamId(), "Bring the pump");
    toSetup.setNotification(TaskNotification::create(ContactMethod::Text, "Sent the address"));
    notified.setAssignment(toSetup);
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    session.open(TestWard::withResponse(withTeam, TestWard::stormWithSmithRecord({}, {notified})));
    QVERIFY(basementTaskIn(session).isNotified());
    ResponseTask reassigned = notified;
    reassigned.setAssignment(TaskAssignment::toPerson(TestWard::carolId(), QString()));

    CommitResult result = commitTaskEdit(session, notified, reassigned);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Flooding task for Smith, Bob & Alice", {"assigned to: Jones, Carol"}));
    ResponseTask after = basementTaskIn(session);
    QVERIFY(after.assignment()->personId() == TestWard::carolId());
    QVERIFY(!after.assignment()->teamId().has_value());
    QVERIFY(!after.isNotified());

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(basementTaskIn(session) == notified);
}

void EmergencyCommandsTest::statisticsCountFamiliesWithoutARecordAsNotContacted()
{
    // Smith: OK, then a visit with no outcome. Jones: OK, with an unresolved task. A family
    // no longer in the directory: Unable to Reach. Lee: no record.
    EmergencyResponse response = TestWard::stormWithSmithRecord(
        {phoned("attempt-smith-ok", 10, 0, ContactOutcome::OK, "Fine"),
         TestWard::attempt("attempt-smith-visit", TestWard::octoberFirst(11, 0), ContactMethod::Visit, TestWard::bobId(),
                           std::nullopt, "Left a note")},
        {});
    FamilyResponseRecord jones = FamilyResponseRecord::create(TestWard::jonesId(), "Jones, Carol", "4 Oak Ave, Provo, UT");
    jones.insertContactAttempt(phoned("attempt-jones-ok", 10, 5, ContactOutcome::OK, "Fine"));
    jones.insertTask(TestWard::task("task-jones", TestWard::octoberFirst(10, 10), "Generator", "Needs fuel"));
    response.setRecord(jones);
    FamilyResponseRecord gone = FamilyResponseRecord::create(FamilyId::fromString("family-gone"), "Gone, Ann", "1 Main St");
    gone.insertContactAttempt(phoned("attempt-gone", 10, 15, ContactOutcome::UnableToReach, "Moved away"));
    response.setRecord(gone);
    Document document = TestWard::withFamily(TestWard::document(), TestWard::lee(2010));
    const QHash<FamilyId, Family>& families = document.families();
    QCOMPARE(families.size(), qsizetype(3));

    QCOMPARE(response.totalFamilies(families), 4);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::OK, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::NeedsHelp, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::UnableToReach, families), 1);
    QCOMPARE(response.countByStatus(EffectiveContactStatus::NotContacted, families), 1);
}
