#pragma once

#include <QObject>

/// The ward holds at most one emergency in progress, changed only by commands: responders
/// working apart all land in it, two offline starts become one emergency, a family's status
/// is its latest outcome whatever order replay lands attempts in, ending writes the report
/// and removes the emergency, and task edits are judged value by value.
class EmergencyCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyCommandsTest(QObject* parent);

private slots:
    void twoRespondersOfflineLandEveryAttempt();
    void twoRespondersFirstAttemptsOnOneFamilyBothLand();
    void differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst();
    void attemptsAtOneTimeAreOrderedByIdWhicheverReplaysFirst();
    void undoingAnAddedAttemptRestoresTheEmergencyAndRedoAddsItAgain();
    void twoOfflineStartsBecomeOneEmergency();
    void attemptWithNoEmergencyInProgressConflicts();
    void startAndNewCategoryAreNotUndoable();
    void endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheEmergency();
    void attemptsReplayedOntoAnEndedEmergencyAreInTheToRedoText();
    void endReplayedAfterAnotherAttemptConflictsAndLeavesTheEmergency();
    void undoingAnEndRestoresAnEqualEmergencyKeepsTheReportAndRedoEndsItAgain();
    void restoreConflictsWhenADifferentEmergencyIsInProgress();
    void endWhoseReportCannotBeWrittenEndsNothing();
    void endingWithNoEmergencyInProgressWritesNothing();
    void taskDeletionReplayedAfterAnotherResolvedItConflicts();
    void taskEditReplayedAfterTheTaskWasDeletedConflicts();
    void taskEditsToDifferentValuesBothApply();
    void oneTaskValueSetDifferentlyConflicts();
    void assignmentToAPersonReplayedAfterTheyWereRemovedConflicts();
    void reassigningReplacesTheWholeAssignmentAndUndoRestoresTheNotification();
    void statisticsCountEveryDirectoryFamilyAndEveryRemovedFamilyTheEmergencyNames();
    void statusTableFollowsEachChangeToOneFamily();
};
