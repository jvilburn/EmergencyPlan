#include "AutomaticUpdatesTest.h"
#include "AddressCoordinatesCommands.h"
#include "Crypto.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"

#include <QSignalSpy>
#include <QTest>
#include <memory>

namespace
{

/// A test command that sets a ward in the document's ward details and gives the document
/// its unit. It stands in for a command that gives units, and gives no address. It is not
/// in CommandRegistry, so it never takes part in a replay.
class GiveWardCommand : public Command
{
public:
    explicit GiveWardCommand(const Ward& ward);

    QString type() const override { return "testGiveWard"; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;
    QStringList unitsGiven() const override;

private:
    Ward m_ward;
};

GiveWardCommand::GiveWardCommand(const Ward& ward)
    : Command(CommandDescription("Give ward", QStringList()))
    , m_ward(ward)
{
}

CommandOutcome GiveWardCommand::execute(Document& document) const
{
    document.metadata().updateWard(m_ward);
    return CommandOutcome::Applied;
}

QJsonObject GiveWardCommand::toJson() const
{
    QJsonObject json;
    json["ward"] = m_ward.toJson();
    return json;
}

DocumentChange GiveWardCommand::documentChange() const
{
    return DocumentChange::metadata().updated();
}

QStringList GiveWardCommand::unitsGiven() const
{
    return QStringList({m_ward.unitNumber()});
}

const Coordinates ElmPosition(40.2338, -111.6585);
const Coordinates OakPosition(40.2410, -111.6520);
const Coordinates NewElmPosition(40.2342, -111.6590);
const Coordinates TypedPosition(40.2339, -111.6587);

Address smithAddress()
{
    return TestWard::smith().address();
}

Address jonesAddress()
{
    return TestWard::jones().address();
}

/// The test ward with an entry for each family's address.
Document mappedWard()
{
    Document document = TestWard::withCoordinates(TestWard::document(), smithAddress(), ElmPosition);
    return TestWard::withCoordinates(document, jonesAddress(), OakPosition);
}

/// Where the family is on the map in the session's document.
std::optional<Coordinates> positionOf(const WardSession& session, const FamilyId& id)
{
    return session.document().coordinatesFor(TestWard::family(session.document(), id).address());
}

/// Commits an address edit to the family, opened on the session's current family.
CommitResult commitAddress(WardSession& session, const FamilyId& familyId, const Address& address)
{
    Family opened = TestWard::family(session.document(), familyId);
    Family saved = opened;
    saved.setAddress(address);
    return session.commit(UpdateFamilyCommand::fromEdit(opened, saved, session.document()));
}

/// Commits a phone edit to the member, opened on the session's current family.
CommitResult commitPhone(WardSession& session, const FamilyId& familyId, const PersonId& personId, const QString& phone)
{
    Family opened = TestWard::family(session.document(), familyId);
    return session.commit(UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, personId, phone), session.document()));
}

/// Commits coordinates typed for the address, against the session's current entry.
CommitResult commitTyped(WardSession& session, const Address& address, const Coordinates& typed)
{
    return session.commit(SetAddressCoordinatesCommand::fromTyped(session.document(), address, typed));
}

/// A ward with that unit number and no details.
Ward wardWithoutDetails(const QString& unitNumber)
{
    Ward ward;
    ward.setUnitNumber(unitNumber);
    return ward;
}

/// A ward with that unit number and a chapel, naming the stake with stakeUnitNumber; it
/// names no stake when stakeUnitNumber is empty.
Ward wardWithChapel(const QString& unitNumber, const QString& stakeUnitNumber)
{
    Ward ward = wardWithoutDetails(unitNumber);
    ward.setChapelAddress(QString("100 Chapel Rd, Provo, UT"));
    ward.setStakeUnitNumber(stakeUnitNumber);
    return ward;
}

