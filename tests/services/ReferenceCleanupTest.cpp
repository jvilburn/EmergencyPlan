#include "ReferenceCleanupTest.h"
#include "EmergencyAssetCommands.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "FamilyEditPanel.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "MemberEditor.h"
#include "PdfText.h"
#include "TeamCommands.h"
#include "TestClient.h"
#include "TestWard.h"

#include <QJsonArray>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

namespace
{

const MinisteringOrg EQ = MinisteringOrg::EldersQuorum;
const MinisteringOrg RS = MinisteringOrg::ReliefSociety;

bool personExists(const Document& document, const std::optional<PersonId>& id)
{
    return !id.has_value()
        || document.findPersonById(*id).has_value();
}

/// True when every person, family and team that a team, asset, group, district or open
/// task's assignment names is in document.
bool referencesAreWhole(const Document& document)
{
    for (const Team& team : document.teams())
    {
        if (!everyPersonExists(team.memberIds(), document) || !personExists(document, team.leaderId()))
        {
            return false;
        }
    }
    for (const EmergencyAsset& asset : document.emergencyAssets())
    {
        if (!everyPersonExists(asset.personIds(), document))
        {
            return false;
        }
    }
    for (MinisteringOrg org : {EQ, RS})
    {
        for (const MinisteringGroup& group : document.groups(org))
        {
            if (!everyPersonExists(group.ministerIds(), document)
                || !everyPersonExists(group.ministeredPersonIds(), document)
                || !personExists(document, group.presidencyMemberId()))
            {
                return false;
            }
            for (const FamilyId& family : group.familyIds())
            {
                if (!document.findFamilyById(family).has_value())
                {
                    return false;
                }
            }
        }
        for (const MinisteringDistrict& district : document.districts(org))
        {
            if (!personExists(document, district.presidencyMemberId()))
            {
                return false;
            }
        }
    }
    if (document.emergencyResponse().has_value())
    {
        for (const ResponseTask& task : document.emergencyResponse()->tasks())
        {
            if (task.isResolved() || !task.assignment().has_value())
            {
                continue;
            }
            const std::optional<TeamId>& team = task.assignment()->teamId();
            if ((team.has_value() && !document.findTeamById(*team).has_value())
                || !personExists(document, task.assignment()->personId()))
            {
                return false;
            }
        }
    }
    return true;
}

Team teamIn(const Document& document, const TeamId& id)
{
    return document.findTeamById(id).value();
}

Team setupIn(const Document& document)
{
    return teamIn(document, TestWard::setupTeamId());
}

EmergencyAsset generatorIn(const Document& document)
{
    return document.findEmergencyAssetById(TestWard::generatorId()).value();
}

MinisteringGroup groupIn(const Document& document, MinisteringOrg org, const MinisteringGroupId& id)
{
    return document.groups(org).value(id);
}

MinisteringDistrict districtIn(const Document& document)
{
    return document.districts(EQ).value(TestWard::districtId());
}

/// The task with that id in the emergency in progress, or a default task when there is none.
ResponseTask taskIn(const Document& document, const TaskId& id)
{
    const ResponseTask* task = document.emergencyResponse()->findTask(id);
    return task != nullptr ? *task : ResponseTask();
}

ResponseTask unassigned(const ResponseTask& task)
{
    ResponseTask updated = task;
    updated.setAssignment(std::nullopt);
    return updated;
}

/// document with task added to its emergency in progress.
Document withTask(const Document& document, const ResponseTask& task)
{
    EmergencyResponse response = *document.emergencyResponse();
    response.insertTask(task);
    return TestWard::withResponse(document, response);
}

/// The Repairs task for the Jones family, created at 11:30, assigned to assignee and
/// resolved at noon.
ResponseTask resolvedFence(const TaskAssignment& assignee)
{
    ResponseTask fence = TestWard::task("task-fence", TestWard::jones(), TestWard::octoberFirst(11, 30), "Repairs",
                                        "Fence down along the drive");
    fence.setAssignment(assignee);
    fence.setResolution(TestWard::resolution("Propped back up", TestWard::octoberFirst(12, 0)));
    return fence;
}

Team withMembers(const Team& team, const QSet<PersonId>& members)
{
    Team updated = team;
    updated.setMemberIds(members);
    return updated;
}

/// family without that member.
Family withoutMember(const Family& family, const PersonId& id)
{
    QList<Person> members;
    for (const Person& member : family.members())
    {
        if (member.id() != id)
        {
            members.append(member);
        }
    }
    Family updated = family;
    updated.setMembers(members);
    return updated;
}

/// The Smith family without that member.
Family smithWithout(const PersonId& id)
{
    return withoutMember(TestWard::smith(), id);
}

/// family with person added as its last member.
Family withAddedMember(const Family& family, const Person& person)
{
    Family updated = family;
    QList<Person> members = family.members();
    members.append(person);
    updated.setMembers(members);
    return updated;
}

/// A directory import of the Tanglewood ward, dated 15 October 2026, holding families.
WardDirectoryImportResult directoryImport(const QList<Family>& families)
{
    WardDirectoryImportResult result;
    result.success = true;
    for (const Family& family : families)
    {
        result.families.insert(family.id(), family);
    }
    result.wardName = "Tanglewood";
    result.wardUnitNumber = "123456";
    result.pdfDate = QDate(2026, 10, 15);
    return result;
}

/// True when no team, asset, group, district or task's assignment in document names id.
bool inNoPlace(const Document& document, const PersonId& id)
{
    for (const Team& team : document.teams())
    {
        if (team.memberIds().contains(id) || team.leaderId() == id)
        {
            return false;
        }
    }
    for (const EmergencyAsset& asset : document.emergencyAssets())
    {
        if (asset.personIds().contains(id))
        {
            return false;
        }
    }
    for (MinisteringOrg org : {EQ, RS})
    {
        for (const MinisteringGroup& group : document.groups(org))
        {
            if (group.ministerIds().contains(id)
                || group.ministeredPersonIds().contains(id)
                || group.presidencyMemberId() == id)
            {
                return false;
            }
        }
        for (const MinisteringDistrict& district : document.districts(org))
        {
            if (district.presidencyMemberId() == id)
            {
                return false;
            }
        }
    }
    if (document.emergencyResponse().has_value())
    {
        for (const ResponseTask& task : document.emergencyResponse()->tasks())
        {
            if (task.assignment().has_value() && task.assignment()->personId() == id)
            {
                return false;
            }
        }
    }
    return true;
}

CommitResult removeFromSmiths(WardSession& session, const PersonId& id)
{
    return session.commit(UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(id), session.document()));
}

