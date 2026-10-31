#include "AutomaticUpdatesTest.h"
#include "AddressCoordinatesCommands.h"
#include "FamilyCommands.h"
#include "TestSession.h"
#include "TestWard.h"

#include <QSignalSpy>
#include <QTest>

namespace
{

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
    return session.commit(UpdateFamilyCommand::fromEdit(opened, saved));
}

/// Commits a phone edit to the member, opened on the session's current family.
CommitResult commitPhone(WardSession& session, const FamilyId& familyId, const PersonId& personId, const QString& phone)
{
    Family opened = TestWard::family(session.document(), familyId);
    return session.commit(UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, personId, phone)));
}

/// Commits coordinates typed for the address, against the session's current entry.
CommitResult commitTyped(WardSession& session, const Address& address, const Coordinates& typed)
{
    return session.commit(SetAddressCoordinatesCommand::fromTyped(session.document(), address.full(), typed));
}

/// A ward with that unit number and no details.
Ward wardWithoutDetails(const QString& unitNumber)
{
    Ward ward;
    ward.setUnitNumber(unitNumber);
    return ward;
}

}  // namespace

AutomaticUpdatesTest::AutomaticUpdatesTest(QObject* parent)
    : QObject(parent)
{
}

void AutomaticUpdatesTest::geocodeResultAppliesOutsideTheJournal()
{
    TestSession session;
    session.open(TestWard::document());
    session.runAutomaticUpdates();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));

    session.geocoder().resolve(smithAddress().full(), ElmPosition);
    session.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(session.journal().isEmpty());
    QVERIFY(!session.canUndo());

    ReplayResult result = session.replayOnto(TestWard::document());

    QVERIFY(result.status == ReplayStatus::FastForwarded);
    QVERIFY(!positionOf(session, TestWard::smithId()).has_value());
    QVERIFY(session.journal().isEmpty());
}

