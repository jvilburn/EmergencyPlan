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
    void aNewWardIsCreatedOnTheServerAndConnected_data();
    void aNewWardIsCreatedOnTheServerAndConnected();
    void aNewWardIsConnectedThoughItsFirstChannelIsRefusedAfterTheCreateAnswers();
    void aCreateWhoseAnswerIsLostIsSettledByTheNextLook();
    void aWardTheServerNoLongerHasIsNoLongerShared();

    // Pushing
    void aCommandIsPushedAndTheOtherClientTakesIt_data();
    void aCommandIsPushedAndTheOtherClientTakesIt();
    void anUpdateFileTakenWhileAPushIsOnItsWayIsSettledByALook();
    void aPushLandedAndThenChangedByAnotherIsNotReplayed();
    void aCommandMadeWhileAPushIsOnItsWayGoesOutWithTheNext();
    void aPushWhoseAnswerIsLostIsNotAppliedTwice();
    void aPushWhoseAnswerIsLostIsSettledAfterARestart();
    void twoPushesOfOneNumberTheSecondReplaysAndPushesTheNext();
    void aConflictAbandonsTheJournalAndTheNextCommandPushes();
    void anAutomaticUpdateIsPushedOnce();
    void anAutomaticUpdateWhoseAnswerIsLostIsNotPushedAgain();
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
    void anInviteIsConfirmedOnceTheServerNamesItAndAnEditAfterItSurvivesTheJoin_data();
    void anInviteIsConfirmedOnceTheServerNamesItAndAnEditAfterItSurvivesTheJoin();
    void anInviteIsNotConfirmedWhileNothingCanBeShared();
    void anInvitationCodeJoinsFromTheServersVersion();

    // The switch and update files
    void aSyncThatIsNotStartedSendsNothing();
    void anUpdateFileAboveTheHeadIsPushedOver();
    void membersWhoTradedFilesTurnTheServerOn();
    void aWardSharedByFilesIsCreatedByTheFirstToLook();
};