/// The team with that name and members, with a fresh id.
Team newTeam(const QString& name, const QSet<PersonId>& members)
{
    Team team = Team::create(name);
    team.setMemberIds(members);
    return team;
}

/// Clicks Remove Member on the panel's editor for that person. False when it has none.
bool clickRemoveMember(const FamilyEditPanel& panel, const PersonId& id)
{
    for (MemberEditor* editor : panel.findChildren<MemberEditor*>())
    {
        if (editor->person().id() != id)
        {
            continue;
        }
        for (QPushButton* button : editor->findChildren<QPushButton*>())
        {
            if (button->text() == "Remove Member")
            {
                button->click();
                return true;
            }
        }
    }
    return false;
}

QList<PersonId> idsOf(const Family& family)
{
    QList<PersonId> ids;
    for (const Person& member : family.members())
    {
        ids.append(member.id());
    }
    return ids;
}

}  // namespace

ReferenceCleanupTest::ReferenceCleanupTest(QObject* parent)
    : QObject(parent)
{
}

void ReferenceCleanupTest::deletingAFamilyRemovesItAndItsPeopleFromEveryPlaceAndUndoRestoresEach()
{
    Document ward = TestWard::documentWithPlaces();
    QVERIFY(referencesAreWhole(ward));
    Document dangling = ward;
    dangling.removeFamily(TestWard::smithId());
    QVERIFY(!referencesAreWhole(dangling));
    TestClient client("Laptop");
    client.createWard(ward);
    Document created = client.session().document();

    CommitResult result = client.session().commit(DeleteFamilyCommand::fromSeen(TestWard::smith(), client.session().document()));

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(result.description, CommandDescription("Delete family: Smith, Bob & Alice", {}));
    const Document& deleted = client.session().document();
    QVERIFY(!deleted.findFamilyById(TestWard::smithId()).has_value());
    QVERIFY(setupIn(deleted) == withMembers(TestWard::setupTeam(), {}));
    EmergencyAsset generator = TestWard::generator();
    generator.setPersonIds({});
    QVERIFY(generatorIn(deleted) == generator);
    MinisteringGroup eqGroup = TestWard::eqGroup();
    eqGroup.setMinisterIds({});
    eqGroup.setFamilyIds({TestWard::jonesId()});
    eqGroup.setPresidencyMemberId(std::nullopt);
    QVERIFY(groupIn(deleted, EQ, TestWard::eqGroupId()) == eqGroup);
    MinisteringGroup rsGroup = TestWard::rsGroup();
    rsGroup.setMinisteredPersonIds({});
    QVERIFY(groupIn(deleted, RS, TestWard::rsGroupId()) == rsGroup);
    MinisteringDistrict district = TestWard::district();
    district.setPresidencyMemberId(std::nullopt);
    QVERIFY(districtIn(deleted) == district);
    QVERIFY(taskIn(deleted, TestWard::shinglesTaskId()) == unassigned(taskIn(ward, TestWard::shinglesTaskId())));
    QVERIFY(taskIn(deleted, TestWard::basementTaskId()) == taskIn(ward, TestWard::basementTaskId()));
    QVERIFY(referencesAreWhole(deleted));
    Document afterDelete = deleted;

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);

    QVERIFY(client.session().redo().status == CommitStatus::Applied);
    QVERIFY(client.session().document() == afterDelete);
}