void AutomaticUpdatesTest::geocodeBetweenAnOfflinePhoneEditAndItsReplayKeepsBoth()
{
    TestSession mine;
    mine.open(TestWard::document());
    QVERIFY(commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    mine.runAutomaticUpdates();
    mine.geocoder().resolve(smithAddress().full(), ElmPosition);
    mine.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(positionOf(mine, TestWard::smithId()) == ElmPosition);

    // The version lacks the result, which was never journalled; the batch run after the
    // replay computes it again.
    ReplayResult result = mine.replayOnto(TestWard::document());
    QVERIFY(result.status == ReplayStatus::Replayed);
    QVERIFY(!positionOf(mine, TestWard::smithId()).has_value());
    mine.runAutomaticUpdates();
    mine.geocoder().resolve(smithAddress().full(), ElmPosition);
    mine.geocoder().resolve(jonesAddress().full(), OakPosition);

    QCOMPARE(QString(TestWard::person(mine.document(), TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(positionOf(mine, TestWard::smithId()) == ElmPosition);
    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
}

void AutomaticUpdatesTest::addressEditPlacesTheFamilyAtTheNewAddressEntry()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");

    // The old address's geocode lands before the edit.
    TestSession before;
    before.open(TestWard::document());
    before.runAutomaticUpdates();
    before.geocoder().resolve(smithAddress().full(), ElmPosition);
    before.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(positionOf(before, TestWard::smithId()) == ElmPosition);

    QVERIFY(commitAddress(before, TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QVERIFY(!positionOf(before, TestWard::smithId()).has_value());
    QCOMPARE(before.geocoder().requests().last(), newAddress.full());

    before.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(before, TestWard::smithId()) == NewElmPosition);
    QVERIFY(!before.document().addressCoordinates().contains(smithAddress().full()));
    QCOMPARE(before.document().addressCoordinates().size(), qsizetype(2));

    // The old address's geocode lands after the edit.
    TestSession after;
    after.open(TestWard::document());
    after.runAutomaticUpdates();
    QVERIFY(commitAddress(after, TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(after.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), newAddress.full()}));

    after.geocoder().resolve(smithAddress().full(), ElmPosition);
    after.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(!positionOf(after, TestWard::smithId()).has_value());

    after.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(after, TestWard::smithId()) == NewElmPosition);
    QVERIFY(!after.document().addressCoordinates().contains(smithAddress().full()));
    QCOMPARE(after.document().addressCoordinates().size(), qsizetype(2));
}

void AutomaticUpdatesTest::typedCoordinatesSetTheEntryAndAGeocodeNeverReplacesThem()
{
    TestSession session;
    session.open(TestWard::document());
    session.runAutomaticUpdates();

    QVERIFY(commitTyped(session, smithAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(session, TestWard::smithId()) == TypedPosition);
    QCOMPARE(session.undoHeading(), QString("Map position for 12 Elm St, Provo, UT"));

    session.geocoder().resolve(smithAddress().full(), ElmPosition);
    session.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(session, TestWard::smithId()) == TypedPosition);
    QVERIFY(positionOf(session, TestWard::jonesId()) == OakPosition);
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
}

void AutomaticUpdatesTest::undoOfTypedCoordinatesRestoresThePreviousEntry()
{
    TestSession session;
    session.open(mappedWard());
    QVERIFY(commitTyped(session, smithAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(session, TestWard::smithId()) == TypedPosition);

    CommitResult undone = session.undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QCOMPARE(undone.description.heading(), QString("Undo: Map position for 12 Elm St, Provo, UT"));
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(session.canRedo());

    // An entry typed where there was none is removed by its undo.
    TestSession unmapped;
    unmapped.open(TestWard::document());
    QVERIFY(commitTyped(unmapped, jonesAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(unmapped, TestWard::jonesId()) == TypedPosition);

    QVERIFY(unmapped.undo().status == CommitStatus::Applied);
    QVERIFY(!positionOf(unmapped, TestWard::jonesId()).has_value());
}

void AutomaticUpdatesTest::typedCoordinatesReplayedAfterAGeocodeOfThatAddressApply()
{
    TestSession mine;
    mine.open(TestWard::document());
    QVERIFY(commitTyped(mine, smithAddress(), TypedPosition).status == CommitStatus::Applied);

    ReplayResult result = mine.replayOnto(mappedWard());

    QVERIFY(result.status == ReplayStatus::Replayed);
    QVERIFY(positionOf(mine, TestWard::smithId()) == TypedPosition);
    QVERIFY(positionOf(mine, TestWard::jonesId()) == OakPosition);
    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
}

void AutomaticUpdatesTest::batchDropsEntriesNoFamilyUsesAndKeepsEveryEntryInUse()
{
    Address gone = TestWard::address("99 Gone St", "Provo, UT");
    TestSession session;
    session.open(TestWard::withCoordinates(mappedWard(), gone, NewElmPosition));
    QSignalSpy appliedSpy(&session, &WardSession::automaticUpdateApplied);

    session.runAutomaticUpdates();

    QVERIFY(session.geocoder().requests().isEmpty());
    QCOMPARE(appliedSpy.count(), 1);
    QCOMPARE(session.document().addressCoordinates().size(), qsizetype(2));
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(session, TestWard::jonesId()) == OakPosition);
    QVERIFY(!session.document().addressCoordinates().contains(gone.full()));
}

void AutomaticUpdatesTest::batchOfSeveralAddressesAppliesOnce()
{
    Family lee = TestWard::lee(2010);
    Document document = TestWard::document();
    document.addFamily(lee);
    document.onDocumentChanged(DocumentChange::full());
    TestSession session;
    session.open(document);
    QSignalSpy appliedSpy(&session, &WardSession::automaticUpdateApplied);

    session.runAutomaticUpdates();
    QCOMPARE(session.geocoder().requests().size(), qsizetype(3));
    session.geocoder().resolve(smithAddress().full(), ElmPosition);
    session.geocoder().resolve(jonesAddress().full(), OakPosition);
    QCOMPARE(appliedSpy.count(), 0);
    session.geocoder().resolve(lee.address().full(), NewElmPosition);

    QCOMPARE(appliedSpy.count(), 1);
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(session, TestWard::jonesId()) == OakPosition);
    QVERIFY(positionOf(session, lee.id()) == NewElmPosition);
}

void AutomaticUpdatesTest::failedAddressIsNotRequestedAgainUntilItsTextChanges()
{
    TestSession session;
    session.open(TestWard::document());
    session.runAutomaticUpdates();
    session.geocoder().fail(smithAddress().full());
    session.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(!positionOf(session, TestWard::smithId()).has_value());

    // Not after a replay: the run that follows requests the Jones address the replay dropped,
    // and not the Smith address.
    session.replayOnto(TestWard::document());
    session.runAutomaticUpdates();
    QCOMPARE(session.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), jonesAddress().full()}));
    session.geocoder().resolve(jonesAddress().full(), OakPosition);

    // Not after another family is given a new address.
    Address newOak = TestWard::address("6 Oak Ave", "Provo, UT");
    QVERIFY(commitAddress(session, TestWard::jonesId(), newOak).status == CommitStatus::Applied);
    QCOMPARE(session.geocoder().requests().last(), newOak.full());
    QCOMPARE(session.geocoder().requests().count(smithAddress().full()), qsizetype(1));
    session.geocoder().resolve(newOak.full(), OakPosition);

    // Requested again once its text changes.
    Address corrected = TestWard::address("12 Elm Street", "Provo, UT");
    QVERIFY(commitAddress(session, TestWard::smithId(), corrected).status == CommitStatus::Applied);
    QCOMPARE(session.geocoder().requests().last(), corrected.full());
}

