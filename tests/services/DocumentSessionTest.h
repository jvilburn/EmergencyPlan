#pragma once

#include <QObject>

/// One open ward - saving, publishing, the sealing-set rule, and keeping the local
/// file safe - run against FakeServer with every timer fired by hand.
class DocumentSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Creating and opening
    void aCreatedWardReopensSealedToThisDeviceWithTheSameContent();
    void openReportsEachFailure();
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

    // The sealing-set rule
    void aRemovalRotatesTheKeyPastWhoeverWasRemoved();
    void aPublishThatPrunesNobodyKeepsTheKey();
    void aRefusalAfterSomeoneLeftTakesTheirRosterAndPublishes();

    // The refused-publish route
    void aRefusedPublishAdoptsTheirKeyAndSealingSet();
    void aResponseChangeAndAFamilyEditMergeSilently();
    void twoRefusalsInARowBothMergeSilently();
    void aFamilyEditOnBothSidesRaisesAConflict();
    void reopeningBeforeResolveMeetsTheConflictAgain();
    void keepMineAfterTheyGrantedKeepsMyDirectoryAndTheirPerson();
    void keepMineNeverRestoresAMemberTheyRemoved();
    void keepTheirsHoldsTheirDirectoryAndTheMergedResponse();

    // Lost access
    void aRemovalMidSessionDeletesEverything();
    void aRemovalOnOpenDeletesEverything();
    void aRemovalOnAPublishDeletesEverything();
    void aLeaveReportsLeftOnceTheServerHasIt();
    void aLeaveFromThisPersonsOtherDeviceReportsLeft();
    void aDeviceTakenOutByItsOwnPersonReportsUnlinked();

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

    // Invites and joins
    void anInviteAddsARecipientAndARosterEntry();
    void aUserJoinsAndShowsInTheInvitersRosterAtTheNextHead();
    void aDeviceJoinsAsAnotherDeviceOfItsPerson();
    void aUsedLinkGivesInviteUsedAndWritesNothing();
    void aJoinSaysWhyItCouldNotReachTheWard();

    // The roster across a refused publish
    void aRosterEditCarriesThroughARefusedPublish();
    void rosterEditsOnBothSidesRaiseAConflict();
};
