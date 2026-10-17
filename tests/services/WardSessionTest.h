#pragma once

#include <QObject>

class WardSessionTest : public QObject
{
    Q_OBJECT

public:
    explicit WardSessionTest(QObject* parent);

private slots:
    void commitAppliesAndJournalsTheSerialisedCommand();
    void editConfirmedAfterARemoteChangeToItsFamilyConflicts();
    void refusedCommandChangesNothingAndJournalsNothing();
    void emptyJournalFastForwards();
    void offlineEditsToDifferentMembersBothApply();
    void noteAndPhoneEditsToOnePersonBothApply();
    void journalThatFitsReplaysInFull();
    void oneConflictAbandonsTheWholeReplay();
    void commandWhoseTargetWasDeletedConflicts();
    void toRedoDescriptionsStayAsWrittenAfterARename();
    void laterAbandonmentAddsItsOwnGroup();
    void newCommandAfterAnAbandonedReplayJournalsAfresh();
    void undoRunsTheInverseThroughTheCommitStep();
    void undoAfterARemoteEditToAnotherFamilyApplies();
    void undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack();
    void commandWithoutAnInverseNeverEntersTheUndoStack();
    void newCommandClearsTheRedoSide();
    void undoStackHoldsAtMostOneHundred();
    void dismissingTheToRedoListEmptiesIt();
};