/// A stake with that unit number and a name.
Stake namedStake(const QString& unitNumber)
{
    Stake stake;
    stake.setUnitNumber(unitNumber);
    stake.setName("Provo North");
    return stake;
}

}  // namespace

AutomaticUpdatesTest::AutomaticUpdatesTest(QObject* parent)
    : QObject(parent)
{
}

void AutomaticUpdatesTest::geocodeResultAppliesOutsideTheJournal()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));

    client.geocoder().resolve(smithAddress().full(), ElmPosition);
    client.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(client.session().journal().isEmpty());
    QVERIFY(!client.session().canUndo());

    ReplayResult result = client.session().replayOnto(TestWard::document());

    QVERIFY(result.status == ReplayStatus::FastForwarded);
    QVERIFY(!positionOf(client.session(), TestWard::smithId()).has_value());
    QVERIFY(client.session().journal().isEmpty());
}

void AutomaticUpdatesTest::geocodeBetweenAnOfflinePhoneEditAndItsReplayKeepsBoth()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    QVERIFY(commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(!positionOf(mine.session(), TestWard::smithId()).has_value());

    // Another device's geocode of the Smith address landed in the version.
    ReplayResult result = mine.session().replayOnto(TestWard::withCoordinates(TestWard::document(), smithAddress(), ElmPosition));

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(QString(TestWard::person(mine.session().document(), TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(positionOf(mine.session(), TestWard::smithId()) == ElmPosition);
    QCOMPARE(mine.session().journal().entries().size(), qsizetype(1));
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::addressEditPlacesTheFamilyAtTheNewAddressEntry()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");

    // The old address's geocode lands before the edit.
    TestClient before("Laptop");
    before.createWard(TestWard::document());
    before.session().automaticUpdates().run();
    before.geocoder().resolve(smithAddress().full(), ElmPosition);
    before.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(positionOf(before.session(), TestWard::smithId()) == ElmPosition);

    QVERIFY(commitAddress(before.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QVERIFY(!positionOf(before.session(), TestWard::smithId()).has_value());
    QCOMPARE(before.geocoder().requests().last(), newAddress.full());

    before.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(before.session(), TestWard::smithId()) == NewElmPosition);
    QVERIFY(!before.session().document().addressCoordinates().contains(smithAddress().full()));
    QCOMPARE(before.session().document().addressCoordinates().size(), qsizetype(2));

    // The old address's geocode lands after the edit.
    TestClient after("Laptop");
    after.createWard(TestWard::document());
    after.session().automaticUpdates().run();
    QVERIFY(commitAddress(after.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(after.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), newAddress.full()}));

    after.geocoder().resolve(smithAddress().full(), ElmPosition);
    after.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(!positionOf(after.session(), TestWard::smithId()).has_value());

    after.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(after.session(), TestWard::smithId()) == NewElmPosition);
    QVERIFY(!after.session().document().addressCoordinates().contains(smithAddress().full()));
    QCOMPARE(after.session().document().addressCoordinates().size(), qsizetype(2));
}

void AutomaticUpdatesTest::typedCoordinatesSetTheEntryAndAGeocodeNeverReplacesThem()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.session().automaticUpdates().run();

    QVERIFY(commitTyped(client.session(), smithAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == TypedPosition);
    QCOMPARE(client.session().undoHeading(), QString("Map position for 12 Elm St, Provo, UT"));

    client.geocoder().resolve(smithAddress().full(), ElmPosition);
    client.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(client.session(), TestWard::smithId()) == TypedPosition);
    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == OakPosition);
    QCOMPARE(client.session().journal().entries().size(), qsizetype(1));
}

