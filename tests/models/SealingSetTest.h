#pragma once

#include <QObject>

/// Who a document is sealed to - people, their devices, and open invites - and the
/// rule every write applies to them.
class SealingSetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // The roster
    void ofMakesItsMakerAnAdminOnOneDevice();
    void anInviteIsARecipientUntilItIsJoined();
    void aUserInviteJoinsAsANewPerson();
    void aDeviceInviteJoinsAsAnotherDeviceOfItsPerson();
    void joinRefusesWhatWouldLeaveTheSetInconsistent();
    void addInviteRefusesAnUnknownPersonOrASharedKey();
    void removeDeviceRefusesAPersonsLastDevice();
    void removePersonRefusesTheLastPersonAndTakesTheirInvites();
    void setRoleChangesOnlyThatPerson();
    void everythingComesInASteadyOrder();

    // The rule
    void anAdditionKeepsTheKey();
    void aRemovalRotates();
    void formerKeysCarryOver();

    // Merging
    void mergeTakesEachEntryFromTheSideThatChangedIt();
    void theSameEntryChangedOnBothSidesIsTheirs();
    void aMergeThatLeavesNobodyIsTheirsWhole();

    // JSON
    void roundTripsThroughJson();
    void fromJsonRefusesAnythingIllFormed();
};
