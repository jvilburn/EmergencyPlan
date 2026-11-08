#pragma once

#include <QObject>

class WardSessionSavingTest : public QObject
{
    Q_OBJECT

public:
    explicit WardSessionSavingTest(QObject* parent);

private slots:
    void createdWardIsSavedSealedToThisDeviceAlone();
    void everyCommitSavesTheDocumentAndJournalInOneWrite();
    void undoRedoAndReplayEachSave();
    void automaticBatchSavesOnceWithTheFailedListAndItSurvivesReopening();
    void saveThatFailsKeepsTheChangeInMemoryAndTheFileAsItWas();
    void retryingAPendingSaveWritesEverythingKept();
    void nextCommandSavesWhatAFailedSaveKept();
    void reopenedWardHasItsJournalAndAnEmptyUndoStack();
    void readFaultChangesNothingAndOpeningAgainRetries();
    void fileThatDoesNotOpenCannotBeReadAndIsNotOverwritten();
    void fileWhoseHeaderIdAndNameAreChangedToAnotherWardFailsToDecrypt();
    void fileRenamedToAnotherWardIsRefused();
    void fileSealedToAnotherDeviceCannotBeRead();
    void fileFromANewerAppIsReportedAsNewer();
    void wardWithNoFileIsMissing();
};
