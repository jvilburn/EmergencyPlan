#include "AutomaticUpdatesTest.h"
#include "AddressCoordinatesCommands.h"
#include "FamilyCommands.h"
#include "TestSession.h"
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
    return session.commit(SetAddressCoordinatesCommand::fromTyped(session.document(), address.full(), typed));
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
    TestSession session;
    session.createWard(TestWard::document());
    session.automaticUpdates().run();
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
    mine.createWard(TestWard::document());
    QVERIFY(commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(!positionOf(mine, TestWard::smithId()).has_value());

    // Another device's geocode of the Smith address landed in the version.
    ReplayResult result = mine.replayOnto(TestWard::withCoordinates(TestWard::document(), smithAddress(), ElmPosition));

    QVERIFY(result.status == ReplayStatus::Replayed);
    QCOMPARE(QString(TestWard::person(mine.document(), TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(positionOf(mine, TestWard::smithId()) == ElmPosition);
    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::addressEditPlacesTheFamilyAtTheNewAddressEntry()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");

    // The old address's geocode lands before the edit.
    TestSession before;
    before.createWard(TestWard::document());
    before.automaticUpdates().run();
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
    after.createWard(TestWard::document());
    after.automaticUpdates().run();
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
    session.createWard(TestWard::document());
    session.automaticUpdates().run();

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
    session.createWard(mappedWard());
    QVERIFY(commitTyped(session, smithAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(session, TestWard::smithId()) == TypedPosition);

    CommitResult undone = session.undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QCOMPARE(undone.description.heading(), QString("Undo: Map position for 12 Elm St, Provo, UT"));
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(session.canRedo());

    // An entry typed where there was none is removed by its undo.
    TestSession unmapped;
    unmapped.createWard(TestWard::document());
    QVERIFY(commitTyped(unmapped, jonesAddress(), TypedPosition).status == CommitStatus::Applied);
    QVERIFY(positionOf(unmapped, TestWard::jonesId()) == TypedPosition);

    QVERIFY(unmapped.undo().status == CommitStatus::Applied);
    QVERIFY(!positionOf(unmapped, TestWard::jonesId()).has_value());
}

void AutomaticUpdatesTest::typedCoordinatesReplayedAfterAGeocodeOfThatAddressApply()
{
    TestSession mine;
    mine.createWard(TestWard::document());
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
    session.createWard(TestWard::withCoordinates(mappedWard(), gone, NewElmPosition));
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);

    session.automaticUpdates().run();

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
    document.setFamily(lee);
    document.onDocumentChanged(DocumentChange::full());
    TestSession session;
    session.createWard(document);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);

    session.automaticUpdates().run();
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
    session.createWard(TestWard::document());
    session.automaticUpdates().run();
    session.geocoder().fail(smithAddress().full());
    session.geocoder().resolve(jonesAddress().full(), OakPosition);
    QVERIFY(!positionOf(session, TestWard::smithId()).has_value());

    // Not after a replay: the run that follows requests the Jones address the replay dropped,
    // and not the Smith address.
    session.replayOnto(TestWard::document());
    session.automaticUpdates().run();
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
    session.createWard(TestWard::withFamily(TestWard::document(), jones));
    QSignalSpy finishedSpy(&session.automaticUpdates(), &AutomaticUpdates::geocodingFinished);

    session.automaticUpdates().run();
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
    mine.createWard(TestWard::document());
    QVERIFY(commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(mine.geocoder().requests().isEmpty());

    TestSession theirs;
    theirs.createWard(TestWard::document());
    QVERIFY(commitAddress(theirs, TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT")).status
            == CommitStatus::Applied);
    QVERIFY(!theirs.geocoder().requests().isEmpty());

    QVERIFY(mine.replayOnto(theirs.document()).status == ReplayStatus::Replayed);
    QVERIFY(mine.geocoder().requests().isEmpty());
}

void AutomaticUpdatesTest::batchThatChangesNothingEmitsNothing()
{
    TestSession mapped;
    mapped.createWard(mappedWard());
    QSignalSpy mappedApplied(&mapped.automaticUpdates(), &AutomaticUpdates::applied);
    QSignalSpy mappedChanged(&mapped, &WardSession::documentChanged);
    mapped.automaticUpdates().run();
    QVERIFY(mapped.geocoder().requests().isEmpty());
    QCOMPARE(mappedApplied.count(), 0);
    QCOMPARE(mappedChanged.count(), 0);

    // A batch whose every request fails.
    TestSession failing;
    failing.createWard(TestWard::document());
    QSignalSpy failingApplied(&failing.automaticUpdates(), &AutomaticUpdates::applied);
    QSignalSpy failingChanged(&failing, &WardSession::documentChanged);
    QSignalSpy failingFinished(&failing.automaticUpdates(), &AutomaticUpdates::geocodingFinished);
    failing.automaticUpdates().run();
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
    session.createWard(document);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);
    QSignalSpy changedSpy(&session, &WardSession::documentChanged);

    session.automaticUpdates().run();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    session.unitLookup().findWard("111111", wardWithoutDetails("111111"));

    QCOMPARE(appliedSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(session.document() == document);

    // A lookup that found no details puts the unit on the failed list.
    session.automaticUpdates().run();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
}

void AutomaticUpdatesTest::failedWardLookupContinuesToTheNextWard()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.createWard(document);

    session.automaticUpdates().run();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    session.unitLookup().fail("111111");

    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));
}

void AutomaticUpdatesTest::wardFoundNamingAMissingStakeLooksUpTheStakeBeforeTheNextWard()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.createWard(document);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);

    session.automaticUpdates().run();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));

    session.unitLookup().findWard("111111", wardWithChapel("111111", "900000"));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "stake 900000"}));
    QVERIFY(!session.document().stakes().contains("900000"));

    session.unitLookup().findStake("900000", namedStake("900000"));

    QVERIFY(session.document().metadata().findStakeByUnit("900000") == namedStake("900000"));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "stake 900000", "ward 222222"}));
    QCOMPARE(appliedSpy.count(), 2);
}

