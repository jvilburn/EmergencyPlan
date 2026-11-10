#include "ImportCommandsTest.h"
#include "EmergencyCommands.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "MinisteringImportService.h"
#include "PdfText.h"
#include "TestClient.h"
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

/// family as the directory prints it: printed, with no gender, birthday, alt phone or unit
/// numbers, which the directory does not print.
Family listedInDirectory(const Family& family)
{
    Family listed = printed(family);
    QList<Person> members;
    for (Person member : listed.members())
    {
        member.setGender(std::nullopt);
        member.setBirthday(Birthday());
        member.setAltPhone(Phone());
        member.setWardUnitNumber(QString());
        member.setStakeUnitNumber(QString());
        members.append(member);
    }
    listed.setMembers(members);
    return listed;
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
        Family listed = listedInDirectory(family);
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

/// An adult with that id, name and phone, and nothing else.
Person adult(const QString& id, const QString& surname, const QString& givenNames, const QString& phone)
{
    return Person::createWithId(PersonId::fromString(id), Name(surname, givenNames), std::nullopt, Birthday(),
                                Phone(phone), Phone(), QString(), QStringList(), true, QString(), QString());
}

/// A ministering PDF dated 1 Sep 2026 listing ministers' households, ministered households
/// and one group.
MinisteringPdfParser::ParseResult ministeringListing(const QList<Family>& ministers, const QList<Family>& ministered,
                                                     const MinisteringGroup& group)
{
    MinisteringPdfParser::ParseResult parsed;
    parsed.success = true;
    parsed.documentDate = QDate(2026, 9, 1);
    for (const Family& family : ministers)
    {
        parsed.ministerFamilies.insert(family.id(), family);
    }
    for (const Family& family : ministered)
    {
        parsed.ministeredFamilies.insert(family.id(), family);
    }
    parsed.groups.insert(group.id(), group);
    return parsed;
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
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    WardDirectoryImportResult result = mergeDirectory(directoryListing({TestWard::smith()}), mine.session().document(),
                                                      octoberFifteenth());
    QCOMPARE(result.removedFamilyIds, QSet<FamilyId>({TestWard::jonesId()}));
    QVERIFY(mine.session().commit(ImportWardDirectoryCommand::fromImport(mine.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    Family lee = TestWard::lee(2010);
    Document theirs = TestWard::withFamily(TestWard::document(), lee);

    ReplayResult replay = mine.session().replayOnto(theirs);

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(mine.session().document().findFamilyById(lee.id()) == lee);
    QVERIFY(mine.session().document().findFamilyById(TestWard::smithId()) == TestWard::smith());
    QVERIFY(!mine.session().document().findFamilyById(TestWard::jonesId()).has_value());

    QVERIFY(mine.session().undo().status == CommitStatus::Applied);

    QVERIFY(mine.session().document().findFamilyById(TestWard::jonesId()) == TestWard::jones());
    QVERIFY(mine.session().document().findFamilyById(lee.id()) == lee);
}

void ImportCommandsTest::importReplayedAfterAnEditToAFamilyItChangesConflicts()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    Family listedSmith = TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-2222");
    WardDirectoryImportResult result = mergeDirectory(directoryListing({listedSmith, TestWard::jones()}),
                                                      mine.session().document(), octoberFifteenth());
    QVERIFY(mine.session().commit(ImportWardDirectoryCommand::fromImport(mine.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    QCOMPARE(QString(TestWard::person(mine.session().document(), TestWard::bobId()).phone()), QString("555-2222"));
    Document theirs = TestWard::withFamily(TestWard::document(),
                                           TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-9999"));

    ReplayResult replay = mine.session().replayOnto(theirs);

    QVERIFY(replay.status == ReplayStatus::Abandoned);
    QVERIFY(replay.toRedo.contains("Import the ward directory from Tanglewood.pdf. (stopped here)"));
    QCOMPARE(QString(TestWard::person(mine.session().document(), TestWard::bobId()).phone()), QString("555-9999"));
    QVERIFY(mine.session().document() == theirs);
}

void ImportCommandsTest::reimportingTheSameDirectoryKeepsNotesAndCoordinatesAndHoldsNoChange()
{
    Person bob = TestWard::bob();
    bob.setBirthday(Birthday::create(1975, 3, 4));
    bob.setAltPhone(Phone("555-1112"));
    bob.setWardUnitNumber("123456");
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

    TestClient client("Laptop");
    client.createWard(document);
    QVERIFY(client.session().commit(ImportMinisteringCommand::fromImport(client.session().document(), result, "Tanglewood EQ.pdf")).status
            == CommitStatus::Applied);
    QVERIFY(client.session().document().groups(MinisteringOrg::EldersQuorum) == result.groups);
    QVERIFY(client.session().document().families() == document.families());
    QVERIFY(client.session().document().ministeringPdfDate() == QDate(2026, 9, 1));

    // An import whose group names people and families the ward lacks conflicts and changes
    // nothing.
    MinisteringImportResult naming = result;
    naming.groups.insert(group.id(), group);
    Document before = client.session().document();
    QVERIFY(client.session().commit(ImportMinisteringCommand::fromImport(client.session().document(), naming, "Tanglewood EQ.pdf")).status
            == CommitStatus::Conflicted);
    QVERIFY(client.session().document() == before);
}

void ImportCommandsTest::attemptOnAFamilyLandsAfterAnImportRemovedItAndTheReportNamesItAsRecorded()
{
    Document document = TestWard::withResponse(TestWard::document(), TestWard::storm());
    TestClient mine("Laptop");
    TestClient theirs("Phone");
    mine.createWard(document);
    theirs.takeWardOf(mine);
    WardDirectoryImportResult result = mergeDirectory(directoryListing({TestWard::smith()}), document,
                                                      octoberFifteenth());
    QVERIFY(theirs.session().commit(ImportWardDirectoryCommand::fromImport(theirs.session().document(), result, "Tanglewood.pdf")).status
            == CommitStatus::Applied);
    QVERIFY(!theirs.session().document().findFamilyById(TestWard::jonesId()).has_value());

    ContactAttempt attempt = TestWard::attempt("attempt-jones", TestWard::jones(), TestWard::octoberFirst(10, 0),
                                               ContactMethod::Phone, TestWard::bob(), ContactOutcome::OK,
                                               "All well");
    QVERIFY(mine.session().commit(AddContactAttemptCommand::fromNew(attempt)).status == CommitStatus::Applied);

    ReplayResult replay = mine.session().replayOnto(theirs.session().document());

    QVERIFY(replay.status == ReplayStatus::Replayed);
    QVERIFY(!mine.session().document().findFamilyById(TestWard::jonesId()).has_value());
    EmergencyResponse response = mine.session().document().emergencyResponse().value();
    const ContactAttempt* landed = response.findContactAttempt(attempt.id());
    QVERIFY(landed != nullptr);
    QVERIFY(*landed == attempt);

    // The report names the family as the attempt recorded it, though the ward no longer
    // holds it
    QString text = reportText(mine.session().document());
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
    TestClient client("Laptop");
    client.createWard(document);
    QVERIFY(client.session().commit(ImportWardDirectoryCommand::fromImport(
                               document, mergeDirectory(parsed, document, octoberFifteenth()), "Tanglewood.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(client.session().document().findFamilyById(FamilyId::fromString("pdf-family-lee")).has_value());

    QVERIFY(client.session().undo().status == CommitStatus::Applied);

    QVERIFY(!client.session().document().findFamilyById(FamilyId::fromString("pdf-family-lee")).has_value());
    QVERIFY(!client.session().document().wardDirectoryPdfDate().has_value());
    QVERIFY(client.session().document().coordinatesFor(TestWard::lee(2010).address()) == listedLee);
    std::optional<Ward> ward = client.session().document().metadata().findWardByUnit("123456");
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

void ImportCommandsTest::directoryReadsEveryChildOfAFamilyWithMoreThanThree()
{
    // A family as the directory prints it: parents in two columns, then children three to a
    // row, each name over its phone.
    auto name = [](const QString& text, double left, double y)
    { return PdfTextField(text, left, left + 40, y, 9.0f, false); };
    auto phone = [](const QString& text, double left, double y)
    { return PdfTextField(text, left, left + 40, y, 7.0f, false); };
    QList<PdfTextField> page = {
        PdfTextField("Tanglewood Ward - 123456", 200, 400, 20, 13.0f, true),
        PdfTextField("Carter, John & Mary", 32, 150, 60, 11.0f, true),
        PdfTextField("3 Birch Ln", 310, 400, 60, 8.0f, false),
        PdfTextField("Provo, UT", 310, 400, 70, 8.0f, false),
        name("John", 50, 90), name("Mary", 319, 90),
        phone("555-555-1001", 50, 100),
        name("Anna", 50, 130), name("Ben", 229, 130), name("Cara", 408, 130),
        phone("555-555-2001", 50, 140), phone("555-555-2002", 229, 140), phone("555-555-2003", 408, 140),
        name("Dan", 50, 170), name("Eve", 229, 170),
        phone("555-555-2004", 50, 180), phone("555-555-2005", 229, 180),
    };

    WardDirectoryPdfParser::ParseResult parsed = WardDirectoryPdfParser::parseFields({page});

    QVERIFY(parsed.success);
    QCOMPARE(parsed.families.size(), 1);
    QStringList names;
    QStringList phones;
    for (const Person& member : parsed.families.begin()->members())
    {
        names.append(member.givenNames());
        phones.append(QString(member.phone()));
    }
    QCOMPARE(names, QStringList({"John", "Mary", "Anna", "Ben", "Cara", "Dan", "Eve"}));
    QCOMPARE(phones, QStringList({"555-555-1001", "", "555-555-2001", "555-555-2002", "555-555-2003",
                                  "555-555-2004", "555-555-2005"}));
}

void ImportCommandsTest::ministerMatchesTheHouseholdHisPhoneNamesNotAnotherWithHisName()
{
    Person father = adult("person-john-father", "Smith", "John", "555-555-1001");
    Person son = adult("person-john-son", "Smith", "John", "555-555-2002");
    QHash<FamilyId, Family> families = {
        {FamilyId::fromString("family-father"),
         Family::createWithId(FamilyId::fromString("family-father"), TestWard::address("1 Oak St", "Provo, UT"),
                              {father})},
        {FamilyId::fromString("family-son"),
         Family::createWithId(FamilyId::fromString("family-son"), TestWard::address("2 Elm St", "Provo, UT"),
                              {son})},
    };
    Person minister = adult("pdf-john", "Smith", "John", "(555) 555-2002");
    MinisteringPdfParser::ParseResult parsed = ministeringListing(
        {Family::createWithId(FamilyId::fromString("pdf-minister"), TestWard::address("2 Elm St", "Provo, UT"),
                              {minister})},
        {}, MinisteringGroup::createEQ({minister.id()}, {}));

    MinisteringImportResult result = MinisteringImportService::merge(parsed, families, std::nullopt);

    QVERIFY(result.families == families);
    QCOMPARE(result.groups.begin()->ministerIds(), QSet<PersonId>({son.id()}));
}

void ImportCommandsTest::sistersSharingAFirstNameKeepTheirOwnRecords()
{
    Person maryJane = Person::createWithId(PersonId::fromString("person-mary-jane"), Name("Carter", "Mary Jane"),
                                           std::nullopt, Birthday(), Phone(), Phone(), QString(), QStringList(),
                                           false, QString(), QString());
    Person maryBeth = Person::createWithId(PersonId::fromString("person-mary-beth"), Name("Carter", "Mary Beth"),
                                           std::nullopt, Birthday(), Phone(), Phone(), QString(), QStringList(),
                                           false, QString(), QString());
    FamilyId carterId = FamilyId::fromString("family-carter");
    Address birch = TestWard::address("3 Birch Ln", "Provo, UT");
    QHash<FamilyId, Family> families = {{carterId, Family::createWithId(carterId, birch, {maryJane, maryBeth})}};
    // The PDF lists Mary Beth first.
    Person listedBeth = Person::createWithId(PersonId::fromString("pdf-mary-beth"), Name("Carter", "Mary Beth"),
                                             Gender::Female, Birthday::create(2010, 6, 2), Phone(), Phone(),
                                             QString(), QStringList(), false, QString(), QString());
    Person listedJane = Person::createWithId(PersonId::fromString("pdf-mary-jane"), Name("Carter", "Mary Jane"),
                                             Gender::Female, Birthday::create(2008, 5, 1), Phone(), Phone(),
                                             QString(), QStringList(), false, QString(), QString());
    Family listed = Family::createWithId(FamilyId::fromString("pdf-carter"), birch, {listedBeth, listedJane});
    MinisteringPdfParser::ParseResult parsed =
        ministeringListing({}, {listed}, MinisteringGroup::createEQ({}, {listed.id()}));

    MinisteringImportResult result = MinisteringImportService::merge(parsed, families, std::nullopt);

    QCOMPARE(result.families.size(), qsizetype(1));
    Family carter = result.families.value(carterId);
    QCOMPARE(memberIds(carter), QStringList({"person-mary-jane", "person-mary-beth"}));
    QCOMPARE(carter.members()[0].givenNames(), QString("Mary Jane"));
    QVERIFY(carter.members()[0].birthday() == Birthday::create(2008, 5, 1));
    QCOMPARE(carter.members()[1].givenNames(), QString("Mary Beth"));
    QVERIFY(carter.members()[1].birthday() == Birthday::create(2010, 6, 2));
    QCOMPARE(result.groups.begin()->familyIds(), QSet<FamilyId>({carterId}));
}

void ImportCommandsTest::householdsSharingAnAddressAndPhoneResolveToTheLowerId()
{
    // Two households at one address with one phone, each holding a John Smith: nothing the
    // PDF prints tells them apart, so every import picks the lower id.
    Address shared = TestWard::address("4 Pine St", "Provo, UT");
    Person first = adult("person-john-a", "Smith", "John", "555-555-4004");
    Person second = adult("person-john-b", "Smith", "John", "555-555-4004");
    QHash<FamilyId, Family> families = {
        {FamilyId::fromString("family-b"), Family::createWithId(FamilyId::fromString("family-b"), shared, {second})},
        {FamilyId::fromString("family-a"), Family::createWithId(FamilyId::fromString("family-a"), shared, {first})},
    };
    Person minister = adult("pdf-john", "Smith", "John", "555-555-4004");
    MinisteringPdfParser::ParseResult parsed = ministeringListing(
        {Family::createWithId(FamilyId::fromString("pdf-minister"), shared, {minister})}, {},
        MinisteringGroup::createEQ({minister.id()}, {}));

    MinisteringImportResult result = MinisteringImportService::merge(parsed, families, std::nullopt);

    QVERIFY(result.families == families);
    QCOMPARE(result.groups.begin()->ministerIds(), QSet<PersonId>({first.id()}));
}

void ImportCommandsTest::directoryFamilyWhoseParentsChangeKeepsItsIdAndNotes()
{
    // Bob was single; the directory now lists the family as "Smith, Bob & Alice".
    Person bob = TestWard::bob();
    bob.setNote("Drives the ward van");
    Family single = Family::createWithId(TestWard::smithId(), TestWard::smith().address(), {bob});
    single.setNote("Gate code is 1234");
    Document document = TestWard::withFamily(TestWard::document(), single);

    WardDirectoryImportResult result =
        mergeDirectory(directoryListing({TestWard::smith(), TestWard::jones()}), document, octoberFifteenth());

    QVERIFY(result.removedFamilyIds.isEmpty());
    QVERIFY(result.families.contains(TestWard::smithId()));
    Family smith = result.families.value(TestWard::smithId());
    QCOMPARE(smith.displayName(), TestWard::smith().displayName());
    QCOMPARE(smith.note(), QString("Gate code is 1234"));
    QCOMPARE(memberIds(smith), QStringList({"person-bob", "pdf-person-alice"}));
    QCOMPARE(smith.members().first().note(), QString("Drives the ward van"));
}

void ImportCommandsTest::newerMinisteringPdfUpdatesAMovedFamilyAndAnOlderOneOnlyFillsGaps()
{
    FamilyId carterId = FamilyId::fromString("family-carter");
    Person john = Person::createWithId(PersonId::fromString("person-john"), Name("Carter", "John"), Gender::Male,
                                       Birthday::create(1970, 5, 1), Phone("555-555-1001"), Phone(),
                                       "john.old@example.com", QStringList(), true, QString(), QString());
    Person mary = Person::createWithId(PersonId::fromString("person-mary"), Name("Carter", "Mary"), Gender::Female,
                                       Birthday(), Phone(), Phone(), QString(), QStringList(), true, QString(),
                                       QString());
    Address birch = TestWard::address("3 Birch Ln", "Provo, UT");
    QHash<FamilyId, Family> families = {{carterId, Family::createWithId(carterId, birch, {john, mary})}};

    // The family moved: the PDF, dated 1 Sep 2026, prints a new address, phone and email,
    // and adults' birthdays without a year.
    Address elm = TestWard::address("9 Elm St", "Provo, UT 84601");
    Person listedJohn = Person::createWithId(PersonId::fromString("pdf-john"), Name("Carter", "John"), Gender::Male,
                                             Birthday::create(std::nullopt, 5, 1), Phone("(555) 555-9999"), Phone(),
                                             "john.new@example.com", QStringList(), true, QString(), QString());
    Person listedMary = Person::createWithId(PersonId::fromString("pdf-mary"), Name("Carter", "Mary"),
                                             Gender::Female, Birthday::create(std::nullopt, 7, 4),
                                             Phone("555-555-2002"), Phone(), QString(), QStringList(), true,
                                             QString(), QString());
    Family listed = Family::createWithId(FamilyId::fromString("pdf-carter"), elm, {listedJohn, listedMary});
    MinisteringPdfParser::ParseResult parsed =
        ministeringListing({}, {listed}, MinisteringGroup::createEQ({}, {listed.id()}));

    // Newer than the directory: what it prints replaces what the ward holds
    MinisteringImportResult newer = MinisteringImportService::merge(parsed, families, QDate(2026, 8, 1));
    Family moved = newer.families.value(carterId);
    QVERIFY(moved.address() == elm);
    QCOMPARE(QString(moved.members()[0].phone()), QString("(555) 555-9999"));
    QCOMPARE(QString(moved.members()[0].email()), QString("john.new@example.com"));
    QVERIFY(moved.members()[0].birthday() == Birthday::create(1970, 5, 1));  // the same day keeps its year
    QCOMPARE(QString(moved.members()[1].phone()), QString("555-555-2002"));
    QVERIFY(moved.members()[1].birthday() == Birthday::create(std::nullopt, 7, 4));

    // Older than the directory: it only fills what the ward lacks
    MinisteringImportResult older = MinisteringImportService::merge(parsed, families, QDate(2026, 10, 1));
    Family kept = older.families.value(carterId);
    QVERIFY(kept.address() == birch);
    QCOMPARE(QString(kept.members()[0].phone()), QString("555-555-1001"));
    QCOMPARE(QString(kept.members()[0].email()), QString("john.old@example.com"));
    QCOMPARE(QString(kept.members()[1].phone()), QString("555-555-2002"));
    QVERIFY(kept.members()[1].birthday() == Birthday::create(std::nullopt, 7, 4));
}

void ImportCommandsTest::eachImportReplayedAfterTheOtherImportChangedTheDateItReadConflicts()
{
    // A directory import, replayed after someone imported a newer ministering PDF.
    TestClient directoryImporter("Laptop");
    directoryImporter.createWard(TestWard::document());
    WardDirectoryImportResult directory = mergeDirectory(directoryListing({TestWard::smith()}),
                                                         directoryImporter.session().document(), octoberFifteenth());
    QVERIFY(directoryImporter.session().commit(ImportWardDirectoryCommand::fromImport(directoryImporter.session().document(), directory,
                                                                            "Tanglewood.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(!directoryImporter.session().document().findFamilyById(TestWard::jonesId()).has_value());
    Document newerMinistering = TestWard::document();
    newerMinistering.setMinisteringPdfDate(QDate(2026, 10, 20));

    ReplayResult directoryReplay = directoryImporter.session().replayOnto(newerMinistering);

    QVERIFY(directoryReplay.status == ReplayStatus::Abandoned);
    QVERIFY(directoryReplay.toRedo.contains("Import the ward directory from Tanglewood.pdf. (stopped here)"));
    QVERIFY(directoryImporter.session().document() == newerMinistering);

    // A ministering import, replayed after someone imported a newer ward directory.
    Document read = TestWard::document();
    read.setWardDirectoryPdfDate(octoberFifteenth());
    TestClient ministeringImporter("Laptop");
    ministeringImporter.createWard(read);
    MinisteringImportResult ministering = bobMinistersToTheJoneses(read);
    QVERIFY(ministeringImporter.session().commit(ImportMinisteringCommand::fromImport(read, ministering, "Tanglewood EQ.pdf"))
                .status
            == CommitStatus::Applied);
    QVERIFY(ministeringImporter.session().document().groups(MinisteringOrg::EldersQuorum) == ministering.groups);
    Document newerDirectory = read;
    newerDirectory.setWardDirectoryPdfDate(QDate(2026, 10, 20));

    ReplayResult ministeringReplay = ministeringImporter.session().replayOnto(newerDirectory);

    QVERIFY(ministeringReplay.status == ReplayStatus::Abandoned);
    QVERIFY(ministeringReplay.toRedo.contains("Import the ministering assignments from Tanglewood EQ.pdf. (stopped here)"));
    QVERIFY(ministeringImporter.session().document() == newerDirectory);
}
