#include "ImportCommandsTest.h"
#include "EmergencyCommands.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "MinisteringImportService.h"
#include "TestSession.h"
#include "TestWard.h"
#include "WardDirectoryImportService.h"

#include <QTest>

namespace
{

/// person as a PDF prints them: a new id, as the parser gives one, and no notes.
Person printedPerson(const Person& person)
{
    return Person::createWithId(PersonId::fromString("pdf-" + person.id().toString()), person.name(), person.gender(),
                                person.birthday(), person.phone(), person.altPhone(), person.email(),
                                person.callings(), person.isParent(), person.wardUnitNumber(),
                                person.stakeUnitNumber());
}

/// family as a PDF prints it: new ids for it and its members, and no notes.
Family printed(const Family& family)
{
    QList<Person> members;
    for (const Person& member : family.members())
    {
        members.append(printedPerson(member));
    }
    return Family::createWithId(FamilyId::fromString("pdf-" + family.id().toString()), family.address(), members);
}

/// The Tanglewood ward directory, unit 123456, listing families, as the parser gives it.
WardDirectoryPdfParser::ParseResult directoryListing(const QList<Family>& families)
{
    WardDirectoryPdfParser::ParseResult parsed;
    parsed.success = true;
    parsed.wardName = "Tanglewood";
    parsed.wardUnitNumber = "123456";
    for (const Family& family : families)
    {
        Family listed = printed(family);
        parsed.families.insert(listed.id(), listed);
    }
    return parsed;
}

/// parsed, from a PDF dated pdfDate, merged onto the families document holds.
WardDirectoryImportResult mergeDirectory(const WardDirectoryPdfParser::ParseResult& parsed,
                                         const Document& document,
                                         const QDate& pdfDate)
{
    WardDirectoryImportService service(nullptr);
    return service.merge(parsed, document.families(), pdfDate, document.ministeringPdfDate());
}

QDate octoberFifteenth()
{
    return QDate(2026, 10, 15);
}

}  // namespace

ImportCommandsTest::ImportCommandsTest(QObject* parent)
    : QObject(parent)
{
}