void AutomaticUpdatesTest::twoFamiliesAtOneAddressShareOneRequestAndOneEntry()
{
    Family jones = TestWard::jones();
    jones.setAddress(smithAddress());
    TestSession session;
    session.open(TestWard::withFamily(TestWard::document(), jones));
    QSignalSpy finishedSpy(&session, &WardSession::geocodingFinished);

    session.runAutomaticUpdates();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full()}));
    session.geocoder().resolve(smithAddress().full(), ElmPosition);

    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(session, TestWard::jonesId()) == ElmPosition);
    QCOMPARE(session.document().addressCoordinates().size(), qsizetype(1));
}

void AutomaticUpdatesTest::commandThatGivesNoAddressStartsNoGeocoding()
{
    TestSession mine;
    mine.open(TestWard::document());
    QVERIFY(commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(mine.geocoder().requests().isEmpty());

    TestSession theirs;
    theirs.open(TestWard::document());
    QVERIFY(commitAddress(theirs, TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT")).status
            == CommitStatus::Applied);
    QVERIFY(!theirs.geocoder().requests().isEmpty());

    QVERIFY(mine.replayOnto(theirs.document()).status == ReplayStatus::Replayed);
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::batchThatChangesNothingEmitsNothing()
{
    TestSession mapped;
    mapped.open(mappedWard());
    QSignalSpy mappedApplied(&mapped, &WardSession::automaticUpdateApplied);
    QSignalSpy mappedChanged(&mapped, &WardSession::documentChanged);
    mapped.runAutomaticUpdates();
    QVERIFY(mapped.geocoder().requests().isEmpty());
    QCOMPARE(mappedApplied.count(), 0);
    QCOMPARE(mappedChanged.count(), 0);

    // A batch whose every request fails.
    TestSession failing;
    failing.open(TestWard::document());
    QSignalSpy failingApplied(&failing, &WardSession::automaticUpdateApplied);
    QSignalSpy failingChanged(&failing, &WardSession::documentChanged);
    QSignalSpy failingFinished(&failing, &WardSession::geocodingFinished);
    failing.runAutomaticUpdates();
    QCOMPARE(failing.geocoder().requests().size(), qsizetype(2));
    failing.geocoder().fail(smithAddress().full());
    failing.geocoder().fail(jonesAddress().full());
    QCOMPARE(failingFinished.count(), 1);
    QCOMPARE(failingApplied.count(), 0);
    QCOMPARE(failingChanged.count(), 0);
}

void AutomaticUpdatesTest::lookupThatChangesNothingEmitsNothing()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.open(document);
    QSignalSpy appliedSpy(&session, &WardSession::automaticUpdateApplied);
    QSignalSpy changedSpy(&session, &WardSession::documentChanged);

    session.runAutomaticUpdates();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    session.unitLookup().findWard("111111", wardWithoutDetails("111111"));

    QCOMPARE(appliedSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(session.document() == document);

    // A lookup that found no details puts the unit on the failed list.
    session.runAutomaticUpdates();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
}

void AutomaticUpdatesTest::failedWardLookupContinuesToTheNextWard()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.open(document);

    session.runAutomaticUpdates();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    session.unitLookup().fail("111111");

    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));
}
