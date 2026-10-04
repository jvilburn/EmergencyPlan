#pragma once

#include <QObject>

/// Section 6.6: every change to who has access, published one at a time on the
/// server's own version.
class RosterChangeQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aChangeReadsBackFromItsRecordExactly();
    void anUnlinkReachesEveryWardAndTheOpenSessionCatchesUp();
    void aWardWhoseLeaseIsHeldElsewhereStillTakesARemoval();
    void aChangeTheServerAlreadyReflectsIsAlreadyDone();
    void aRemovalByAnAdminDemotedMeanwhileNoLongerApplies();
    void aLeaveWhoseReplyWasLostFinishesAsAlreadyDoneAndForgetsTheWard();
    void aPendingChangeSurvivesRecreatingTheQueue();
    void unpublishedDirectoryChangesSurviveARosterChange();
    void twoAdminsGrantingAtOnceBothEndUpWithTheirPerson();
    void anUnlinkOfADeviceWhoseLinkIsStillPendingDropsBoth();
};
