#pragma once

#include <QObject>

/// WardSync against the fake server: clients that share one ward through it, driven step by
/// step by handing over the server's replies and channel events.
class ServerSyncTest : public QObject
{
    Q_OBJECT

public:
    explicit ServerSyncTest(QObject* parent);

private slots:
    // The first look
    void aNewWardIsCreatedOnTheServerAndConnected();
    void aCreateWhoseAnswerIsLostIsSettledByTheNextLook();
    void aWardTheServerNoLongerHasIsNoLongerShared();

    // Pushing
    void aCommandIsPushedAndTheOtherClientTakesIt();
    void aCommandMadeWhileAPushIsOnItsWayGoesOutWithTheNext();
    void aPushWhoseAnswerIsLostIsNotAppliedTwice();
    void aPushWhoseAnswerIsLostIsSettledAfterARestart();
    void twoPushesOfOneNumberTheSecondReplaysAndPushesTheNext();
    void aConflictAbandonsTheJournalAndTheNextCommandPushes();
    void anAutomaticUpdateIsPushedOnce();
    void aSaveThatFailsPushesNothing();
    void aSaveThatFailsWhileALookIsOnItsWayIsNotPushed();

    // Taking
    void aServerAheadWithAnEmptyJournalFastForwards();
    void aHeadThatRanksBelowIsPushedOver();
    void aHeadFromBeforeThisDeviceJoinedIsRetriedUntilAMemberPushes();
    void aVersionOfAnotherWardReplacesNothing();
    void aNewerFormatOnTheServerStopsPushesAndKeepsEdits();
    void aRemovedDeviceIsNoLongerSharedAndStopsAtOnce();

    // Unreachable
    void editsMadeWhileUnreachableGoUpOnReconnect();
    void afterTheServerComesBackTheWriteIsSentAgain();

    // The channel
    void theChannelSaysWhoIsHere();

    // Invitations
    void anInvitationCarriesTheServersVersionAndAnEditAfterItSurvivesTheJoin();
    void anInvitationIsNotGivenWhileNothingCanBeShared();

    // The switch and update files
    void withTheServerOffNothingIsSent();
    void anUpdateFileAboveTheHeadIsPushedOver();
    void membersWhoTradedFilesTurnTheServerOn();
};
