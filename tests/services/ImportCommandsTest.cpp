#include "ImportCommandsTest.h"
#include "EmergencyCommands.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "MinisteringImportService.h"
#include "PdfText.h"
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
    return WardDirectoryImportService::merge(parsed, document.families(), pdfDate, document.ministeringPdfDate());
}

QDate octoberFifteenth()
{
    return QDate(2026, 10, 15);
}

/// An elders quorum import onto the families document holds, dated 1 September 2026: one
/// group, Bob ministering to the Jones family.
MinisteringImportResult bobMinistersToTheJoneses(const Document& document)
{
    MinisteringImportResult result;
    result.success = true;
    result.families = document.families();
    MinisteringGroup group = MinisteringGroup::createEQ({TestWard::bobId()}, {TestWard::jonesId()});
    result.groups.insert(group.id(), group);
    result.pdfDate = QDate(2026, 9, 1);
    return result;
}

/// The ids of family's members, in its member order.
QStringList memberIds(const Family& family)
{
    QStringList ids;
    for (const Person& member : family.members())
    {
        ids.append(member.id().toString());
    }
    return ids;
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

    MinisteringImportResult result = MinisteringImportService::merge(parsed, document.families(),
                                                                     document.wardDirectoryPdfDate());

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

void ImportCommandsTest::attemptOnAFamilyLandsAfterAnImportRemovedItAndTheReportNamesItAsRecorded()
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
    ContactAttempt attempt = TestWard::attempt("attempt-jones", TestWard::jones(), TestWard::octoberFirst(10, 0),
                                               ContactMethod::Phone, TestWard::bob(), ContactOutcome::OK,
                                               "All well");
    QVERIFY(mine.commit(AddContactAttemptCommand::fromNew(attempt)).status == CommitStatus::Applied);

    ReplayResult replay = mine.replayOnto(theirs.document());

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(!mine.document().findFamilyById(TestWard::jonesId()).has_value());
    EmergencyResponse response = mine.document().emergencyResponse().value();
    const ContactAttempt* landed = response.findContactAttempt(attempt.id());
    QVERIFY(landed != nullptr);
    QVERIFY(*landed == attempt);

    // The report names the family as the attempt recorded it, though the ward no longer
    // holds it
    QString text = reportText(mine.document());
    QVERIFY2(text.contains("Phone - Smith, Bob - OK - All well"), qPrintable(text));
    QVERIFY2(text.contains("Jones, Carol"), qPrintable(text));
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

void ImportCommandsTest::importNeverReplacesAnEntryInTheTableOfAddressCoordinates()
{
    Document document = TestWard::document();
    WardDirectoryPdfParser::ParseResult parsed = directoryListing({TestWard::smith(), TestWard::jones()});
    Coordinates listedSmith(40.2300, -111.6500);
    Coordinates listedJones(40.2400, -111.6600);
    parsed.addressCoordinates.insert(TestWard::smith().address().full(), listedSmith);
    parsed.addressCoordinates.insert(TestWard::jones().address().full(), listedJones);
    UndoableCommandPtr command = ImportWardDirectoryCommand::fromImport(
        document, mergeDirectory(parsed, document, octoberFifteenth()), "Tanglewood.pdf");
    QVERIFY(command != nullptr);

    // Someone gives the Smith address its own entry after the import was worked out.
    Coordinates own(40.2338, -111.6585);
    Document applied = TestWard::withCoordinates(document, TestWard::smith().address(), own);

    QVERIFY(command->execute(applied) == CommandOutcome::Applied);

    QVERIFY(applied.coordinatesFor(TestWard::smith().address()) == own);
    QVERIFY(applied.coordinatesFor(TestWard::jones().address()) == listedJones);
}

void ImportCommandsTest::importCreatesTheWardRecordOnlyWhenTheWardLacksIt()
{
    Document document = TestWard::document();
    WardDirectoryImportResult result = mergeDirectory(
        directoryListing({TestWard::smith(), TestWard::jones(), TestWard::lee(2010)}), document, octoberFifteenth());
    UndoableCommandPtr command = ImportWardDirectoryCommand::fromImport(document, result, "Tanglewood.pdf");

    QCOMPARE(command->unitsGiven(), QStringList({"123456"}));
    QVERIFY(command->addressesGiven() == QList<Address>({TestWard::address("8 Pine St", "Provo, UT")}));

    QVERIFY(!document.metadata().findWardByUnit("123456").has_value());
    Document created = document;
    QVERIFY(command->execute(created) == CommandOutcome::Applied);
    Ward listed;
    listed.setUnitNumber("123456");
    listed.setName("Tanglewood");
    QVERIFY(created.metadata().findWardByUnit("123456") == listed);

    Ward own = listed;
    own.setStakeUnitNumber("654321");
    own.setMeetingTime(QString("9:00 AM"));
    Document withWard = document;
    withWard.metadata().updateWard(own);
    UndoableCommandPtr onto = ImportWardDirectoryCommand::fromImport(withWard, result, "Tanglewood.pdf");
    QVERIFY(onto->execute(withWard) == CommandOutcome::Applied);
    QCOMPARE(memberIds(TestWard::family(withWard, FamilyId::fromString("pdf-family-lee"))),
             QStringList({"pdf-person-ken", "pdf-person-mia", "pdf-person-ed", "pdf-person-fay"}));
    QVERIFY(withWard.metadata().findWardByUnit("123456") == own);
}

void ImportCommandsTest::memberThePdfAddsLandsAtItsPdfIndex()
{
    Document document = TestWard::document();
    Family listedSmith = TestWard::smith();
    listedSmith.setMembers({TestWard::bob(), TestWard::dan(), TestWard::alice()});
    UndoableCommandPtr command = ImportWardDirectoryCommand::fromImport(
        document, mergeDirectory(directoryListing({listedSmith, TestWard::jones()}), document, octoberFifteenth()),
        "Tanglewood.pdf");
    Document applied = document;

    QVERIFY(command->execute(applied) == CommandOutcome::Applied);

    QCOMPARE(memberIds(TestWard::family(applied, TestWard::smithId())),
             QStringList({"person-bob", "pdf-person-dan", "person-alice"}));
}

void ImportCommandsTest::undoOfADirectoryImportLeavesTheSeededCoordinatesAndTheWardRecord()
{
    Document document = TestWard::document();
    WardDirectoryPdfParser::ParseResult parsed = directoryListing({TestWard::smith(), TestWard::jones(),
                                                                   TestWard::lee(2010)});
    Coordinates listedLee(40.2400, -111.6600);
    parsed.addressCoordinates.insert(TestWard::lee(2010).address().full(), listedLee);
    TestSession session;
    session.open(document);
    QVERIFY(session.commit(ImportWardDirectoryCommand::fromImport(
                               document, mergeDirectory(parsed, document, octoberFifteenth()), "Tanglewood.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(session.document().findFamilyById(FamilyId::fromString("pdf-family-lee")).has_value());

    QVERIFY(session.undo().status == CommitStatus::Applied);

    QVERIFY(!session.document().findFamilyById(FamilyId::fromString("pdf-family-lee")).has_value());
    QVERIFY(!session.document().wardDirectoryPdfDate().has_value());
    QVERIFY(session.document().coordinatesFor(TestWard::lee(2010).address()) == listedLee);
    std::optional<Ward> ward = session.document().metadata().findWardByUnit("123456");
    QVERIFY(ward.has_value());
    QCOMPARE(ward->name(), QString("Tanglewood"));
}

void ImportCommandsTest::membersSharingAFirstGivenNameKeepTheirOwnIdsInEitherOrder()
{
    Person father = Person::createWithId(PersonId::fromString("person-john-robert"), Name("Carter", "John Robert"),
                                         Gender::Male, Birthday::create(1970, 5, 1), Phone("555-5001"), Phone(),
                                         "johnrobert@example.com", QStringList(), true, QString(), QString());
    father.setNote("Ward clerk");
    Person son = Person::createWithId(PersonId::fromString("person-john-david"), Name("Carter", "John David"),
                                      Gender::Male, Birthday::create(2004, 8, 9), Phone("555-5002"), Phone(),
                                      "johndavid@example.com", QStringList(), false, QString(), QString());
    son.setNote("Away at college");
    Family carter = Family::createWithId(FamilyId::fromString("family-carter"),
                                         TestWard::address("3 Birch Ln", "Provo, UT"), {father, son});
    Document document = TestWard::withFamily(TestWard::document(), carter);

    QList<QList<Person>> orders = {{father, son}, {son, father}};
    for (const QList<Person>& order : orders)
    {
        Family listed = carter;
        listed.setMembers(order);
        WardDirectoryImportResult result = mergeDirectory(
            directoryListing({TestWard::smith(), TestWard::jones(), listed}), document, octoberFifteenth());
        Document applied = document;

        QVERIFY(ImportWardDirectoryCommand::fromImport(document, result, "Tanglewood.pdf")->execute(applied)
                == CommandOutcome::Applied);

        QCOMPARE(memberIds(TestWard::family(applied, carter.id())),
                 QStringList({"person-john-robert", "person-john-david"}));
        Person importedFather = TestWard::person(applied, father.id());
        QCOMPARE(importedFather.givenNames(), QString("John Robert"));
        QCOMPARE(QString(importedFather.phone()), QString("555-5001"));
        QCOMPARE(importedFather.note(), QString("Ward clerk"));
        Person importedSon = TestWard::person(applied, son.id());
        QCOMPARE(importedSon.givenNames(), QString("John David"));
        QCOMPARE(QString(importedSon.phone()), QString("555-5002"));
        QCOMPARE(importedSon.note(), QString("Away at college"));
    }
}

void ImportCommandsTest::eachImportReplayedAfterTheOtherImportChangedTheDateItReadConflicts()
{
    // A directory import, replayed after someone imported a newer ministering PDF.
    TestSession directoryImporter;
    directoryImporter.open(TestWard::document());
    WardDirectoryImportResult directory = mergeDirectory(directoryListing({TestWard::smith()}),
                                                         directoryImporter.document(), octoberFifteenth());
    QVERIFY(directoryImporter.commit(ImportWardDirectoryCommand::fromImport(directoryImporter.document(), directory,
                                                                            "Tanglewood.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(!directoryImporter.document().findFamilyById(TestWard::jonesId()).has_value());
    Document newerMinistering = TestWard::document();
    newerMinistering.setMinisteringPdfDate(QDate(2026, 10, 20));

    ReplayResult directoryReplay = directoryImporter.replayOnto(newerMinistering);

    QVERIFY(directoryReplay.status == ReplayStatus::Abandoned);
    QVERIFY(directoryReplay.toRedo.contains("Import the ward directory from Tanglewood.pdf. (stopped here)"));
    QVERIFY(directoryImporter.document() == newerMinistering);

    // A ministering import, replayed after someone imported a newer ward directory.
    Document read = TestWard::document();
    read.setWardDirectoryPdfDate(octoberFifteenth());
    TestSession ministeringImporter;
    ministeringImporter.open(read);
    MinisteringImportResult ministering = bobMinistersToTheJoneses(read);
    QVERIFY(ministeringImporter.commit(ImportMinisteringCommand::fromImport(read, ministering, "Tanglewood EQ.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(ministeringImporter.document().groups(MinisteringOrg::EldersQuorum) == ministering.groups);
    Document newerDirectory = read;
    newerDirectory.setWardDirectoryPdfDate(QDate(2026, 10, 20));

    ReplayResult ministeringReplay = ministeringImporter.replayOnto(newerDirectory);

    QVERIFY(ministeringReplay.status == ReplayStatus::Abandoned);
    QVERIFY(ministeringReplay.toRedo.contains("Import the ministering assignments from Tanglewood EQ.pdf. (stopped here)"));
    QVERIFY(ministeringImporter.document() == newerDirectory);
}
