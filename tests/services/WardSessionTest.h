#pragma once

#include <QObject>

class WardSessionTest : public QObject
{
    Q_OBJECT

public:
    explicit WardSessionTest(QObject* parent);

private slots:
    void commitAppliesAndJournalsTheSerialisedCommand();
    void editThatChangesNothingIsNothingToDo();
    void editConfirmedAfterARemoteChangeToItsFamilyConflicts();
    void refusedCommandChangesNothingAndJournalsNothing();
    void appliedCommitSignalsOneFamilyChangeAndOneUndoStackChange();
    void conflictedOrRefusedCommitSignalsNothing();
    void emptyJournalFastForwards();
    void offlineEditsToDifferentMembersBothApply();
    void noteAndPhoneEditsToOnePersonBothApply();
    void journalThatFitsReplaysInFull();
    void oneConflictAbandonsTheWholeReplay();
    void abandonedReplaySignalsOneFullDocumentChange();
    void commandWhoseTargetWasDeletedConflicts();
    void toRedoTextStaysAsWrittenAfterARename();
    void eachAbandonmentGivesItsOwnText();
    void newCommandAfterAnAbandonedReplayJournalsAfresh();
    void openEmptiesTheJournalAndTheUndoStack();
    void undoRunsTheInverseThroughTheCommitStep();
    void appliedUndoSignalsADocumentChangeAndAnUndoStackChange();
    void removingTheFirstMemberAndUndoingRestoresTheFamilyExactly();
    void undoAfterARemoteEditToAnotherFamilyApplies();
    void undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack();
    void commandThatIsNotUndoableNeverEntersTheUndoStack();
    void newCommandClearsTheRedoSide();
    void undoBeforeAnAbandonedReplayLeavesNothingToRedo();
    void undoStackHoldsAtMostOneHundred();
};