void AutomaticUpdatesTest::undoOfTypedCoordinatesRestoresThePreviousEntry()
{
    TestClient client("Laptop");
    client.createWard(mappedWard());
    QVERIFY(commitTyped(client.session(), smithAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == TypedPosition);

    CommitResult undone = client.session().undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QCOMPARE(undone.description.heading(), QString("Undo: Map position for 12 Elm St, Provo, UT"));
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(client.session().canRedo());

    // An entry typed where there was none is removed by its undo.
    TestClient unmapped("Laptop");
    unmapped.createWard(TestWard::document());
    QVERIFY(commitTyped(unmapped.session(), jonesAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(unmapped.session(), TestWard::jonesId()) == TypedPosition);

    QVERIFY(unmapped.session().undo().status == CommitStatus::Applied);
    QVERIFY(!positionOf(unmapped.session(), TestWard::jonesId()).has_value());
}

void AutomaticUpdatesTest::typedCoordinatesReplayedAfterAGeocodeOfThatAddressApply()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    QVERIFY(commitTyped(mine.session(), smithAddress(), TypedPosition).status == CommitStatus::Applied);

    ReplayResult result = mine.session().replayOnto(mappedWard());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QVERIFY(positionOf(mine.session(), TestWard::smithId()) == TypedPosition);
    QVERIFY(positionOf(mine.session(), TestWard::jonesId()) == OakPosition);
    QCOMPARE(mine.session().journal().entries().size(), qsizetype(1));
}

void AutomaticUpdatesTest::batchDropsEntriesNoFamilyUsesAndKeepsEveryEntryInUse()
{
    Address gone = TestWard::address("99 Gone St", "Provo, UT");
    TestClient client("Laptop");
    client.createWard(TestWard::withCoordinates(mappedWard(), gone, NewElmPosition));
    int writesBefore = client.store().writes();

    client.session().automaticUpdates().run();

    QVERIFY(client.geocoder().requests().isEmpty());
    QCOMPARE(client.store().writes(), writesBefore + 1);
    QCOMPARE(client.session().document().addressCoordinates().size(), qsizetype(2));
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == OakPosition);
    QVERIFY(!client.session().document().addressCoordinates().contains(gone.full()));
}

void AutomaticUpdatesTest::batchOfSeveralAddressesSavesOnce()
{
    Family lee = TestWard::lee(2010);
    Document document = TestWard::document();
    document.setFamily(lee);
    document.onDocumentChanged(DocumentChange::full());
    TestClient client("Laptop");
    client.createWard(document);
    int writesBefore = client.store().writes();

    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests().size(), qsizetype(3));
    client.geocoder().resolve(smithAddress().full(), ElmPosition);
    client.geocoder().resolve(jonesAddress().full(), OakPosition);
    QCOMPARE(client.store().writes(), writesBefore);
    client.geocoder().resolve(lee.address().full(), NewElmPosition);

    QCOMPARE(client.store().writes(), writesBefore + 1);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == OakPosition);
    QVERIFY(positionOf(client.session(), lee.id()) == NewElmPosition);
}

void AutomaticUpdatesTest::failedAddressIsNotRequestedAgainUntilItsTextChanges()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.session().automaticUpdates().run();
    client.geocoder().fail(smithAddress().full());
    client.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(!positionOf(client.session(), TestWard::smithId()).has_value());

    // Not after a replay: the run that follows requests the Jones address the replay dropped,
    // and not the Smith address.
    client.session().replayOnto(TestWard::document());
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), jonesAddress().full()}));
    client.geocoder().resolve(jonesAddress().full(), OakPosition);

    // Not after another family is given a new address.
    Address newOak = TestWard::address("6 Oak Ave", "Provo, UT");
    QVERIFY(commitAddress(client.session(), TestWard::jonesId(), newOak).status == CommitStatus::Applied);
    QCOMPARE(client.geocoder().requests().last(), newOak.full());
    QCOMPARE(client.geocoder().requests().count(smithAddress().full()), qsizetype(1));
    client.geocoder().resolve(newOak.full(), OakPosition);

    // Requested again once its text changes.
    Address corrected = TestWard::address("12 Elm Street", "Provo, UT");
    QVERIFY(commitAddress(client.session(), TestWard::smithId(), corrected).status == CommitStatus::Applied);
    QCOMPARE(client.geocoder().requests().last(), corrected.full());
}