void AutomaticUpdatesTest::undoOfAnAddressEditRequestsTheRestoredAddress()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestSession session;
    session.createWard(mappedWard());

    QVERIFY(commitAddress(session, TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(session.geocoder().requests(), QStringList({newAddress.full()}));
    session.geocoder().resolve(newAddress.full(), NewElmPosition);
    QVERIFY(positionOf(session, TestWard::smithId()) == NewElmPosition);
    QVERIFY(!session.document().addressCoordinates().contains(smithAddress().full()));

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QCOMPARE(TestWard::family(session.document(), TestWard::smithId()).address().full(), smithAddress().full());
    QCOMPARE(session.geocoder().requests(), QStringList({newAddress.full(), smithAddress().full()}));
}

void AutomaticUpdatesTest::lateAnswersChangeNothing()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    Address gone = TestWard::address("99 Gone St", "Provo, UT");
    Document document = TestWard::document();
    document.metadata().updateWard(wardWithoutDetails("222222"));
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.createWard(document);
    session.automaticUpdates().run();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    QSignalSpy changedSpy(&session, &WardSession::documentChanged);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);
    QSignalSpy progressSpy(&session.automaticUpdates(), &AutomaticUpdates::geocodingProgress);

    // Answers for addresses the batch is not waiting on, and for units not being looked up.
    session.geocoder().resolve(gone.full(), NewElmPosition);
    session.geocoder().fail(newAddress.full());
    session.unitLookup().findWard("222222", wardWithChapel("222222", "900000"));
    session.unitLookup().findStake("900000", namedStake("900000"));
    session.unitLookup().fail("222222");

    QVERIFY(session.document() == document);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(appliedSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 0);
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));

    // An answer the batch is waiting on maps the family; the same address answered again
    // later changes nothing.
    session.geocoder().resolve(smithAddress().full(), ElmPosition);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    session.geocoder().resolve(smithAddress().full(), NewElmPosition);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);

    // The answer for the unit being looked up applies; the late failure of the other ward
    // left it off the failed list, so it is looked up next.
    session.unitLookup().findWard("111111", wardWithChapel("111111", QString()));
    QCOMPARE(appliedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 2);
    QVERIFY(session.document().metadata().findWardByUnit("111111")->chapelAddress().has_value());
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));

    // The late failure of the new address left it off the failed list too.
    QVERIFY(commitAddress(session, TestWard::jonesId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(session.geocoder().requests(),
             QStringList({smithAddress().full(), jonesAddress().full(), newAddress.full()}));
}

void AutomaticUpdatesTest::wardFoundWithoutAChapelIsNotLookedUpAgain()
{
    Document document = mappedWard();
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.createWard(document);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);
    session.automaticUpdates().run();
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));

    Ward found = wardWithoutDetails("111111");
    found.setMeetingTime(QString("Sunday 9:00 AM"));
    session.unitLookup().findWard("111111", found);
    QCOMPARE(appliedSpy.count(), 1);
    QVERIFY(session.document().metadata().findWardByUnit("111111")->meetingTime() == QString("Sunday 9:00 AM"));

    // A version taken as it stands brings another ward lacking its details; the next run
    // looks up that ward and not the one found without a chapel.
    Document version = session.document();
    version.metadata().updateWard(wardWithoutDetails("222222"));
    QVERIFY(session.replayOnto(version).status == ReplayStatus::FastForwarded);
    session.automaticUpdates().run();

    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "ward 222222"}));
}

