#pragma once

#include <QObject>

/// The wards on this device, in local app data.
class DocumentLibraryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void newIdsAreDistinctAndPathsAreUnderTheDirectory();
    void pathsForAnUnknownIdAreEmpty();
    void addingAWardTwiceKeepsItsEntry();
    void aWardAddedFromAFileIsCopiedIn();
    void entriesSortByWardNameThenId();
    void aDocumentThatNamesItsWardNamesTheEntry();
    void openingAWardMarksItToReopen();
    void anExportedCopyHasAFreshIdAndTheWardAsItsOrigin();
    void aCopyOfAWardNotInTheLibraryIsRefused();
    void aCopyIntoTheWardsDirectoryIsRefused();
    void aRecoveryCopyOpensForEveryRosterDeviceAndIsRecorded();
    void aSecondRecoveryCopyInTheSameMinuteKeepsTheFirst();
    void aRecoveryCopyNameIsOneAnyFileSystemTakes();
    void openCopySaysWhyItCannotOpenAFile();
    void forgetRemovesTheFilesRecordAndEntryAndKeepsCopies();
    void forgetRemovedDeletesCopiesStillAtTheirPaths();
    void aClearedLibraryIsRestoredFromTheServerAndEachOpens();
    void aListedWardWhoseFileWasDeletedIsFetchedOnRequest();
    void onlyAWardThatArrivesIsAdded();
    void aWardWhoseFetchIsCutShortIsNotAdded();
    void aWardProvenRemovedWhileFetchedKeepsItsEntryForForgetRemoved();
};
