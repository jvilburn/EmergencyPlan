#pragma once

#include <QObject>

/// Section 1.10: emergency response records merged three ways on reconnect.
class DocumentMergeTest : public QObject
{
    Q_OBJECT

private slots:
    // Records and attempts
    void twoRespondersOfflineRecordsBothSurvive();
    void overlappingContactAttemptsAreAllKept();
    void anAttemptsNotesEditedOnOneSideKeepTheEdit();
    void anAttemptAbsentFromTheBaseAndEditedOnBothTakesMine();
    void contactStatusChangedDifferentlyTakesMineAndOnlyOnTheirsTakesTheirs();
    void aFamilyRecordAddedOnEitherSideSurvives();

    // Tasks
    void aTaskResolvedOnOneSideAndReassignedOnTheOtherKeepsBoth();
    void aTaskRemovedOnEitherSideStaysRemoved();
    void aTaskRemovedOnOneSideAndEditedOnTheOtherKeepsTheEdit();
    void aNotificationClearedWithItsAssignmentStaysCleared();
    void aNotificationNeverOutlivesItsAssignment();
    void categoriesMergeAsASet();

    // The response itself
    void anEmergencyStartedOnlyOnMineIsKept();
    void anEmergencyStartedOnBothIsOneResponseNamedByTheirs();
    void endedOnTheirsWhileMineRecordedKeepsTheResponse();
    void endedOnTheirsWithMineUnchangedIsEnded();
    void nothingChangedOnEitherSideIsTheBase();

    // Documents
    void aResponseChangeAndADirectoryEditBothSurviveWhicheverSideIsMine();
    void aDirectoryEditOnBothSidesIsAConflict();
    void noDirectoryChangeMergesTheResponse();
    void aRosterEditOnOneSideCarriesThrough();
    void aRosterEditAndAFamilyEditOnOppositeSidesBothCarry();
};