void AutomaticUpdatesTest::failedListStartsEmptyInANewWard()
{
    Document document = TestWard::withCoordinates(TestWard::document(), jonesAddress(), OakPosition);
    document.metadata().updateWard(wardWithoutDetails("111111"));
    TestSession session;
    session.createWard(document);
    session.automaticUpdates().run();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full()}));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));
    session.geocoder().fail(smithAddress().full());
    session.unitLookup().fail("111111");

    // On the failed list, neither is requested again.
    session.automaticUpdates().run();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full()}));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111"}));

    session.createWard(document);
    session.automaticUpdates().run();

    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full(), smithAddress().full()}));
    QCOMPARE(session.unitLookup().requests(), QStringList({"ward 111111", "ward 111111"}));
}

void AutomaticUpdatesTest::resultsMapFamiliesAsTheyArrive()
{
    TestSession session;
    session.createWard(TestWard::document());
    QSignalSpy changedSpy(&session.automaticUpdates(), &AutomaticUpdates::documentChanged);
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);
    session.automaticUpdates().run();
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));

    session.geocoder().resolve(smithAddress().full(), ElmPosition);

    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(!positionOf(session, TestWard::jonesId()).has_value());
    QCOMPARE(changedSpy.count(), 1);
    DocumentChange first = qvariant_cast<DocumentChange>(changedSpy.at(0).at(0));
    QVERIFY(first.action == ChangeAction::Updated);
    QVERIFY(first.familyId == TestWard::smithId());
    QCOMPARE(appliedSpy.count(), 0);

    session.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(session, TestWard::jonesId()) == OakPosition);
    QCOMPARE(changedSpy.count(), 2);
    DocumentChange second = qvariant_cast<DocumentChange>(changedSpy.at(1).at(0));
    QVERIFY(second.action == ChangeAction::Updated);
    QVERIFY(second.familyId == TestWard::jonesId());
    QCOMPARE(appliedSpy.count(), 1);
}

