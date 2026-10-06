#pragma once

#include <QObject>

/// Task 11: the wards on this device, in local app data.
class DocumentLibraryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void newIdsAreDistinctAndPathsAreUnderTheDirectory();
    void pathsForAnUnknownIdAreEmpty();
    void entriesSortByWardName();
    void aDocumentThatNamesItsWardNamesTheEntry();
    void openingAWardMarksItToReopen();
    void anExportedCopyHasAFreshIdAndTheWardAsItsOrigin();
    void aCopyForAnotherDeviceIsRefused();
    void forgetRemovesTheFilesRecordAndEntryAndKeepsCopies();
    void forgetRemovedDeletesCopiesStillAtTheirPaths();
};