void ImportCommandsTest::importReplayedAfterSomeoneAddedAFamilyKeepsThatFamily()
{
    TestSession mine;
    mine.open(TestWard::document());
    WardDirectoryImportResult result = mergeDirectory(directoryListing({TestWard::smith()}), mine.document(),
                                                      octoberFifteenth());
    QCOMPARE(result.removedFamilyIds, QSet<FamilyId>({TestWard::jonesId()}));
    QVERIFY(mine.commit(ImportWardDirectoryCommand::fromImport(mine.document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    Family lee = TestWard::lee(2010);
    Document theirs = TestWard::withFamily(TestWard::document(), lee);

    ReplayResult replay = mine.replayOnto(theirs);

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(mine.document().findFamilyById(lee.id()) == lee);
    QVERIFY(mine.document().findFamilyById(TestWard::smithId()) == TestWard::smith());
    QVERIFY(!mine.document().findFamilyById(TestWard::jonesId()).has_value());

    QVERIFY(mine.undo().status == CommitStatus::Applied);

    QVERIFY(mine.document().findFamilyById(TestWard::jonesId()) == TestWard::jones());
    QVERIFY(mine.document().findFamilyById(lee.id()) == lee);
}

void ImportCommandsTest::importReplayedAfterAnEditToAFamilyItChangesConflicts()
{
    TestSession mine;
    mine.open(TestWard::document());
    Family listedSmith = TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-2222");
    WardDirectoryImportResult result = mergeDirectory(directoryListing({listedSmith, TestWard::jones()}),
                                                      mine.document(), octoberFifteenth());
    QVERIFY(mine.commit(ImportWardDirectoryCommand::fromImport(mine.document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    QCOMPARE(QString(TestWard::person(mine.document(), TestWard::bobId()).phone()), QString("555-2222"));
    Document theirs = TestWard::withFamily(TestWard::document(),
                                           TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-9999"));

    ReplayResult replay = mine.replayOnto(theirs);

    QVERIFY(replay.status == ReplayStatus::Abandoned);
    QVERIFY(replay.toRedo.contains("Import the ward directory from Tanglewood.pdf. (stopped here)"));
    QCOMPARE(QString(TestWard::person(mine.document(), TestWard::bobId()).phone()), QString("555-9999"));
    QVERIFY(mine.document() == theirs);
}

void ImportCommandsTest::reimportingTheSameDirectoryKeepsNotesAndCoordinatesAndHoldsNoChange()
{
    Person bob = TestWard::bob();
    bob.setNote("Drives the ward van");
    bob.setSpecialNeedNote("Uses a wheelchair; the ramp is at the back door.");
    Family smith = TestWard::withMember(TestWard::smith(), bob);
    smith.setNote("Gate code is 1234");
    Coordinates own(40.2338, -111.6585);
    Document document = TestWard::withCoordinates(TestWard::withFamily(TestWard::document(), smith), smith.address(), own);
    document.setWardDirectoryPdfDate(octoberFifteenth());
    Ward ward;
    ward.setUnitNumber("123456");
    ward.setName("Tanglewood");
    document.metadata().updateWard(ward);
    WardDirectoryPdfParser::ParseResult parsed = directoryListing({TestWard::smith(), TestWard::jones()});
    parsed.addressCoordinates.insert(smith.address().full(), Coordinates(40.2300, -111.6500));

    WardDirectoryImportResult same = mergeDirectory(parsed, document, octoberFifteenth());

    QVERIFY(same.families == document.families());
    QVERIFY(same.removedFamilyIds.isEmpty());
    QVERIFY(ImportWardDirectoryCommand::fromImport(document, same, "Tanglewood.pdf") == nullptr);

    // A newer copy of the directory, which also prints the Jones family's coordinates.
    Coordinates listedJones(40.2400, -111.6600);
    parsed.addressCoordinates.insert(TestWard::jones().address().full(), listedJones);
    WardDirectoryImportResult newer = mergeDirectory(parsed, document, QDate(2026, 10, 20));
    UndoableCommandPtr command = ImportWardDirectoryCommand::fromImport(document, newer, "Tanglewood.pdf");
    QVERIFY(command != nullptr);
    Document applied = document;

    QVERIFY(command->execute(applied) == CommandOutcome::Applied);

    QVERIFY(applied.families() == document.families());
    QVERIFY(applied.coordinatesFor(smith.address()) == own);
    QVERIFY(applied.coordinatesFor(TestWard::jones().address()) == listedJones);
    QVERIFY(applied.wardDirectoryPdfDate() == QDate(2026, 10, 20));
}

void ImportCommandsTest::olderMinisteringPdfAddsNoUnknownFamilyOrMemberAndNoGroupNamesAMissingId()
{
    Document document = TestWard::document();
    document.setWardDirectoryPdfDate(octoberFifteenth());

    // Bob and Eve, whom the directory does not list, minister to the Jones family and to the
    // Lee family, which the directory does not list either; Eve oversees the district.
    Person eve = Person::createWithId(PersonId::fromString("pdf-eve"), Name("Smith", "Eve"), Gender::Female,
                                      Birthday(), Phone(), Phone(), QString(), QStringList(), false, QString(),
                                      QString());
    Family ministers = Family::createWithId(FamilyId::fromString("pdf-smith"), Address(),
                                            {printedPerson(TestWard::bob()), eve});
    Family jones = printed(TestWard::jones());
    Family lee = printed(TestWard::lee(2010));
    MinisteringGroup group = MinisteringGroup::createEQ({printedPerson(TestWard::bob()).id(), eve.id()},
                                                        {jones.id(), lee.id()});
    MinisteringDistrict district = MinisteringDistrict::create("District 1", eve.id(), {group.id()});
    MinisteringPdfParser::ParseResult parsed;
    parsed.success = true;
    parsed.documentDate = QDate(2026, 9, 1);
    parsed.ministerFamilies.insert(ministers.id(), ministers);
    parsed.ministeredFamilies.insert(jones.id(), jones);
    parsed.ministeredFamilies.insert(lee.id(), lee);
    parsed.groups.insert(group.id(), group);
    parsed.districts.insert(district.id(), district);

    MinisteringImportService service(nullptr);
    MinisteringImportResult result = service.merge(parsed, document.families(), document.wardDirectoryPdfDate());

    QVERIFY(result.families == document.families());
    QCOMPARE(result.groups.value(group.id()).ministerIds(), QSet<PersonId>({TestWard::bobId()}));
    QCOMPARE(result.groups.value(group.id()).familyIds(), QSet<FamilyId>({TestWard::jonesId()}));
    QVERIFY(!result.districts.value(district.id()).presidencyMemberId().has_value());
    QCOMPARE(result.districts.value(district.id()).groupIds(), QSet<MinisteringGroupId>({group.id()}));

    TestSession session;
    session.open(document);
    QVERIFY(session.commit(ImportMinisteringCommand::fromImport(session.document(), result, "Tanglewood EQ.pdf")).status
            == CommitStatus::Applied);
    QVERIFY(session.document().groups(MinisteringOrg::EldersQuorum) == result.groups);
    QVERIFY(session.document().families() == document.families());
    QVERIFY(session.document().ministeringPdfDate() == QDate(2026, 9, 1));

    // An import whose group names people and families the ward lacks conflicts and changes
    // nothing.
    MinisteringImportResult naming = result;
    naming.groups.insert(group.id(), group);
    Document before = session.document();
    QVERIFY(session.commit(ImportMinisteringCommand::fromImport(session.document(), naming, "Tanglewood EQ.pdf")).status
            == CommitStatus::Conflicted);
    QVERIFY(session.document() == before);
}

void ImportCommandsTest::attemptOnAFamilyLandsAfterAnImportRemovedIt()
{
    Document document = TestWard::withResponse(TestWard::document(), TestWard::storm());
    TestSession theirs;
    theirs.open(document);
    WardDirectoryImportResult result = mergeDirectory(directoryListing({TestWard::smith()}), document,
                                                      octoberFifteenth());
    QVERIFY(theirs.commit(ImportWardDirectoryCommand::fromImport(theirs.document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    QVERIFY(!theirs.document().findFamilyById(TestWard::jonesId()).has_value());

    TestSession mine;
    mine.open(document);
    ContactAttempt attempt = TestWard::attempt("attempt-jones", TestWard::jonesId(), TestWard::octoberFirst(10, 0),
                                               ContactMethod::Phone, TestWard::bobId(), ContactOutcome::OK,
                                               "All well");
    QVERIFY(mine.commit(AddContactAttemptCommand::fromNew(TestWard::stormId(), attempt, mine.document())).status
            == CommitStatus::Applied);

    ReplayResult replay = mine.replayOnto(theirs.document());

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(!mine.document().findFamilyById(TestWard::jonesId()).has_value());
    EmergencyResponse response = mine.document().findEmergencyResponseById(TestWard::stormId()).value();
    const ContactAttempt* landed = response.findContactAttempt(attempt.id());
    QVERIFY(landed != nullptr);
    QVERIFY(landed->familyId() == TestWard::jonesId());
}

void ImportCommandsTest::importDescriptionIsOneLineNamingTheFile()
{
    WardDirectoryImportResult directory = mergeDirectory(directoryListing({TestWard::smith()}), TestWard::document(),
                                                         octoberFifteenth());
    UndoableCommandPtr directoryImport = ImportWardDirectoryCommand::fromImport(TestWard::document(), directory,
                                                                                "Tanglewood.pdf");

    QCOMPARE(directoryImport->description(),
             CommandDescription("Import the ward directory from Tanglewood.pdf.", QStringList()));
    QCOMPARE(directoryImport->inverse()->description(),
             CommandDescription("Undo: Import the ward directory from Tanglewood.pdf.", QStringList()));

    MinisteringImportResult ministering;
    ministering.success = true;
    ministering.families = TestWard::document().families();
    MinisteringGroup group = MinisteringGroup::createEQ({TestWard::bobId()}, {TestWard::jonesId()});
    ministering.groups.insert(group.id(), group);
    UndoableCommandPtr ministeringImport = ImportMinisteringCommand::fromImport(TestWard::document(), ministering,
                                                                                "Tanglewood EQ.pdf");

    QCOMPARE(ministeringImport->description(),
             CommandDescription("Import the ministering assignments from Tanglewood EQ.pdf.", QStringList()));
}