void ReferenceCleanupTest::removingAMemberThroughTheEditPanelCleansUpThatPerson()
{
    Document ward = TestWard::documentWithPlaces();
    TestClient client("Laptop");
    client.createWard(ward);
    Document created = client.session().document();
    QSignalSpy documentSpy(&client.session(), &WardSession::documentChanged);
    FamilyEditPanel panel(nullptr);
    panel.setFamily(TestWard::smith(), client.session().document());

    QVERIFY(clickRemoveMember(panel, TestWard::aliceId()));
    QCOMPARE(idsOf(panel.family()), QList<PersonId>({TestWard::bobId()}));
    CommitResult result = client.session().commit(UpdateFamilyCommand::fromEdit(panel.openedFamily(), panel.family(),
                                                                       client.session().document()));

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(documentSpy.count(), 1);
    QVERIFY(documentSpy.at(0).at(0).value<DocumentChange>().action == ChangeAction::Full);
    const Document& removed = client.session().document();
    QVERIFY(TestWard::family(removed, TestWard::smithId()) == smithWithout(TestWard::aliceId()));
    Team setup = withMembers(TestWard::setupTeam(), {TestWard::bobId()});
    setup.setLeaderId(TestWard::bobId());
    QVERIFY(setupIn(removed) == setup);
    EmergencyAsset generator = TestWard::generator();
    generator.setPersonIds({TestWard::bobId()});
    QVERIFY(generatorIn(removed) == generator);
    MinisteringGroup eqGroup = TestWard::eqGroup();
    eqGroup.setPresidencyMemberId(std::nullopt);
    QVERIFY(groupIn(removed, EQ, TestWard::eqGroupId()) == eqGroup);
    MinisteringGroup rsGroup = TestWard::rsGroup();
    rsGroup.setMinisteredPersonIds({});
    QVERIFY(groupIn(removed, RS, TestWard::rsGroupId()) == rsGroup);
    QVERIFY(districtIn(removed) == TestWard::district());
    QVERIFY(taskIn(removed, TestWard::shinglesTaskId()) == unassigned(taskIn(ward, TestWard::shinglesTaskId())));
    QVERIFY(taskIn(removed, TestWard::basementTaskId()) == taskIn(ward, TestWard::basementTaskId()));
    QVERIFY(referencesAreWhole(removed));

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);
}

void ReferenceCleanupTest::deletingATeamUnassignsItsOpenTasksAndUndoAssignsThemAgain()
{
    Document places = TestWard::documentWithPlaces();
    ResponseTask fence = resolvedFence(TaskAssignment::toTeam(setupIn(places)));
    Document ward = withTask(places, fence);
    TestClient client("Laptop");
    client.createWard(ward);
    Document created = client.session().document();

    QVERIFY(client.session().commit(DeleteTeamCommand::fromSeen(setupIn(ward), client.session().document())).status == CommitStatus::Applied);

    QVERIFY(!client.session().document().findTeamById(TestWard::setupTeamId()).has_value());
    QVERIFY(taskIn(client.session().document(), TestWard::basementTaskId()) == unassigned(taskIn(ward, TestWard::basementTaskId())));
    QVERIFY(taskIn(client.session().document(), TestWard::shinglesTaskId()) == taskIn(ward, TestWard::shinglesTaskId()));
    ResponseTask fenceAfter = taskIn(client.session().document(), fence.id());
    QVERIFY(fenceAfter == fence);
    QVERIFY(fenceAfter.assignment()->teamId() == TestWard::setupTeamId());
    QCOMPARE(fenceAfter.assignment()->assigneeName(), QString("Setup"));
    QVERIFY(referencesAreWhole(client.session().document()));

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);
}

void ReferenceCleanupTest::removalReplayedAfterThePersonJoinedAnotherTeamTakesThemOffItToo()
{
    Team cleanup = newTeam("Cleanup", {TestWard::carolId()});
    Document ward = TestWard::withTeam(TestWard::documentWithPlaces(), cleanup);
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);

    Team joined = withMembers(cleanup, {TestWard::carolId(), TestWard::aliceId()});
    QVERIFY(theirs.session().commit(UpdateTeamCommand::fromEdit(cleanup, joined, theirs.session().document())).status == CommitStatus::Applied);
    QVERIFY(removeFromSmiths(mine.session(), TestWard::aliceId()).status == CommitStatus::Applied);

    ReplayResult replay = mine.session().replayOnto(theirs.session().document());

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(!mine.session().document().findPersonById(TestWard::aliceId()).has_value());
    QVERIFY(teamIn(mine.session().document(), cleanup.id()) == cleanup);
    QCOMPARE(setupIn(mine.session().document()).memberIds(), QSet<PersonId>({TestWard::bobId()}));
    QVERIFY(referencesAreWhole(mine.session().document()));

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(TestWard::person(mine.session().document(), TestWard::aliceId()) == TestWard::alice());
    QVERIFY(setupIn(mine.session().document()) == setupIn(ward));
    QVERIFY(teamIn(mine.session().document(), cleanup.id()) == cleanup);
}

