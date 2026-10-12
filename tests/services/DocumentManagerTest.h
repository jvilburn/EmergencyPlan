#pragma once

#include <QObject>

/// The window's ward on the sync-and-save core: commands, undo, the emergency
/// response, and automatic directory updates, each through the session.
class DocumentManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aForeignLeaseRefusesCommandsUndoAndRedoButNotResponses();
    void aFastForwardClearsTheUndoStack();
    void aContactChangeAfterAMergeKeepsTheOtherSidesRecords();
    void batchGeocodingDoesNotStartWithoutTheLease();
    void aGeocodingResultAfterTheLeaseIsLostChangesNothing();
    void aConflictWaitsUntilItIsSettled();

    // Access history and lost access, through the app's own services
    void accessChangesShowOnceAndStaySeenOnceDismissed();
    void aFetchThatFindsAccessLostSaysWhy();
};
