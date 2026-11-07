#pragma once

#include <QObject>

/// References stay whole: a removal takes what it removes out of every place that holds it
/// when it runs, and its undo restores the places it held that still stand as it left them,
/// naming the rest.
class ReferenceCleanupTest : public QObject
{
    Q_OBJECT

public:
    explicit ReferenceCleanupTest(QObject* parent);

private slots:
    void deletingAFamilyRemovesItAndItsPeopleFromEveryPlaceAndUndoRestoresEach();
    void removingAMemberThroughTheEditPanelCleansUpThatPerson();
    void deletingATeamUnassignsItsTasksAndUndoAssignsThemAgain();
    void removalReplayedAfterThePersonJoinedAnotherTeamTakesThemOffItToo();
    void removalReplayedAfterThePersonWasAssignedATaskUnassignsIt();
    void undoingARemovalSkipsATeamDeletedSinceAndNamesIt();
    void undoingARemovalLeavesALeaderSetSince();
    void undoingADirectoryImportRestoresTheRemovedPeoplesMemberships();
    void ministeringImportReplayedAfterACleanupChangedAGroupConflicts();
};