void AutomaticUpdatesTest::twoFamiliesAtOneAddressShareOneRequestAndOneEntry()
{
    Family jones = TestWard::jones();
    jones.setAddress(smithAddress());
    TestClient client("Laptop");
    client.createWard(TestWard::withFamily(TestWard::document(), jones));
    QSignalSpy finishedSpy(&client.session().automaticUpdates(), &AutomaticUpdates::geocodingFinished);

    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full()}));
    client.geocoder().resolve(smithAddress().full(), ElmPosition);

    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == ElmPosition);
    QCOMPARE(client.session().document().addressCoordinates().size(), qsizetype(1));
}

void AutomaticUpdatesTest::commandThatGivesNoAddressStartsNoGeocoding()
{
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(TestWard::document());
    theirs.takeWardOf(mine);
    QVERIFY(commitPhone(mine.session(), TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(mine.geocoder().requests().isEmpty());

    QVERIFY(commitAddress(theirs.session(), TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT")).status
            == CommitStatus::Applied);
    QVERIFY(!theirs.geocoder().requests().isEmpty());

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::Replayed);
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::batchThatChangesNothingSavesNothing()
{
    TestClient mapped("Laptop");
    mapped.createWard(mappedWard());
    int mappedWritesBefore = mapped.store().writes();
    QSignalSpy mappedChanged(&mapped.session(), &WardSession::documentChanged);
    mapped.session().automaticUpdates().run();
    QVERIFY(mapped.geocoder().requests().isEmpty());
    QCOMPARE(mapped.store().writes(), mappedWritesBefore);
    QCOMPARE(mappedChanged.count(), 0);

    // A batch whose every request fails.
    TestClient failing("Laptop");
    failing.createWard(TestWard::document());
    int failingWritesBefore = failing.store().writes();
    QSignalSpy failingChanged(&failing.session(), &WardSession::documentChanged);
    QSignalSpy failingFinished(&failing.session().automaticUpdates(), &AutomaticUpdates::geocodingFinished);
    failing.session().automaticUpdates().run();
    QCOMPARE(failing.geocoder().requests().size(), qsizetype(2));
    failing.geocoder().fail(smithAddress().full());
    failing.geocoder().fail(jonesAddress().full());
    QCOMPARE(failingFinished.count(), 1);
    QCOMPARE(failing.store().writes(), failingWritesBefore);
    QCOMPARE(failingChanged.count(), 0);
}

void AutomaticUpdatesTest::lookupThatChangesNothingSavesNothing()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);
    Document created = client.session().document();
    int writesBefore = client.store().writes();
    QSignalSpy changedSpy(&client.session(), &WardSession::documentChanged);

    client.session().automaticUpdates().run();
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));
    client.unitLookup().findWard("111111", wardWithoutDetails("111111"));

    QCOMPARE(client.store().writes(), writesBefore);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(client.session().document() == created);

    // A lookup that found no details puts the unit on the failed list.
    client.session().automaticUpdates().run();
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));
}

void AutomaticUpdatesTest::failedWardLookupContinuesToTheNextWard()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);

    client.session().automaticUpdates().run();
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));
    client.unitLookup().fail("111111");

    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));
}

void AutomaticUpdatesTest::wardFoundNamingAMissingStakeLooksUpTheStakeBeforeTheNextWard()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);
    int writesBefore = client.store().writes();

    client.session().automaticUpdates().run();
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));

    client.unitLookup().findWard("111111", wardWithChapel("111111", "900000"));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "stake 900000"}));
    QVERIFY(!client.session().document().stakes().contains("900000"));

    client.unitLookup().findStake("900000", namedStake("900000"));

    QVERIFY(client.session().document().metadata().findStakeByUnit("900000") == namedStake("900000"));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "stake 900000", "ward 222222"}));
    QCOMPARE(client.store().writes(), writesBefore + 2);
}

