#pragma once

#include <QObject>

/// Access-list changes as commands: their rules on the first run and on replay, and removals
/// and rotations reaching every device through update files.
class AccessTest : public QObject
{
    Q_OBJECT

public:
    explicit AccessTest(QObject* parent);

private slots:
    void removalRotatesAndReachesEveryDevice();
    void writeThatRemovesNobodyKeepsTheKeyAndSealsToExactlyTheAccessList();
    void twoAdminsRemovingDifferentUsersBothApply();
    void removingTheSameUserTwiceAppliesInFull();
    void removalReplayedAfterThatUserAddedADeviceTakesTheNewDeviceToo();
    void removalReplayedAfterThatUserMadeAnInviteWithdrawsIt();
    void demotedAdminsRemovalIsRefusedWithTheMembersWording();
    void onlyAdminCannotStepDownFirstRunOrOnReplay();
    void removingYourselfOrYourLastDeviceIsRefusedWithUseLeave();
    void memberCannotRemoveUsersSetRolesOrWithdrawAnothersInvite();
    void removingAnotherDeviceOfYourOwnRotatesItOut();
    void roleChangeReplayedAfterItsUserWasRemovedConflicts();
    void offlineAcrossTwoRotationsTakesTheLatest();
    void updatesThatCrossARotationConverge();
    void renamesReachEveryDevice();
};
