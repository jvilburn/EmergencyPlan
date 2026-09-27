#pragma once

#include <QObject>

class AccessControlTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Grant
    void grantLetsTheNewPersonsDeviceOpenTheDocument();
    void grantLeavesTheDataKeyAndGenerationAlone();
    void grantMakesTheNewPersonAMember();
    void grantRefusesAWriterWhoIsNotAnAdmin();
    void grantRefusesAMemberIdAlreadyInTheRoster();
    void grantRefusesAPersonWithMoreThanOneDevice();
    void grantRefusesADeviceWithNoName();
    void grantRefusesADeviceWithInvalidKeys();
    void grantRefusesADeviceSharingAKey();
    void grantRefusesAnInvalidDataKey();
    void grantVerifiesAsALink();

    // Link
    void linkDeviceLetsTheNewDeviceOpenTheDocumentAndLinkVerifies();
    void linkDeviceIsOpenToAMember();
    void linkDeviceRefusesAnotherPersonsDevice();
    void linkDeviceRefusesADeviceAlreadyLinked();
    void linkDeviceRefusesADeviceWithNoName();
    void linkDeviceRefusesADeviceSharingAKey();
    void linkDeviceRefusesAWriterNotOnTheRoster();

    // Unlink
    void unlinkDeviceRotatesTheKeyAndTheUnlinkedDeviceCannotUnseal();
    void unlinkDeviceRefusesTheLastDevice();
    void unlinkDeviceLetsAnAdminDropAnotherPersonsDevice();
    void unlinkDeviceRefusesAMemberDroppingAnotherPersonsDevice();
    void unlinkDeviceRefusesTheWritingDevice();

    // Remove
    void removeDropsEveryDeviceOfThePerson();
    void removeRotatesTheKeyAndBumpsTheGeneration();
    void removedPersonCannotOpenWhatComesNext();
    void removeVerifiesAsALink();
    void removeLetsAnAdminRemoveAnotherAdmin();
    void removeRefusesAWriterWhoIsNotAnAdmin();
    void removeRefusesTheWritersOwnPerson();
    void removeRefusesSomeoneNotOnTheRoster();

    // Roles
    void setRolePromotesAMemberAndLinkVerifies();
    void setRoleLetsAnAdminStepDownWhenAnotherRemains();
    void setRoleRefusesDemotingTheLastAdmin();
    void setRoleRefusesAWriterWhoIsNotAnAdmin();
    void setRoleRefusesSomeoneNotOnTheRoster();

    // Leave
    void leaveDropsEveryDeviceOfTheLeaverAndLinkVerifies();
    void leaveDoesNotRotateTheKey();
    void remainingPeopleOpenTheVersionALeaveWrote();
    void leaveRefusesTheLastAdminWhileOthersRemain();
    void leaveLetsAnAdminGoWhenAnotherRemains();
    void theLastPersonsLeaveListsNoOne();
    void leaveRefusesAWriterNotOnTheRoster();

    // Reseal and match
    void resealAllGivesEveryMemberAnEntry();
    void resealAllCopiesEachMembersVerifyKey();
    void resealAllListsTheRostersAdmins();
    void resealAllRefusesAnInvalidDataKey();
    void headerEntryWithNoRosterDeviceFailsTheMatch();
    void rosterDeviceWithNoHeaderEntryFailsTheMatch();
    void entryWhoseVerifyKeyDisagreesWithRosterFailsTheMatch();
    void entryWhoseMemberDisagreesWithRosterFailsTheMatch();
    void entryWhosePublicKeyDisagreesWithRosterFailsTheMatch();
    void headerWithADuplicateEntryFailsTheMatch();
    void headerWhoseAdminsDisagreeWithRosterFailsTheMatch();

    // Every operation
    void everyAccessChangeProducesAMatchingHeaderAndRoster();
    void operationsRefuseAHeaderThatDisagreesWithItsRoster();
    void changesClearWhatAWriteMustProduceAfresh();
};
