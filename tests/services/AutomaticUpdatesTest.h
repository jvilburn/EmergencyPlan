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
    void batchOfSeveralAddressesAppliesOnce();
    void failedAddressIsNotRequestedAgainUntilItsTextChanges();
    void twoFamiliesAtOneAddressShareOneRequestAndOneEntry();
    void commandThatGivesNoAddressStartsNoGeocoding();
    void batchThatChangesNothingEmitsNothing();
    void lookupThatChangesNothingEmitsNothing();
    void failedWardLookupContinuesToTheNextWard();
    void wardFoundNamingAMissingStakeLooksUpTheStakeBeforeTheNextWard();
    void undoOfAnAddressEditRequestsTheRestoredAddress();
    void lateAnswersChangeNothing();
    void wardFoundWithoutAChapelIsNotLookedUpAgain();
    void failedListEmptiesWhenAWardOpens();
    void resultsMapFamiliesAsTheyArrive();
    void documentReplacedMidBatchLosesNoResults();
    void commandThatGivesAWardNeedingALookupStartsOne();
    void addressEditFromAnotherDeviceLeavesTheFamilyUnmappedAndRequestsNothing();
};
