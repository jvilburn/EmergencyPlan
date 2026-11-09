#pragma once

#include <QObject>

class AutomaticUpdatesTest : public QObject
{
    Q_OBJECT

public:
    explicit AutomaticUpdatesTest(QObject* parent);

private slots:
    void geocodeResultAppliesOutsideTheJournal();
    void geocodeBetweenAnOfflinePhoneEditAndItsReplayKeepsBoth();
    void addressEditPlacesTheFamilyAtTheNewAddressEntry();
    void typedCoordinatesSetTheEntryAndAGeocodeNeverReplacesThem();
    void undoOfTypedCoordinatesRestoresThePreviousEntry();
    void typedCoordinatesReplayedAfterAGeocodeOfThatAddressApply();
    void batchDropsEntriesNoFamilyUsesAndKeepsEveryEntryInUse();
    void batchOfSeveralAddressesSavesOnce();
    void failedAddressIsNotRequestedAgainUntilItsTextChanges();
    void twoFamiliesAtOneAddressShareOneRequestAndOneEntry();
    void commandThatGivesNoAddressStartsNoGeocoding();
    void batchThatChangesNothingSavesNothing();
    void lookupThatChangesNothingSavesNothing();
    void failedWardLookupContinuesToTheNextWard();
    void wardFoundNamingAMissingStakeLooksUpTheStakeBeforeTheNextWard();
    void undoOfAnAddressEditRequestsTheRestoredAddress();
    void lateAnswersChangeNothing();
    void wardFoundWithoutAChapelIsNotLookedUpAgain();
    void failedListStartsEmptyWhenAWardIsOpenedOrCreated();
    void resultsMapFamiliesAsTheyArrive();
    void documentReplacedMidBatchLosesNoResults();
    void commandThatGivesAWardNeedingALookupStartsOne();
    void addressEditFromAnotherDeviceLeavesTheFamilyUnmappedAndRequestsNothing();
    void takenUpdateRequestsAnAddressItLeavesUnmapped();
    void lookUpOfAnAddressSavedWhileItIsPendingMapsTheFamilyWithOneRequest();
    void lookUpRequestsAFailedAddressAgain();
    void lookUpOfAnAddressNoFamilyHasAnswersAndAddsNoEntry();
};