void ReferenceCleanupTest::removalReplayedAfterThePersonWasAssignedATaskUnassignsIt()
{
    Document ward = TestWard::documentWithPlaces();
    ResponseTask gutter = TestWard::task("task-gutter", TestWard::jones(), TestWard::octoberFirst(11, 0), "Repairs",
                                         "Gutter hanging loose");
    ward = withTask(ward, gutter);
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);

    ResponseTask assigned = gutter;
    assigned.setAssignment(TaskAssignment::toPerson(TestWard::alice()));
    QVERIFY(theirs.session().commit(UpdateTaskCommand::fromEdit(gutter, assigned)).status == CommitStatus::Applied);
    QVERIFY(taskIn(theirs.session().document(), gutter.id()) == assigned);
    QVERIFY(removeFromSmiths(mine.session(), TestWard::aliceId()).status == CommitStatus::Applied);

    ReplayResult replay = mine.session().replayOnto(theirs.session().document());

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(taskIn(mine.session().document(), gutter.id()) == gutter);
    QVERIFY(taskIn(mine.session().document(), TestWard::shinglesTaskId()) == unassigned(taskIn(ward, TestWard::shinglesTaskId())));
    QVERIFY(referencesAreWhole(mine.session().document()));

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(taskIn(mine.session().document(), gutter.id()) == gutter);
    QVERIFY(taskIn(mine.session().document(), TestWard::shinglesTaskId()) == taskIn(ward, TestWard::shinglesTaskId()));
}

void ReferenceCleanupTest::removingAPersonUnassignsTheirOpenTaskAndLeavesTheirResolvedOneAssigned()
{
    // Alice holds the open Shingles task and the resolved Fence task, and phoned the Joneses
    Document places = TestWard::documentWithPlaces();
    ResponseTask fence = resolvedFence(TaskAssignment::toPerson(TestWard::alice()));
    EmergencyResponse response = *withTask(places, fence).emergencyResponse();
    response.insertContactAttempt(TestWard::attempt("attempt-alice", TestWard::jones(), TestWard::octoberFirst(9, 45),
                                                    ContactMethod::Phone, TestWard::alice(), std::nullopt,
                                                    "Left a message"));
    Document ward = TestWard::withResponse(places, response);
    TestClient client("Laptop");
    client.createWard(ward);
    Document created = client.session().document();

    QVERIFY(removeFromSmiths(client.session(), TestWard::aliceId()).status == CommitStatus::Applied);

    const Document& removed = client.session().document();
    QVERIFY(!removed.findPersonById(TestWard::aliceId()).has_value());
    QVERIFY(taskIn(removed, TestWard::shinglesTaskId()) == unassigned(taskIn(ward, TestWard::shinglesTaskId())));
    ResponseTask fenceAfter = taskIn(removed, fence.id());
    QVERIFY(fenceAfter == fence);
    QVERIFY(fenceAfter.assignment()->personId() == TestWard::aliceId());
    QCOMPARE(fenceAfter.assignment()->assigneeName(), QString("Smith, Alice"));
    QVERIFY(referencesAreWhole(removed));

    // The report names her as the assignment and the attempt recorded her; the open task,
    // unassigned, names no one
    QString text = reportText(removed);
    QVERIFY2(text.contains("Fence down along the drive"), qPrintable(text));
    QCOMPARE(text.count("Assigned to: Smith, Alice"), qsizetype(1));
    QVERIFY2(text.contains("Phone - Smith, Alice - Left a message"), qPrintable(text));
    QVERIFY2(text.contains("Assigned to: Setup"), qPrintable(text));

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);
}

void ReferenceCleanupTest::reopeningATaskWhoseAssigneeWasRemovedReopensItUnassigned()
{
    // Alice resolved the Fence task, then left the ward
    ResponseTask fence = resolvedFence(TaskAssignment::toPerson(TestWard::alice()));
    TestClient client("Laptop");
    client.createWard(withTask(TestWard::documentWithPlaces(), fence));
    QVERIFY(removeFromSmiths(client.session(), TestWard::aliceId()).status == CommitStatus::Applied);
    QVERIFY(taskIn(client.session().document(), fence.id()) == fence);

    CommitResult reopened = client.session().commit(UpdateTaskCommand::reopening(fence, client.session().document()));

    QVERIFY(reopened.status == CommitStatus::Applied);
    ResponseTask expected = unassigned(fence);
    expected.setResolution(std::nullopt);
    QVERIFY(taskIn(client.session().document(), fence.id()) == expected);
    QVERIFY(referencesAreWhole(client.session().document()));
}