void AutomaticUpdatesTest::documentReplacedMidBatchLosesNoResults()
{
    TestSession session;
    session.createWard(TestWard::document());
    QSignalSpy appliedSpy(&session.automaticUpdates(), &AutomaticUpdates::applied);
    session.automaticUpdates().run();
    session.geocoder().resolve(smithAddress().full(), ElmPosition);
    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);

    QVERIFY(session.replayOnto(TestWard::document()).status == ReplayStatus::FastForwarded);
    QVERIFY(!positionOf(session, TestWard::smithId()).has_value());

    session.geocoder().resolve(jonesAddress().full(), OakPosition);

    QVERIFY(positionOf(session, TestWard::smithId()) == ElmPosition);
    QVERIFY(positionOf(session, TestWard::jonesId()) == OakPosition);
    QCOMPARE(session.document().addressCoordinates().size(), qsizetype(2));
    QCOMPARE(session.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
    QCOMPARE(appliedSpy.count(), 1);
}

void AutomaticUpdatesTest::commandThatGivesAWardNeedingALookupStartsOne()
{
    // A ward lacking its details, from a command that gives no address.
    TestSession lacking;
    lacking.createWard(mappedWard());
    QVERIFY(lacking.commit(std::make_shared<GiveWardCommand>(wardWithoutDetails("111111"))).status
            == CommitStatus::Applied);
    QCOMPARE(lacking.unitLookup().requests(), QStringList({"ward 111111"}));
    QVERIFY(lacking.geocoder().requests().isEmpty());

    // A ward with its details, naming a stake the document lacks.
    TestSession stakeMissing;
    stakeMissing.createWard(mappedWard());
    QVERIFY(stakeMissing.commit(std::make_shared<GiveWardCommand>(wardWithChapel("111111", "900000"))).status
            == CommitStatus::Applied);
    QCOMPARE(stakeMissing.unitLookup().requests(), QStringList({"stake 900000"}));

    // A ward with its details, naming a stake the document has, starts no run, though a run
    // would look up the other ward and geocode both addresses.
    Document document = TestWard::document();
    document.metadata().updateStake(namedStake("900000"));
    document.metadata().updateWard(wardWithoutDetails("222222"));
    TestSession complete;
    complete.createWard(document);
    QVERIFY(complete.commit(std::make_shared<GiveWardCommand>(wardWithChapel("111111", "900000"))).status
            == CommitStatus::Applied);
    QVERIFY(complete.document().metadata().findWardByUnit("111111").has_value());
    QVERIFY(complete.unitLookup().requests().isEmpty());
    QVERIFY(complete.geocoder().requests().isEmpty());

    complete.automaticUpdates().run();
    QCOMPARE(complete.unitLookup().requests(), QStringList({"ward 222222"}));
    QCOMPARE(complete.geocoder().requests(), QStringList({smithAddress().full(), jonesAddress().full()}));
}

void AutomaticUpdatesTest::addressEditFromAnotherDeviceLeavesTheFamilyUnmappedAndRequestsNothing()
{
    Address newAddress = TestWard::address("14 Elm St", "Provo, UT");
    TestSession theirs;
    theirs.createWard(mappedWard());
    QVERIFY(commitAddress(theirs, TestWard::smithId(), newAddress).status == CommitStatus::Applied);
    QCOMPARE(theirs.geocoder().requests(), QStringList({newAddress.full()}));

    TestSession mine;
    mine.createWard(mappedWard());
    QVERIFY(mine.replayOnto(theirs.document()).status == ReplayStatus::FastForwarded);

    QCOMPARE(TestWard::family(mine.document(), TestWard::smithId()).address().full(), newAddress.full());
    QVERIFY(!positionOf(mine, TestWard::smithId()).has_value());
    QVERIFY(positionOf(mine, TestWard::jonesId()) == OakPosition);
    QVERIFY(mine.document().coordinatesFor(smithAddress()) == ElmPosition);
    for (const Family& family : mine.document().families())
    {
        QVERIFY(family.address().full() != smithAddress().full());
    }
    QVERIFY(mine.geocoder().requests().isEmpty());
}
