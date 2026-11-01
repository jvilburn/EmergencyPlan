#pragma once

#include <QObject>

/// Emergency responses are a keyed collection changed only by commands: responders working
/// apart all land, a family's status is its latest outcome whatever order replay lands
/// attempts in, ending writes the report and removes the response, and task edits are
/// judged value by value.
class EmergencyCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyCommandsTest(QObject* parent);

private slots:
    void twoRespondersOfflineLandEveryAttempt();
    void twoRespondersOnAFamilyWithNoRecordLandInOneRecord();
    void differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst();
    void twoOfflineStartsMakeTwoEvents();
    void startAndNewCategoryAreNotUndoable();
    void endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheResponse();
    void attemptsReplayedOntoAnEndedResponseAreInTheToRedoText();
    void endReplayedAfterAnotherAttemptConflictsAndLeavesTheResponse();
    void undoingAnEndRestoresAnEqualResponseAndLeavesTheReportFile();
    void endWhoseReportCannotBeWrittenEndsNothing();
    void endingAResponseAlreadyGoneWritesNothing();
    void taskDeletionReplayedAfterAnotherResolvedItConflicts();
    void taskEditsToDifferentValuesBothApply();
    void oneTaskValueSetDifferentlyConflicts();
    void assignmentToAPersonReplayedAfterTheyWereRemovedConflicts();
    void changingTheAssigneeClearsTheNotification();
    void statisticsCountFamiliesWithoutARecordAsNotContacted();
};
