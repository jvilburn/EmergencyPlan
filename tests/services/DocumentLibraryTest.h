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
    void entriesSortByWardNameThenId();
    void aDocumentThatNamesItsWardNamesTheEntry();
    void openingAWardMarksItToReopen();
    void anExportedCopyHasAFreshIdAndTheWardAsItsOrigin();
    void aCopyOfAWardNotInTheLibraryIsRefused();
    void aCopyIntoTheWardsDirectoryIsRefused();
    void openCopySaysWhyItCannotOpenAFile();
    void forgetRemovesTheFilesRecordAndEntryAndKeepsCopies();
    void forgetRemovedDeletesCopiesStillAtTheirPaths();
};
