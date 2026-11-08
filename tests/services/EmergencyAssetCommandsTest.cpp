#include "EmergencyAssetCommandsTest.h"
#include "EmergencyAssetCommands.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"

#include <QTest>

namespace
{

EmergencyAsset assetIn(const WardSession& session)
{
    return session.document().findEmergencyAssetById(TestWard::generatorId()).value();
}

CommitResult commitAssetEdit(WardSession& session, const EmergencyAsset& opened, const EmergencyAsset& saved)
{
    return session.commit(UpdateEmergencyAssetCommand::fromEdit(opened, saved, session.document()));
}

EmergencyAsset withPeople(const EmergencyAsset& asset, const QSet<PersonId>& people)
{
    EmergencyAsset updated = asset;
    updated.setPersonIds(people);
    return updated;
}

/// Creates a ward from the test ward holding asset in mine, and gives theirs the same ward.
void openBoth(TestClient& mine, TestClient& theirs, const EmergencyAsset& asset)
{
    mine.createWard(TestWard::withAsset(TestWard::document(), asset));
    theirs.takeWardOf(mine);
}

}  // namespace

EmergencyAssetCommandsTest::EmergencyAssetCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void EmergencyAssetCommandsTest::assetEditsAddingDifferentPeopleBothLand()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    EmergencyAsset bobOnly = withPeople(TestWard::generator(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    QVERIFY(commitAssetEdit(theirs.session(), bobOnly, withPeople(bobOnly, {TestWard::bobId(), TestWard::carolId()})).status
            == CommitStatus::Applied);
    QVERIFY(commitAssetEdit(mine.session(), bobOnly, withPeople(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(assetIn(mine.session()).personIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()}));
    QCOMPARE(mine.session().document().personResponseAreas(TestWard::carolId()), QSet<ResponseArea>({ResponseArea::Recovery}));
}

void EmergencyAssetCommandsTest::removalOfAPersonAlreadyRemovedIsDone()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    EmergencyAsset opened = TestWard::generator();
    openBoth(mine, theirs, opened);

    QVERIFY(commitAssetEdit(theirs.session(), opened, withPeople(opened, {TestWard::bobId()})).status == CommitStatus::Applied);
    EmergencyAsset saved = withPeople(opened, {TestWard::bobId()});
    saved.setName("Generator, 5 kW");
    QVERIFY(commitAssetEdit(mine.session(), opened, saved).status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(assetIn(mine.session()).personIds(), QSet<PersonId>({TestWard::bobId()}));
    QCOMPARE(assetIn(mine.session()).name(), QString("Generator, 5 kW"));
}

void EmergencyAssetCommandsTest::additionOfAPersonReplayedAfterTheyWereRemovedConflicts()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    EmergencyAsset bobOnly = withPeople(TestWard::generator(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    Family smith = TestWard::smith();
    Family withoutAlice = smith;
    withoutAlice.setMembers({TestWard::bob()});
    QVERIFY(theirs.session().commit(UpdateFamilyCommand::fromEdit(smith, withoutAlice, theirs.session().document())).status == CommitStatus::Applied);
    QVERIFY(commitAssetEdit(mine.session(), bobOnly, withPeople(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(theirs.session().document());

    QVERIFY(result.status == ReplayStatus::Abandoned);
    QVERIFY(!mine.session().document().findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(assetIn(mine.session()).personIds(), QSet<PersonId>({TestWard::bobId()}));
}

void EmergencyAssetCommandsTest::undoTakesBackAnAddAnEditAndADeletion()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    EmergencyAsset asset = TestWard::generator();

    QVERIFY(client.session().commit(AddEmergencyAssetCommand::fromNew(asset, client.session().document())).status == CommitStatus::Applied);
    QVERIFY(assetIn(client.session()) == asset);
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(!client.session().document().findEmergencyAssetById(TestWard::generatorId()).has_value());
    QVERIFY(client.session().redo().status == CommitStatus::Applied);
    QVERIFY(assetIn(client.session()) == asset);

    EmergencyAsset saved = withPeople(asset, {TestWard::bobId(), TestWard::carolId()});
    saved.setName("Generator, 5 kW");
    QVERIFY(commitAssetEdit(client.session(), asset, saved).status == CommitStatus::Applied);
    QVERIFY(assetIn(client.session()) == saved);
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(assetIn(client.session()) == asset);

    QVERIFY(client.session().commit(DeleteEmergencyAssetCommand::fromSeen(asset, client.session().document())).status
            == CommitStatus::Applied);
    QVERIFY(!client.session().document().findEmergencyAssetById(TestWard::generatorId()).has_value());
    QVERIFY(client.session().undo().status == CommitStatus::Applied);
    QVERIFY(assetIn(client.session()) == asset);
}
