#include "TeamCommandsTest.h"
#include "FamilyCommands.h"
#include "TeamCommands.h"
#include "TestClient.h"
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

/// Creates a ward from the test ward holding team in mine, and gives theirs the same ward.
void openBoth(TestClient& mine, TestClient& theirs, const Team& team)
{
    mine.createWard(TestWard::withTeam(TestWard::document(), team));
    theirs.takeWardOf(mine);
}

}  // namespace

TeamCommandsTest::TeamCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void TeamCommandsTest::twoUsersAddingDifferentMembersToOneTeamBothLand()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    Team bobOnly = withMembers(TestWard::setupTeam(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    QVERIFY(commitTeamEdit(theirs.session(), bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::carolId()})).status
            == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine.session(), bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(teamIn(mine.session()).memberIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()}));
}

void TeamCommandsTest::memberRemovalReplayedAfterTheyBecameLeaderConflictsAndLeavesThemMemberAndLeader()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    Team opened = TestWard::setupTeam();
    openBoth(mine, theirs, opened);

    QVERIFY(commitTeamEdit(theirs.session(), opened, withLeader(opened, TestWard::aliceId())).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine.session(), opened, withMembers(opened, {TestWard::bobId()})).status == CommitStatus::Applied);
    QCOMPARE(teamIn(mine.session()).memberIds(), QSet<PersonId>({TestWard::bobId()}));

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Setup team"));
    QVERIFY(result.toRedo.contains("removed: Smith, Alice"));
    QCOMPARE(teamIn(mine.session()).memberIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId()}));
    QVERIFY(teamIn(mine.session()).leaderId() == TestWard::aliceId());
}

void TeamCommandsTest::removingTheLeaderClearsTheLeaderAndUndoRestoresBoth()
{
    TestClient client("Laptop");
    Team opened = withLeader(TestWard::setupTeam(), TestWard::aliceId());
    client.createWard(TestWard::withTeam(TestWard::document(), opened));
    Team saved = withMembers(opened, {TestWard::bobId()});
    QVERIFY(!saved.leaderId().has_value());

    CommitResult result = commitTeamEdit(client.session(), opened, saved);

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Setup team", QStringList{"leader: (none)", "removed: Smith, Alice"}));
    QCOMPARE(teamIn(client.session()).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    QVERIFY(!teamIn(client.session()).leaderId().has_value());

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QVERIFY(teamIn(client.session()) == opened);
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
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    openBoth(mine, theirs, team);
    QVERIFY(commitTeamEdit(theirs.session(), team, withMembers(team, {TestWard::bobId()})).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine.session(), team, withLeader(team, TestWard::aliceId())).status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QCOMPARE(teamIn(mine.session()).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    QVERIFY(!teamIn(mine.session()).leaderId().has_value());
}

void TeamCommandsTest::additionOfAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    Team bobOnly = withMembers(TestWard::setupTeam(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    QVERIFY(theirs.session().commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice, theirs.session().document())).status == CommitStatus::Applied);
    QVERIFY(commitTeamEdit(mine.session(), bobOnly, withMembers(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.session().document().findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(teamIn(mine.session()).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    for (const Team& team : mine.session().document().teams())
    {
        for (const PersonId& member : team.memberIds())
        {
            QVERIFY(mine.session().document().findPersonById(member).has_value());
        }
    }
}

void TeamCommandsTest::teamDeletionReplayedAfterSomeoneAddedAMemberConflictsAndLeavesTheTeam()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    Team seen = TestWard::setupTeam();
    openBoth(mine, theirs, seen);
    Team grown = withMembers(seen, {TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});

    QVERIFY(commitTeamEdit(theirs.session(), seen, grown).status == CommitStatus::Applied);
    QVERIFY(mine.session().commit(DeleteTeamCommand::fromSeen(seen, mine.session().document())).status == CommitStatus::Applied);
    QVERIFY(!mine.session().document().findTeamById(TestWard::setupTeamId()).has_value());

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(result.toRedo.contains("Delete team: Setup"));
    QVERIFY(teamIn(mine.session()) == grown);
}

void TeamCommandsTest::undoTakesBackAnAddAndADeletion()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    Team team = TestWard::setupTeam();

    QVERIFY(client.session().commit(AddTeamCommand::fromNew(team, client.session().document())).status == CommitStatus::Applied);
    QVERIFY(teamIn(client.session()) == team);
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(!client.session().document().findTeamById(TestWard::setupTeamId()).has_value());
    QVERIFY(client.session().redo().status == CommitStatus::Applied);
    QVERIFY(teamIn(client.session()) == team);

    QVERIFY(client.session().commit(DeleteTeamCommand::fromSeen(team, client.session().document())).status == CommitStatus::Applied);
    QVERIFY(!client.session().document().findTeamById(TestWard::setupTeamId()).has_value());
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(teamIn(client.session()) == team);
}
