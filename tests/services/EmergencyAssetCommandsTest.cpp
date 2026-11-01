#include "EmergencyAssetCommandsTest.h"
#include "EmergencyAssetCommands.h"
#include "TestSession.h"
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

/// Opens both sessions on the test ward holding asset.
void openBoth(WardSession& mine, WardSession& theirs, const EmergencyAsset& asset)
{
    Document document = TestWard::withAsset(TestWard::document(), asset);
    mine.open(document);
    theirs.open(document);
}

}  // namespace

EmergencyAssetCommandsTest::EmergencyAssetCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void EmergencyAssetCommandsTest::assetEditsAddingDifferentPeopleBothLand()
{
    TestSession mine;
    TestSession theirs;
    EmergencyAsset bobOnly = withPeople(TestWard::generator(), {TestWard::bobId()});
    openBoth(mine, theirs, bobOnly);

    QVERIFY(commitAssetEdit(theirs, bobOnly, withPeople(bobOnly, {TestWard::bobId(), TestWard::carolId()})).status
            == CommitStatus::Applied);
    QVERIFY(commitAssetEdit(mine, bobOnly, withPeople(bobOnly, {TestWard::bobId(), TestWard::aliceId()})).status
            == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(assetIn(mine).personIds(), QSet<PersonId>({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()}));
    QCOMPARE(mine.document().personResponseAreas(TestWard::carolId()), QSet<ResponseArea>({ResponseArea::Recovery}));
}

void EmergencyAssetCommandsTest::removalOfAPersonAlreadyRemovedIsDone()
{
    TestSession mine;
    TestSession theirs;
    EmergencyAsset opened = TestWard::generator();
    openBoth(mine, theirs, opened);

    QVERIFY(commitAssetEdit(theirs, opened, withPeople(opened, {TestWard::bobId()})).status == CommitStatus::Applied);
    EmergencyAsset saved = withPeople(opened, {TestWard::bobId()});
    saved.setName("Generator, 5 kW");
    QVERIFY(commitAssetEdit(mine, opened, saved).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(theirs.document());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(assetIn(mine).personIds(), QSet<PersonId>({TestWard::bobId()}));
    QCOMPARE(assetIn(mine).name(), QString("Generator, 5 kW"));
}
