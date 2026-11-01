#pragma once

#include <QObject>

/// Team commands hold their changed fields and member changes only: two users' additions
/// both land, a team's leader stays one of its members, and a person removed from the ward
/// is not added to a team.
class TeamCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit TeamCommandsTest(QObject* parent);

private slots:
    void twoUsersAddingDifferentMembersToOneTeamBothLand();
    void memberRemovalReplayedAfterTheyBecameLeaderConflictsAndLeavesThemMemberAndLeader();
    void removingTheLeaderClearsTheLeaderAndUndoRestoresBoth();
    void leaderChangeNeedsTheNewLeaderOnTheTeam();
    void additionOfAPersonReplayedAfterTheyWereRemovedConflicts();
    void teamDeletionReplayedAfterSomeoneAddedAMemberConflictsAndLeavesTheTeam();
    void undoTakesBackAnAddAndADeletion();
};
