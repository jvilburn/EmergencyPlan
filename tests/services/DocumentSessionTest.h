#pragma once

#include <QObject>

/// Section 6.5: one open ward - saving, publishing, and keeping the local file
/// safe - run against FakeServer with every timer fired by hand.
class DocumentSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Creating and opening
    void aCreatedWardReopensWithOnePersonAndTheSameContent();
    void openReportsEachFailure();
    void aFileRolledBackToALowerSaveCounterIsRefused();
    void aFileRolledBackToALowerGenerationIsRefused();
    void aSharedWardWhoseLocalFileWasDeletedIsDownloadedThenOpens();
    void aSharedWardWhoseLocalFileWasCorruptedIsDownloadedThenOpens();
    void atBaseWithNothingUnpublishedOpensTheLocalFile();
    void aheadWithNothingUnpublishedFastForwards();
    void atBaseWithUnpublishedChangesPublishes();
    void aheadWithUnpublishedChangesMerges();
    void unreachableOpensTheLocalFile();

    // Saving
    void severalEditsThenOneSaveAdvanceTheCounterByOne();
    void flushWritesAScheduledSaveAtOnce();
    void directoryAndResponseEditsSaveThroughTheSamePath();
    void aFailedWriteReportsAndLeavesThePreviousFile();

    // Publishing
    void offlineSavesLeaveOnePendingUpload();
    void theRetryIntervalGrowsAndCaps();
    void aPublishPendingAtCloseIsPendingAgainOnReopen();
    void aSaveDuringAPublishPublishesAfterIt();
    void aPublishWhoseReplyWasLostIsAdoptedAsItsOwn();
    void publishesAndFetchesWriteABaseCopyMatchingTheRecord();

    // Downloads
    void aServerVersionFailingValidationTouchesNothing_data();
    void aServerVersionFailingValidationTouchesNothing();
    void aVersionBelowTheHighWaterMarkIsRefused();

    // The refused-publish route
    void aRefusedPublishAdoptsTheirKeyGenerationAndRoster();
    void aResponseChangeAndAFamilyEditMergeSilently();
    void twoRefusalsInARowBothMergeSilently();
    void aFamilyEditOnBothSidesRaisesAConflict();
    void reopeningBeforeResolveMeetsTheConflictAgain();
    void keepMineAfterTheyGrantedKeepsMyDirectoryAndTheirPerson();
    void keepMineNeverRestoresAMemberTheyRemoved();
    void keepTheirsHoldsTheirDirectoryAndTheMergedResponse();

    // Removal
    void aProvenRevocationMidSessionDeletesEverything();
    void aProvenRevocationOnOpenDeletesEverything();
    void aProvenRevocationOnAPublishDeletesEverything();
    void aRevocationByAnAdminGrantedAfterTheBaseIsProven();
    void aWithheldHeaderDeletesNothing();
    void aSubstitutedHeaderDeletesNothing();
    void aRefusalWithNoProvingChainDeletesNothing();
    void aLeaveFromThisPersonsOtherDeviceReportsLeft();

    // Read-only and updates
    void aForeignLeaseRefusesDirectoryEditsButNotResponses();
    void aLeaseWhoseHeartbeatStoppedIsTakenOverAfterExpiry();
    void goingUnreachableLeavesTheSessionEditable();
    void aReadOnlySessionFastForwardsToTheHoldersPublish();
    void headChecksRunFasterDuringAnEmergency();
    void destroyingASessionReleasesTheLeaseAndWritesItsSave();
    void aLeaseGrantedAfterTheSessionIsGoneIsReleased();
    void aLaterFormatRequiresAnUpdateUntilTheServerServesOneThisBuildReads();

    // Lifetime
    void destroyingASessionWithRepliesHeldWritesNothing();

    // Sharing
    void sharingKeepsTheIdAndPathsAndRecordsABase();
    void aSecondDeviceGrantedAccessOpensIt();
    void aLinkQueuedWhileUnsharedJoinsTheFirstHeader();
};