void ReferenceCleanupTest::reopeningReplayedAfterTheAssigneeWasRemovedConflicts()
{
    ResponseTask fence = resolvedFence(TaskAssignment::toPerson(TestWard::alice()));
    Document ward = withTask(TestWard::documentWithPlaces(), fence);
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);

    // Reopened while the ward still holds Alice, the task keeps her
    QVERIFY(mine.session().commit(UpdateTaskCommand::reopening(fence, mine.session().document())).status == CommitStatus::Applied);
    ResponseTask reopened = fence;
    reopened.setResolution(std::nullopt);
    QVERIFY(taskIn(mine.session().document(), fence.id()) == reopened);

    QVERIFY(removeFromSmiths(theirs.session(), TestWard::aliceId()).status == CommitStatus::Applied);
    ReplayResult replay = mine.session().replayOnto(theirs.session().document());

    QVERIFY(replay.status == ReplayStatus::Abandoned);
    QVERIFY(mine.session().document() == theirs.session().document());
    QVERIFY(taskIn(mine.session().document(), fence.id()) == fence);
    QVERIFY(referencesAreWhole(mine.session().document()));
}

void ReferenceCleanupTest::undoingARemovalLeavesATeamDeletedSince()
{
    Team cleanup = newTeam("Cleanup", {TestWard::aliceId(), TestWard::carolId()});
    Document ward = TestWard::withTeam(TestWard::documentWithPlaces(), cleanup);
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);

    QVERIFY(theirs.session().commit(DeleteTeamCommand::fromSeen(cleanup, theirs.session().document())).status == CommitStatus::Applied);
    QVERIFY(mine.session().commit(DeleteFamilyCommand::fromSeen(TestWard::smith(), mine.session().document())).status == CommitStatus::Applied);
    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QCOMPARE(undone.description.heading(), QString("Undo: Delete family: Smith, Bob & Alice"));
    QVERIFY(TestWard::family(mine.session().document(), TestWard::smithId()) == TestWard::smith());
    QVERIFY(setupIn(mine.session().document()) == setupIn(ward));
    QVERIFY(!mine.session().document().findTeamById(cleanup.id()).has_value());
    QVERIFY(referencesAreWhole(mine.session().document()));
}

void ReferenceCleanupTest::undoingARemovalLeavesALeaderSetSince()
{
    Document ward = TestWard::documentWithPlaces();
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);
    Team setup = setupIn(ward);
    QVERIFY(setup.leaderId() == TestWard::bobId());

    Team aliceLeads = setup;
    aliceLeads.setLeaderId(TestWard::aliceId());
    QVERIFY(theirs.session().commit(UpdateTeamCommand::fromEdit(setup, aliceLeads, theirs.session().document())).status == CommitStatus::Applied);
    QVERIFY(removeFromSmiths(mine.session(), TestWard::bobId()).status == CommitStatus::Applied);
    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);
    Team aliceOnly = withMembers(aliceLeads, {TestWard::aliceId()});
    QVERIFY(aliceOnly.leaderId() == TestWard::aliceId());
    QVERIFY(setupIn(mine.session().document()) == aliceOnly);

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(setupIn(mine.session().document()) == aliceLeads);
    QVERIFY(generatorIn(mine.session().document()) == TestWard::generator());
    QVERIFY(groupIn(mine.session().document(), EQ, TestWard::eqGroupId()) == TestWard::eqGroup());
    QVERIFY(districtIn(mine.session().document()) == TestWard::district());
}

