#pragma once

#include <QObject>

/// Users and devices invited and joined entirely through files, the changes-to-share rule
/// through crossed updates, and names changed in Settings reaching every ward.
class FileSharingTest : public QObject
{
    Q_OBJECT

public:
    explicit FileSharingTest(QObject* parent);

private slots:
    void invitationReadsBackItsDocumentAndSeed();
    void anyOtherFileIsNotAnInvitation();
    void userInvitedAndJoinedEntirelyThroughFiles();
    void memberVersionWrittenBeforeTakingTheJoinIsReadWithTheSeed();
    void versionFromAMemberWhoNeverTookTheJoinRunsTheJoinAgain();
    void sameInvitationUsedOnTwoDevices();
    void withdrawnInviteRotatesAndItsInvitationJoinsNothingLater();
    void invitationOpenedOnADeviceTheWardNamesJoinsNothing();
    void deviceInviteJoinsAsOneOfItsMakersDevices();
    void seedLeavesTheFileOnceTheInviteIsWithdrawnOrAbandoned();
    void addAUserIsForAdminsAndAnyoneAddsADevice();
    void changesToShareThroughCrossedUpdates();
    void renamingThisDeviceJournalsOneRenameInEachWardThatNamesItOtherwise();
    void secondDeviceTakingTheNewUserNameFollowsIt();
};
