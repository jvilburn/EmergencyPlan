#pragma once

#include <QObject>

class AccessRosterTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Devices
    void deviceFingerprintFollowsBothKeys();
    void deviceRoundTripsThroughJson();
    void deviceFromJsonRefusesMissingOrIllFormedFields();
    void parsersRefuseAnUnknownKey();

    // People
    void personAddsAndRemovesDevices();
    void personRefusesToRemoveItsLastDevice();
    void personSortsDevicesByName();
    void personFromJsonRefusesADeviceListedTwice();
    void personRoleRoundTripsThroughJson();
    void personFromJsonRefusesAMissingOrUnknownRole();

    // Roster
    void addsRemovesAndFindsPeople();
    void refusesAMemberIdAlreadyPresent();
    void addingTheSameDeviceTwiceLeavesOne();
    void refusesADeviceHeldByAnotherPerson();
    void refusesADeviceSharingAKeyWithAnotherDevice();
    void findsAPersonAndDeviceByFingerprint();
    void removingAPersonsLastDeviceIsRefused();
    void twoPeopleSharingADisplayNameAreDistinctAndSortStably();
    void peopleAndDevicesSortIgnoringCase();
    void setRoleChangesWhoIsListedAsAdmin();
    void rosterRoundTripsThroughJson();
    void rosterFromJsonRefusesAMalformedPerson();
    void rosterFromJsonRefusesADeviceUnderTwoPeople();
    void rosterFromJsonRefusesDevicesSharingAKey();

    // Document
    void documentCarriesTheRoster();
    void documentParseAcceptsWhatToJsonWrote();
    void documentParseNeedsNewerVersionForALaterPayload();
    void documentParseRefusesAMalformedRoster();
    void documentParseRefusesAnEmptyRoster();
    void documentFromJsonDoesNotReadTheRoster();
    void rosterTakesPartInDocumentEquality();
};