void ReferenceCleanupTest::undoingADirectoryImportRestoresTheRemovedPeoplesMemberships()
{
    Document places = TestWard::documentWithPlaces();
    EmergencyAsset generator = TestWard::generator();
    generator.setPersonIds({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});
    Document ward = TestWard::withAsset(TestWard::withTeam(places, withMembers(setupIn(places), {TestWard::bobId(),
                                                                                                   TestWard::aliceId(),
                                                                                                   TestWard::carolId()})),
                                        generator);
    TestClient client("Laptop");
    client.createWard(ward);
    WardDirectoryImportResult result;
    result.success = true;
    result.families.insert(TestWard::smithId(), TestWard::smith());
    result.removedFamilyIds.insert(TestWard::jonesId());
    result.wardName = "Tanglewood";
    result.wardUnitNumber = "123456";
    result.pdfDate = QDate(2026, 10, 15);

    QVERIFY(client.session().commit(ImportWardDirectoryCommand::fromImport(client.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);

    const Document& imported = client.session().document();
    QVERIFY(!imported.findFamilyById(TestWard::jonesId()).has_value());
    QVERIFY(setupIn(imported) == setupIn(places));
    QVERIFY(generatorIn(imported) == TestWard::generator());
    MinisteringGroup rsGroup = TestWard::rsGroup();
    rsGroup.setMinisterIds({});
    QVERIFY(groupIn(imported, RS, TestWard::rsGroupId()) == rsGroup);
    MinisteringGroup eqGroup = TestWard::eqGroup();
    eqGroup.setFamilyIds({TestWard::smithId()});
    QVERIFY(groupIn(imported, EQ, TestWard::eqGroupId()) == eqGroup);
    QVERIFY(referencesAreWhole(imported));

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(TestWard::family(client.session().document(), TestWard::jonesId()) == TestWard::jones());
    QVERIFY(client.session().document().teams() == ward.teams());
    QVERIFY(client.session().document().emergencyAssets() == ward.emergencyAssets());
    QVERIFY(client.session().document().groups(EQ) == ward.groups(EQ));
    QVERIFY(client.session().document().groups(RS) == ward.groups(RS));
    QVERIFY(client.session().document().districts(EQ) == ward.districts(EQ));
}

void ReferenceCleanupTest::ministeringImportReplayedAfterACleanupChangedAGroupConflicts()
{
    Document ward = TestWard::documentWithPlaces();
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(ward);
    theirs.takeWardOf(mine);

    QVERIFY(removeFromSmiths(theirs.session(), TestWard::aliceId()).status == CommitStatus::Applied);
    MinisteringGroup cleaned = TestWard::eqGroup();
    cleaned.setPresidencyMemberId(std::nullopt);
    QVERIFY(groupIn(theirs.session().document(), EQ, TestWard::eqGroupId()) == cleaned);

    MinisteringImportResult result;
    result.success = true;
    result.families = mine.session().document().families();
    MinisteringGroup imported = MinisteringGroup::createEQ({TestWard::bobId()}, {TestWard::jonesId()});
    result.groups.insert(imported.id(), imported);
    result.pdfDate = QDate(2026, 9, 1);
    QVERIFY(mine.session().commit(ImportMinisteringCommand::fromImport(mine.session().document(), result, "Tanglewood EQ.pdf")).status
            == CommitStatus::Applied);
    QVERIFY(mine.session().document().groups(EQ) == (QHash<MinisteringGroupId, MinisteringGroup>{{imported.id(), imported}}));

    ReplayResult replay = mine.session().replayOnto(theirs.session().document());

    QVERIFY(replay.status == ReplayStatus::Abandoned);
    QVERIFY(replay.toRedo.contains("Import the ministering assignments from Tanglewood EQ.pdf. (stopped here)"));
    QVERIFY(mine.session().document().groups(EQ) == (QHash<MinisteringGroupId, MinisteringGroup>{{TestWard::eqGroupId(), cleaned}}));
    QVERIFY(mine.session().document() == theirs.session().document());
}

void ReferenceCleanupTest::undoingAFamilyDeleteLeavesEachPlaceSomeoneElseChanged()
{
    // Bob also holds the Gutter task, so the deletion holds two tasks: Alice's Shingles and
    // Bob's Gutter
    ResponseTask gutter = TestWard::task("task-gutter", TestWard::jones(), TestWard::octoberFirst(11, 0), "Repairs",
                                         "Gutter hanging loose");
    gutter.setAssignment(TaskAssignment::toPerson(TestWard::bob()));
    Document ward = withTask(TestWard::documentWithPlaces(), gutter);
    TestClient mine("Laptop");
    mine.createWard(ward);
    QVERIFY(mine.session().commit(DeleteFamilyCommand::fromSeen(TestWard::smith(), mine.session().document())).status == CommitStatus::Applied);

    // Someone else, on the version after the deletion: deletes the Generator, replaces the
    // elders quorum groups and gives the district's presidency role to Carol, puts Carol on
    // Setup as its leader, gives Shingles to Carol, and deletes Gutter
    TestClient theirs("Phone");
    theirs.takeWardOf(mine);
    QVERIFY(theirs.session().commit(DeleteEmergencyAssetCommand::fromSeen(generatorIn(theirs.session().document()), theirs.session().document())).status
            == CommitStatus::Applied);
    MinisteringGroup replacement = MinisteringGroup::createEQ({TestWard::carolId()}, {TestWard::jonesId()});
    QJsonObject district = districtIn(theirs.session().document()).toJson();
    district["presidencyMemberId"] = TestWard::carolId().toString();
    district["groupIds"] = QJsonArray({replacement.id().toString()});
    MinisteringDistrict carolPresides = MinisteringDistrict::fromJson(JsonReader(district));
    MinisteringImportResult ministering;
    ministering.success = true;
    ministering.families = theirs.session().document().families();
    ministering.groups.insert(replacement.id(), replacement);
    ministering.districts.insert(carolPresides.id(), carolPresides);
    ministering.pdfDate = QDate(2026, 9, 1);
    QVERIFY(theirs.session().commit(ImportMinisteringCommand::fromImport(theirs.session().document(), ministering, "Tanglewood EQ.pdf")).status
            == CommitStatus::Applied);
    Team carolLeads = withMembers(setupIn(theirs.session().document()), {TestWard::carolId()});
    carolLeads.setLeaderId(TestWard::carolId());
    QVERIFY(theirs.session().commit(UpdateTeamCommand::fromEdit(setupIn(theirs.session().document()), carolLeads, theirs.session().document())).status
            == CommitStatus::Applied);
    ResponseTask carolShingles = taskIn(theirs.session().document(), TestWard::shinglesTaskId());
    QVERIFY(!carolShingles.assignment().has_value());
    carolShingles.setAssignment(TaskAssignment::toPerson(TestWard::carol()));
    QVERIFY(theirs.session().commit(UpdateTaskCommand::fromEdit(taskIn(theirs.session().document(), TestWard::shinglesTaskId()), carolShingles))
            .status == CommitStatus::Applied);
    QVERIFY(theirs.session().commit(DeleteTaskCommand::fromSeen(taskIn(theirs.session().document(), gutter.id()))).status
            == CommitStatus::Applied);

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);
    QVERIFY(mine.session().document() == theirs.session().document());

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    const Document& restored = mine.session().document();
    QVERIFY(TestWard::family(restored, TestWard::smithId()) == TestWard::smith());
    QVERIFY(!restored.findEmergencyAssetById(TestWard::generatorId()).has_value());
    QVERIFY(restored.groups(EQ) == (QHash<MinisteringGroupId, MinisteringGroup>{{replacement.id(), replacement}}));
    QVERIFY(restored.districts(EQ)
            == (QHash<MinisteringDistrictId, MinisteringDistrict>{{carolPresides.id(), carolPresides}}));
    QVERIFY(taskIn(restored, TestWard::shinglesTaskId()) == carolShingles);
    QVERIFY(restored.emergencyResponse()->findTask(gutter.id()) == nullptr);

    // The places that still stand as the deletion left them take Bob and Alice back: Setup
    // its members, under Carol as leader, and the Relief Society group Alice
    Team setup = withMembers(carolLeads, {TestWard::carolId(), TestWard::bobId(), TestWard::aliceId()});
    QVERIFY(setup.leaderId() == TestWard::carolId());
    QVERIFY(setupIn(restored) == setup);
    QVERIFY(groupIn(restored, RS, TestWard::rsGroupId()) == TestWard::rsGroup());
    QVERIFY(taskIn(restored, TestWard::basementTaskId()) == taskIn(ward, TestWard::basementTaskId()));

    Document expected = theirs.session().document();
    expected.setFamily(TestWard::smith());
    expected.setTeam(setup);
    expected.setGroups(RS, ward.groups(RS));
    QVERIFY(restored == expected);
    QVERIFY(referencesAreWhole(restored));
}

void ReferenceCleanupTest::undoingAnImportRestoresTheMemberItRemovedFromAFamily()
{
    Document ward = TestWard::documentWithPlaces();
    TestClient client("Laptop");
    client.createWard(ward);
    WardDirectoryImportResult result = directoryImport({smithWithout(TestWard::aliceId()), TestWard::jones()});

    QVERIFY(client.session().commit(ImportWardDirectoryCommand::fromImport(client.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);

    const Document& imported = client.session().document();
    QVERIFY(TestWard::family(imported, TestWard::smithId()) == smithWithout(TestWard::aliceId()));
    QVERIFY(TestWard::family(imported, TestWard::jonesId()) == TestWard::jones());
    Team setup = withMembers(setupIn(ward), {TestWard::bobId()});
    QVERIFY(setup.leaderId() == TestWard::bobId());
    QVERIFY(setupIn(imported) == setup);
    EmergencyAsset generator = TestWard::generator();
    generator.setPersonIds({TestWard::bobId()});
    QVERIFY(generatorIn(imported) == generator);
    MinisteringGroup eqGroup = TestWard::eqGroup();
    eqGroup.setPresidencyMemberId(std::nullopt);
    QVERIFY(groupIn(imported, EQ, TestWard::eqGroupId()) == eqGroup);
    MinisteringGroup rsGroup = TestWard::rsGroup();
    rsGroup.setMinisteredPersonIds({});
    QVERIFY(groupIn(imported, RS, TestWard::rsGroupId()) == rsGroup);
    QVERIFY(taskIn(imported, TestWard::shinglesTaskId()) == unassigned(taskIn(ward, TestWard::shinglesTaskId())));
    QVERIFY(inNoPlace(imported, TestWard::aliceId()));
    QVERIFY(referencesAreWhole(imported));

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    const Document& restored = client.session().document();
    QVERIFY(restored.families() == ward.families());
    QVERIFY(restored.teams() == ward.teams());
    QVERIFY(restored.emergencyAssets() == ward.emergencyAssets());
    QVERIFY(restored.groups(EQ) == ward.groups(EQ));
    QVERIFY(restored.groups(RS) == ward.groups(RS));
    QVERIFY(restored.districts(EQ) == ward.districts(EQ));
    QVERIFY(restored.emergencyResponse() == ward.emergencyResponse());
}

void ReferenceCleanupTest::secondUndoOfOneFamilyDeletePutsBackOnlyTheMembersItStillHolds()
{
    Document ward = TestWard::documentWithPlaces();
    TestClient client("Laptop");
    client.createWard(ward);
    Document created = client.session().document();

    // Two users delete the Smith family from the same version; both deletions land, the
    // second with its result already holding
    UndoableCommandPtr first = DeleteFamilyCommand::fromSeen(TestWard::smith(), ward);
    UndoableCommandPtr second = DeleteFamilyCommand::fromSeen(TestWard::smith(), ward);
    QVERIFY(client.session().commit(first).status == CommitStatus::Applied);
    QVERIFY(client.session().commit(second).status == CommitStatus::Applied);

    // The second user undoes, and someone removes Alice from the family
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(client.session().document() == created);
    QVERIFY(removeFromSmiths(client.session(), TestWard::aliceId()).status == CommitStatus::Applied);
    QVERIFY(inNoPlace(client.session().document(), TestWard::aliceId()));

    // The first user's undo lands on that version
    QVERIFY(client.session().commit(first->inverse()).status == CommitStatus::Applied);

    const Document& restored = client.session().document();
    QVERIFY(TestWard::family(restored, TestWard::smithId()) == smithWithout(TestWard::aliceId()));
    QVERIFY(!restored.findPersonById(TestWard::aliceId()).has_value());
    QVERIFY(inNoPlace(restored, TestWard::aliceId()));
    QVERIFY(referencesAreWhole(restored));

    // Bob is back in each place the first deletion held him
    Team setup = withMembers(setupIn(ward), {TestWard::bobId()});
    QVERIFY(setup.leaderId() == TestWard::bobId());
    QVERIFY(setupIn(restored) == setup);
    EmergencyAsset generator = TestWard::generator();
    generator.setPersonIds({TestWard::bobId()});
    QVERIFY(generatorIn(restored) == generator);
    MinisteringGroup eqGroup = TestWard::eqGroup();
    eqGroup.setPresidencyMemberId(std::nullopt);
    QVERIFY(groupIn(restored, EQ, TestWard::eqGroupId()) == eqGroup);
    QVERIFY(districtIn(restored) == TestWard::district());
}

void ReferenceCleanupTest::undoingAnImportRestoresALeaderWhateverTheFamilyOrder_data()
{
    QTest::addColumn<QString>("leader");
    QTest::addColumn<QString>("losing");
    QTest::addColumn<QString>("gaining");

    QTest::newRow("Bob leaves the Smiths, Dan joins the Joneses")
        << TestWard::bobId().toString() << TestWard::smithId().toString() << TestWard::jonesId().toString();
    QTest::newRow("Carol leaves the Joneses, Dan joins the Smiths")
        << TestWard::carolId().toString() << TestWard::jonesId().toString() << TestWard::smithId().toString();
}

void ReferenceCleanupTest::undoingAnImportRestoresALeaderWhateverTheFamilyOrder()
{
    QFETCH(QString, leader);
    QFETCH(QString, losing);
    QFETCH(QString, gaining);
    PersonId leaderId = PersonId::fromString(leader);

    // Setup holds the leader and Alice, led by the leader
    Document places = TestWard::documentWithPlaces();
    Team setup = withMembers(setupIn(places), {leaderId, TestWard::aliceId()});
    setup.setLeaderId(leaderId);
    Document ward = TestWard::withTeam(places, setup);
    Family losingFamily = TestWard::family(ward, FamilyId::fromString(losing));
    Family gainingFamily = TestWard::family(ward, FamilyId::fromString(gaining));

    // The import takes the leader out of one family and adds Dan to the other
    TestClient mine("Laptop");
    mine.createWard(ward);
    WardDirectoryImportResult result = directoryImport({withoutMember(losingFamily, leaderId),
                                                        withAddedMember(gainingFamily, TestWard::dan())});
    QVERIFY(mine.session().commit(ImportWardDirectoryCommand::fromImport(mine.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    Team aliceOnly = withMembers(setup, {TestWard::aliceId()});
    aliceOnly.setLeaderId(std::nullopt);
    QVERIFY(setupIn(mine.session().document()) == aliceOnly);

    // Someone else then puts Dan on Setup as its leader
    TestClient theirs("Phone");
    theirs.takeWardOf(mine);
    Team danLeads = withMembers(aliceOnly, {TestWard::aliceId(), TestWard::danId()});
    danLeads.setLeaderId(TestWard::danId());
    QVERIFY(theirs.session().commit(UpdateTeamCommand::fromEdit(aliceOnly, danLeads, theirs.session().document())).status
            == CommitStatus::Applied);
    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);
    QVERIFY(setupIn(mine.session().document()) == danLeads);

    CommitResult undone = mine.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    const Document& restored = mine.session().document();
    QVERIFY(TestWard::family(restored, losingFamily.id()) == losingFamily);
    QVERIFY(TestWard::family(restored, gainingFamily.id()) == gainingFamily);
    QVERIFY(setupIn(restored) == setup);
    QVERIFY(inNoPlace(restored, TestWard::danId()));
    QVERIFY(referencesAreWhole(restored));
}
