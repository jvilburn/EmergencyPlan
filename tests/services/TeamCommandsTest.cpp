#include "TeamCommandsTest.h"
#include "FamilyCommands.h"
#include "TeamCommands.h"
#include "TestSession.h"
#include "TestWard.h"

#include <QTest>

namespace
{

Team teamIn(const WardSession& session)
{
    return session.document().findTeamById(TestWard::setupTeamId()).value();
}

CommitResult commitTeamEdit(WardSession& session, const Team& opened, const Team& saved)
{
    return session.commit(UpdateTeamCommand::fromEdit(opened, saved, session.document()));
}

Team withMembers(const Team& team, const QSet<PersonId>& members)
{
    Team updated = team;
    updated.setMemberIds(members);
    return updated;
}

Team withLeader(const Team& team, const std::optional<PersonId>& leader)
{
    Team updated = team;
    updated.setLeaderId(leader);
    return updated;
}

/// Creates a ward in both sessions from the test ward holding team.
void openBoth(TestSession& mine, TestSession& theirs, const Team& team)
{
    Document document = TestWard::withTeam(TestWard::document(), team);
    mine.createWard(document);
    theirs.createWard(document);
}

}  // namespace

TeamCommandsTest::TeamCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void TeamCommandsTest::twoUsersAddingDifferentMembersToOneTeamBothLand()
{
    TestSession mine;
    TestSession theirs;
    Team bobOnly = withMembers(TestWard::setupTeam(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    QVERIFY(commitTeamEdit(theirs, bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::carolId()})).status
            == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine, bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(teamIn(mine).memberIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()}));
}

void TeamCommandsTest::memberRemovalReplayedAfterTheyBecameLeaderConflictsAndLeavesThemMemberAndLeader()
{
    TestSession mine;
    TestSession theirs;
    Team opened = TestWard::setupTeam();
    openBoth(mine, theirs, opened);

    QVERIFY(commitTeamEdit(theirs, opened, withLeader(opened, TestWard::aliceId())).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine, opened, withMembers(opened, {TestWard::bobId()})).status == CommitStatus::Applied);
    QCOMPARE(teamIn(mine).memberIds(), QSet<PersonId>({TestWard::bobId()}));

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Setup team"));
    QVERIFY(result.toRedo.contains("removed: Smith, Alice"));
    QCOMPARE(teamIn(mine).memberIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId()}));
    QVERIFY(teamIn(mine).leaderId() == TestWard::aliceId());
}

void TeamCommandsTest::removingTheLeaderClearsTheLeaderAndUndoRestoresBoth()
{
    TestSession session;
    Team opened = withLeader(TestWard::setupTeam(), TestWard::aliceId());
    session.createWard(TestWard::withTeam(TestWard::document(), opened));
    Team saved = withMembers(opened, {TestWard::bobId()});
    QVERIFY(!saved.leaderId().has_value());

    CommitResult result = commitTeamEdit(session, opened, saved);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Setup team", QStringList{"leader: (none)", "removed: Smith, Alice"}));
    QCOMPARE(teamIn(session).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    QVERIFY(!teamIn(session).leaderId().has_value());

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(teamIn(session) == opened);
}

void TeamCommandsTest::leaderChangeNeedsTheNewLeaderOnTheTeam()
{
    // A leader who is not a member, set directly.
    Team team = TestWard::setupTeam();
    Document document = TestWard::withTeam(TestWard::document(), team);
    Document before = document;
    QVERIFY(UpdateTeamCommand::fromEdit(team, withLeader(team, TestWard::carolId()), document)->execute(document)
            == CommandOutcome::Conflicted);
    QVERIFY(document == before);
    QVERIFY(UpdateTeamCommand::fromEdit(team, withLeader(team, TestWard::bobId()), document)->execute(document)
            == CommandOutcome::Applied);
    QVERIFY(TestWard::withTeam(TestWard::document(), withLeader(team, TestWard::bobId())) == document);

    // A member made leader, replayed after someone removed them from the team.
    TestSession mine;
    TestSession theirs;
    openBoth(mine, theirs, team);
    QVERIFY(commitTeamEdit(theirs, team, withMembers(team, {TestWard::bobId()})).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine, team, withLeader(team, TestWard::aliceId())).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QCOMPARE(teamIn(mine).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    QVERIFY(!teamIn(mine).leaderId().has_value());
}

void TeamCommandsTest::additionOfAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestSession mine;
    TestSession theirs;
    Team bobOnly = withMembers(TestWard::setupTeam(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    QVERIFY(theirs.commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice, theirs.document())).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine, bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.document().findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(teamIn(mine).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    for (const Team& team : mine.document().teams())
    {
        for (const PersonId& member : team.memberIds())
        {
            QVERIFY(mine.document().findPersonById(member).has_value());
        }
    }
}

void TeamCommandsTest::teamDeletionReplayedAfterSomeoneAddedAMemberConflictsAndLeavesTheTeam()
{
    TestSession mine;
    TestSession theirs;
    Team seen = TestWard::setupTeam();
    openBoth(mine, theirs, seen);
    Team grown = withMembers(seen, {TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});

    QVERIFY(commitTeamEdit(theirs, seen, grown).status == CommitStatus::Applied);
    QVERIFY(mine.commit(DeleteTeamCommand::fromSeen(seen, mine.document())).status == CommitStatus::Applied);
    QVERIFY(!mine.document().findTeamById(TestWard::setupTeamId()).has_value());

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete team: Setup"));
    QVERIFY(teamIn(mine) == grown);
}

void TeamCommandsTest::undoTakesBackAnAddAndADeletion()
{
    TestSession session;
    session.createWard(TestWard::document());
    Team team = TestWard::setupTeam();

    QVERIFY(session.commit(AddTeamCommand::fromNew(team, session.document())).status == CommitStatus::Applied);
    QVERIFY(teamIn(session) == team);
    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(!session.document().findTeamById(TestWard::setupTeamId()).has_value());
    QVERIFY(session.redo().status == CommitStatus::Applied);
    QVERIFY(teamIn(session) == team);

    QVERIFY(session.commit(DeleteTeamCommand::fromSeen(team, session.document())).status == CommitStatus::Applied);
    QVERIFY(!session.document().findTeamById(TestWard::setupTeamId()).has_value());
    QVERIFY(session.undo().status == CommitStatus::Applied);
    QVERIFY(teamIn(session) == team);
}