void AutomaticUpdatesTest::undoOfAnAddressEditRequestsTheRestoredAddress()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestClient client("Laptop");
    client.createWard(mappedWard());

    QVERIFY(commitAddress(client.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full()}));
    client.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == NewElmPosition);
    QVERIFY(!client.session().document().addressCoordinates().contains(smithAddress().full()));

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QCOMPARE(TestWard::family(client.session().document(), TestWard::smithId()).address().full(), smithAddress().full());
    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full(), smithAddress().full()}));
}

void AutomaticUpdatesTest::lateAnswersChangeNothing()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    Address gone = TestWard::address("99 Gone St", "Provo, UT");
    Document document = TestWard::document();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);
    Document created = client.session().document();
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));
    QSignalSpy changedSpy(&client.session(), &WardSession::documentChanged);
    int writesBefore = client.store().writes();
    QSignalSpy progressSpy(&client.session().automaticUpdates(), &AutomaticUpdates::geocodingProgress);

    // Answers for addresses the batch is not waiting on, and for units not being looked up.
    client.geocoder().resolve(gone.full(), NewElmPosition);
    client.geocoder().fail(newAddress.full());
    client.unitLookup().findWard("222222", wardWithChapel("222222", "900000"));
    client.unitLookup().findStake("900000", namedStake("900000"));
    client.unitLookup().fail("222222");

    QVERIFY(client.session().document() == created);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(client.store().writes(), writesBefore);
    QCOMPARE(progressSpy.count(), 0);
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));

    // An answer the batch is waiting on maps the family; the same address answered again
    // later changes nothing.
    client.geocoder().resolve(smithAddress().full(), ElmPosition);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    client.geocoder().resolve(smithAddress().full(), NewElmPosition);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);

    // The answer for the unit being looked up applies; the late failure of the other ward
    // left it off the failed list, so it is looked up next.
    client.unitLookup().findWard("111111", wardWithChapel("111111", QString()));
    QCOMPARE(client.store().writes(), writesBefore + 1);
    QCOMPARE(changedSpy.count(), 2);
    QVERIFY(!client.session().document().metadata().findWardByUnit("111111")->chapelAddress().isEmpty());
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));

    // The late failure of the new address left it off the failed list too.
    QVERIFY(commitAddress(client.session(), TestWard::jonesId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(client.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), newAddress.full()}));
}

void AutomaticUpdatesTest::wardFoundWithoutAChapelIsNotLookedUpAgain()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);
    int writesBefore = client.store().writes();
    client.session().automaticUpdates().run();
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));

    Ward found = wardWithoutDetails("111111");
    found.setMeetingTime(QString("Sunday 9:00 AM"));
    client.unitLookup().findWard("111111", found);
    QCOMPARE(client.store().writes(), writesBefore + 1);
    QVERIFY(client.session().document().metadata().findWardByUnit("111111")->meetingTime() == QString("Sunday 9:00 AM"));

    // A version taken as it stands brings another ward lacking its details; the next run
    // looks up that ward and not the one found without a chapel.
    Document version = client.session().document();
    version.metadata().updateWard(wardWithoutDetails("222222"));
    QVERIFY(client.session().replayOnto(version).status == ReplayStatus::FastForwarded);
    client.session().automaticUpdates().run();

    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));
}

void AutomaticUpdatesTest::failedListStartsEmptyWhenAWardIsOpenedOrCreated()
{
    Document document = TestWard::withCoordinates(TestWard::document(), jonesAddress(), OakPosition);
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestClient client("Laptop");
    client.createWard(document);
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full()}));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));
    client.geocoder().fail(smithAddress().full());
    client.unitLookup().fail("111111");

    // On the failed list, neither is requested again.
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full()}));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111"}));

    QVERIFY(client.session().openWard(client.session().documentId()).status == OpenStatus::Opened);
    client.session().automaticUpdates().run();

    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full(), smithAddress().full()}));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "ward 111111"}));
    client.geocoder().fail(smithAddress().full());
    client.unitLookup().fail("111111");

    client.createWard(document);
    client.session().automaticUpdates().run();

    QCOMPARE(client.geocoder().requests(),
             QStringList({smithAddress().full(), smithAddress().full(), smithAddress().full()}));
    QCOMPARE(client.unitLookup().requests(), QStringList({"ward 111111", "ward 111111", "ward 111111"}));
}

