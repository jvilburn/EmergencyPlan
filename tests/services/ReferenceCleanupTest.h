#pragma once

#include <QObject>

/// References stay whole: a removal takes what it removes out of every place that holds it
/// when it runs, and its undo restores the places it held that still stand as it left them,
/// leaving the rest as they stand, and puts back only people the document still holds.
class ReferenceCleanupTest : public QObject
{
    Q_OBJECT

public:
    explicit ReferenceCleanupTest(QObject* parent);

private slots:
    void deletingAFamilyRemovesItAndItsPeopleFromEveryPlaceAndUndoRestoresEach();
    void removingAMemberThroughTheEditPanelCleansUpThatPerson();
    void deletingATeamUnassignsItsOpenTasksAndUndoAssignsThemAgain();
    void removalReplayedAfterThePersonJoinedAnotherTeamTakesThemOffItToo();
    void removalReplayedAfterThePersonWasAssignedATaskUnassignsIt();
    void removingAPersonUnassignsTheirOpenTaskAndLeavesTheirResolvedOneAssigned();
    void reopeningATaskWhoseAssigneeWasRemovedReopensItUnassigned();
    void reopeningReplayedAfterTheAssigneeWasRemovedConflicts();
    void undoingARemovalLeavesATeamDeletedSince();
    void undoingARemovalLeavesALeaderSetSince();
    void undoingADirectoryImportRestoresTheRemovedPeoplesMemberships();
    void ministeringImportReplayedAfterACleanupChangedAGroupConflicts();
    void undoingAFamilyDeleteLeavesEachPlaceSomeoneElseChanged();
    void undoingAnImportRestoresTheMemberItRemovedFromAFamily();
    void secondUndoOfOneFamilyDeletePutsBackOnlyTheMembersItStillHolds();
    void undoingAnImportRestoresALeaderWhateverTheFamilyOrder_data();
    void undoingAnImportRestoresALeaderWhateverTheFamilyOrder();
};