void AutomaticUpdatesTest::resultsMapFamiliesAsTheyArrive()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    QSignalSpy changedSpy(&client.session().automaticUpdates(), &AutomaticUpdates::documentChanged);
    int writesBefore = client.store().writes();
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));

    client.geocoder().resolve(smithAddress().full(), ElmPosition);

    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(!positionOf(client.session(), TestWard::jonesId()).has_value());
    QCOMPARE(changedSpy.count(), 1);
    DocumentChange first = qvariant_cast<DocumentChange>(changedSpy.at(0).at(0));
    QVERIFY(first.action == ChangeAction::Updated);
    QVERIFY(first.familyId == TestWard::smithId());
    QCOMPARE(client.store().writes(), writesBefore);

    client.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == OakPosition);
    QCOMPARE(changedSpy.count(), 2);
    DocumentChange second = qvariant_cast<DocumentChange>(changedSpy.at(1).at(0));
    QVERIFY(second.action == ChangeAction::Updated);
    QVERIFY(second.familyId == TestWard::jonesId());
    QCOMPARE(client.store().writes(), writesBefore + 1);
}

void AutomaticUpdatesTest::documentReplacedMidBatchLosesNoResults()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.session().automaticUpdates().run();
    client.geocoder().resolve(smithAddress().full(), ElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);

    QVERIFY(client.session().replayOnto(TestWard::document()).status == ReplayStatus::FastForwarded);
    QVERIFY(!positionOf(client.session(), TestWard::smithId()).has_value());
    int writesBefore = client.store().writes();

    client.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(client.session(), TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::jonesId()) == OakPosition);
    QCOMPARE(client.session().document().addressCoordinates().size(), qsizetype(2));
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
    QCOMPARE(client.store().writes(), writesBefore + 1);
}

void AutomaticUpdatesTest::commandThatGivesAWardNeedingALookupStartsOne()
{
    // A ward lacking its details, from a command that gives no address.
    TestClient lacking("Laptop");
    lacking.createWard(mappedWard());
    QVERIFY(lacking.session().commit(std::make_shared<GiveWardCommand>(wardWithoutDetails("111111"))).status
            == CommitStatus::Applied);
    QCOMPARE(lacking.unitLookup().requests(), QStringList({"ward 111111"}));
    QVERIFY(lacking.geocoder().requests().isEmpty());

    // A ward with its details, naming a stake the document lacks.
    TestClient stakeMissing("Laptop");
    stakeMissing.createWard(mappedWard());
    QVERIFY(stakeMissing.session().commit(std::make_shared<GiveWardCommand>(wardWithChapel("111111", "900000"))).status
            == CommitStatus::Applied);
    QCOMPARE(stakeMissing.unitLookup().requests(), QStringList({"stake 900000"}));

    // A ward with its details, naming a stake the document has, starts no run, though a run
    // would look up the other ward and geocode both addresses.
    Document document = TestWard::document();
    document.metadata().updateStake(namedStake("900000"));
    document.metadata().updateWard(wardWithoutDetails("222222"));
    TestClient complete("Laptop");
    complete.createWard(document);
    QVERIFY(complete.session().commit(std::make_shared<GiveWardCommand>(wardWithChapel("111111", "900000"))).status
            == CommitStatus::Applied);
    QVERIFY(complete.session().document().metadata().findWardByUnit("111111").has_value());
    QVERIFY(complete.unitLookup().requests().isEmpty());
    QVERIFY(complete.geocoder().requests().isEmpty());

    complete.session().automaticUpdates().run();
    QCOMPARE(complete.unitLookup().requests(), QStringList({"ward 222222"}));
    QCOMPARE(complete.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
}

void AutomaticUpdatesTest::addressEditFromAnotherDeviceLeavesTheFamilyUnmappedAndRequestsNothing()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(mappedWard());
    theirs.takeWardOf(mine);
    QVERIFY(commitAddress(theirs.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(theirs.geocoder().requests(), QStringList({newAddress.full()}));

    QVERIFY(mine.session().replayOnto(theirs.session().document()).status == ReplayStatus::FastForwarded);

    QCOMPARE(TestWard::family(mine.session().document(), TestWard::smithId()).address().full(), newAddress.full());
    QVERIFY(!positionOf(mine.session(), TestWard::smithId()).has_value());
    QVERIFY(positionOf(mine.session(), TestWard::jonesId()) == OakPosition);
    QVERIFY(mine.session().document().coordinatesFor(smithAddress()) == ElmPosition);
    for (const Family& family : mine.session().document().families())
    {
        QVERIFY(family.address().full() != smithAddress().full());
    }
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::takenUpdateRequestsAnAddressItLeavesUnmapped()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    TestClient::holdWard({&mine, &theirs}, {&mine, &theirs}, DocumentId::generate(),
                         Crypto::randomBytes(Crypto::KeyBytes), mappedWard());
    QVERIFY(commitAddress(theirs.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);

    QVERIFY(mine.session().takeUpdate(theirs.session().writeUpdate()).status == TakeStatus::Taken);

    QCOMPARE(mine.geocoder().requests(), QStringList({newAddress.full()}));
    mine.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(mine.session(), TestWard::smithId()) == NewElmPosition);
}

void AutomaticUpdatesTest::lookUpOfAnAddressSavedWhileItIsPendingMapsTheFamilyWithOneRequest()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestClient client("Laptop");
    client.createWard(mappedWard());
    QSignalSpy answered(&client.session().automaticUpdates(), &AutomaticUpdates::answered);

    client.session().automaticUpdates().lookUp(newAddress.full());
    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full()}));
    QVERIFY(commitAddress(client.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full()}));
    QVERIFY(!positionOf(client.session(), TestWard::smithId()).has_value());

    client.geocoder().resolve(newAddress.full(), NewElmPosition);

    QVERIFY(positionOf(client.session(), TestWard::smithId()) == NewElmPosition);
    QCOMPARE(answered.size(), 1);
    QCOMPARE(answered.first().first().toString(), newAddress.full());
}

void AutomaticUpdatesTest::lookUpRequestsAFailedAddressAgain()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestClient client("Laptop");
    client.createWard(mappedWard());
    QVERIFY(commitAddress(client.session(), TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    client.geocoder().fail(newAddress.full());
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full()}));

    client.session().automaticUpdates().lookUp(newAddress.full());

    QCOMPARE(client.geocoder().requests(), QStringList({newAddress.full(), newAddress.full()}));
    client.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(client.session(), TestWard::smithId()) == NewElmPosition);
}

void AutomaticUpdatesTest::lookUpOfAnAddressNoFamilyHasAnswersAndAddsNoEntry()
{
    Address unsaved = TestWard::address("14 Elm St", "Provo, UT");
    TestClient client("Laptop");
    client.createWard(mappedWard());
    QSignalSpy answered(&client.session().automaticUpdates(), &AutomaticUpdates::answered);

    client.session().automaticUpdates().lookUp(unsaved.full());
    client.geocoder().resolve(unsaved.full(), NewElmPosition);

    QCOMPARE(answered.size(), 1);
    QCOMPARE(answered.first().first().toString(), unsaved.full());
    QVERIFY(client.session().document().addressCoordinates() == mappedWard().addressCoordinates());
}
