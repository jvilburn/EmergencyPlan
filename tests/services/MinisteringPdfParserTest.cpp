#include "MinisteringPdfParserTest.h"
#include "MinisteringPdfParser.h"

#include <QTest>

namespace
{

// Positions and sizes as PdfExtractor reads them from the church's ministering export.
constexpr double MinisterX = 23.4;
constexpr double FamilyX = 168.7;
constexpr double ParentX = 296.2;
constexpr double ChildX = 309.3;
constexpr double GenderX = 457.6;
constexpr double BirthdayX = 516.0;
constexpr float BodySize = 8.5f;

PdfTextField text(const QString& value, double left, double y, bool bold = false, float size = BodySize)
{
    return PdfTextField(value, left, left + 60, y, size, bold);
}

/// A page's header and footer, around content, dated 21 Sep 2025.
QList<PdfTextField> page(int number, const QList<PdfTextField>& content)
{
    QList<PdfTextField> fields = {
        text("Ministering Assignments", 17.3, 23.0, false, 9.6f),
        text("Tanglewood Ward (181048)", 477.4, 23.0, false, 9.6f),
        text("Winston-Salem North Carolina Stake (510033)", 397.6, 34.5, false, 9.6f),
    };
    fields.append(content);
    fields.append(text(QString::number(number), 17.3, 766.8, false, 7.7f));
    fields.append(text("For Church Use Only (c) 2025 by Intellectual Reserve, Inc.", 175.8, 766.8, false, 7.7f));
    fields.append(text("21 Sep 2025", 551.7, 766.8, false, 7.7f));
    return fields;
}

/// A district title and its unassigned presidency member line, at the top of a page.
QList<PdfTextField> districtStart(const QString& title)
{
    return {
        text(title, 23.4, 62.5, false, 16.9f),
        text("Presidency Member:  ", 23.4, 89.6, false, 11.5f),
        text(" Unassigned", 134.9, 89.6, true, 11.5f),
        text(". . . . . . . . . . . . . . . . . . . .", 23.4, 110.8),
        text("Presidency Member: ", 23.4, 128.5),
        text(" Unassigned", 102.6, 128.5, true),
    };
}

/// The dotted line that ends a companionship.
PdfTextField separator(double y)
{
    return text(". . . . . . . . . . . . . . . . . . . .", 23.4, y);
}

/// The family in result holding a member named name, or none.
std::optional<Family> familyWith(const QHash<FamilyId, Family>& families, const QString& name)
{
    for (const Family& family : families)
    {
        for (const Person& member : family.members())
        {
            if (member.displayName() == name)
            {
                return family;
            }
        }
    }
    return std::nullopt;
}

/// The member of family named name.
Person memberNamed(const Family& family, const QString& name)
{
    for (const Person& member : family.members())
    {
        if (member.displayName() == name)
        {
            return member;
        }
    }
    return Person();
}

}  // namespace

MinisteringPdfParserTest::MinisteringPdfParserTest(QObject* parent)
    : QObject(parent)
{
}

void MinisteringPdfParserTest::eldersQuorumCompanionshipReadsMinistersAndHouseholds()
{
    QList<PdfTextField> content = districtStart("District 1");
    content.append({
        text("Flake, Andrew", MinisterX, 165.4, true),
        text("Edwards", FamilyX, 169.3, true),
        text("Edwards, Debra", ParentX, 170.0),
        text("Female", GenderX, 170.0),
        text("6 May", BirthdayX, 170.0),
        text("801-995-2349", MinisterX, 175.4),
        text("336-287-8372", FamilyX, 179.2),
        text("3776 Frye Bridge Road", MinisterX, 185.4),
        text("Edwards, Ann", ChildX, 185.4),
        text("Female", GenderX, 185.4),
        text("7 Oct (16)", BirthdayX, 185.4),
        text("338 Oakland Ave", FamilyX, 189.2),
        text("Clemmons NC 27012", MinisterX, 195.4),
        text("Mocksville NC 27028-8315", FamilyX, 199.2),
        text("a.flake@example.com", MinisterX, 205.4),
        separator(226.0),
    });

    MinisteringPdfParser::ParseResult result = MinisteringPdfParser::parseFields({page(1, content)});

    QVERIFY2(result.success, qPrintable(result.errors.join("; ")));
    QVERIFY(!result.isRSFormat);
    QCOMPARE(result.wardName, QString("Tanglewood Ward"));
    QCOMPARE(result.wardUnitNumber, QString("181048"));
    QCOMPARE(result.documentDate, QDate(2025, 9, 21));
    QCOMPARE(result.groups.size(), qsizetype(1));
    const MinisteringGroup& group = *result.groups.begin();

    std::optional<Family> ministerHousehold = familyWith(result.ministerFamilies, "Flake, Andrew");
    QVERIFY(ministerHousehold.has_value());
    Person minister = ministerHousehold->members().first();
    QCOMPARE(group.ministerIds(), QSet<PersonId>({minister.id()}));
    QCOMPARE(minister.phone().digits(), QString("8019952349"));
    QCOMPARE(QString(minister.email()), QString("a.flake@example.com"));
    QCOMPARE(ministerHousehold->address().lines(), QStringList({"3776 Frye Bridge Road", "Clemmons NC 27012"}));

    std::optional<Family> household = familyWith(result.ministeredFamilies, "Edwards, Debra");
    QVERIFY(household.has_value());
    QCOMPARE(group.familyIds(), QSet<FamilyId>({household->id()}));
    QCOMPARE(household->address().lines(), QStringList({"338 Oakland Ave", "Mocksville NC 27028-8315"}));
    QCOMPARE(household->members().size(), qsizetype(2));
    Person debra = memberNamed(*household, "Edwards, Debra");
    QVERIFY(debra.isParent());
    QVERIFY(debra.gender() == Gender::Female);
    QVERIFY(debra.birthday() == Birthday::create(std::nullopt, 5, 6));
    QCOMPARE(debra.phone().digits(), QString("3362878372"));
    Person ann = memberNamed(*household, "Edwards, Ann");
    QVERIFY(!ann.isParent());
    QVERIFY(ann.gender() == Gender::Female);
    // 16 on 21 Sep 2025 with a birthday still to come that year
    QVERIFY(ann.birthday() == Birthday::create(2008, 10, 7));
}

void MinisteringPdfParserTest::reliefSocietyCompanionshipMinistersToTheSisterItNames()
{
    QList<PdfTextField> content = districtStart("District 1");
    content.append({
        text("Allen, Mary", MinisterX, 146.2, true),
        text("Berlin, Jill", FamilyX, 150.0, true),
        text("Berlin, Aaron Jay", ParentX, 150.8),
        text("Male", GenderX, 150.8),
        text("15 Feb", BirthdayX, 150.8),
        text("336-624-2670", MinisterX, 156.2),
        text("336-918-9474", FamilyX, 160.0),
        text("Berlin, Jill", ParentX, 166.2, true),
        text("Female", GenderX, 166.2),
        text("15 Apr", BirthdayX, 166.2),
        text("110 Coventry Park Ln", FamilyX, 170.0),
        text("Ellsworth, Ashley", MinisterX, 175.5, true),
        text("Winston Salem NC 27104-3666", FamilyX, 180.0),
        text("801-450-6205", MinisterX, 185.5),
        separator(210.0),
    });

    MinisteringPdfParser::ParseResult result = MinisteringPdfParser::parseFields({page(1, content)});

    QVERIFY2(result.success, qPrintable(result.errors.join("; ")));
    QVERIFY(result.isRSFormat);
    QCOMPARE(result.groups.size(), qsizetype(1));
    const MinisteringGroup& group = *result.groups.begin();
    QCOMPARE(group.ministerIds().size(), qsizetype(2));
    std::optional<Family> household = familyWith(result.ministeredFamilies, "Berlin, Jill");
    QVERIFY(household.has_value());
    Person jill = memberNamed(*household, "Berlin, Jill");
    QCOMPARE(group.ministeredPersonIds(), QSet<PersonId>({jill.id()}));
    QVERIFY(jill.gender() == Gender::Female);
    QVERIFY(memberNamed(*household, "Berlin, Aaron Jay").gender() == Gender::Male);
}

void MinisteringPdfParserTest::companionshipsOnEveryPageOfALongListStayApart()
{
    // One district over 60 pages, one companionship on each; each page's fields are
    // page-relative, as PdfExtractor gives them.
    constexpr int PageCount = 60;
    QList<QList<PdfTextField>> pages;
    QHash<QString, QString> expected;  // minister's surname -> household's surname
    for (int number = 1; number <= PageCount; ++number)
    {
        QString suffix = QString(QChar('a' + (number - 1) / 26)) + QChar('a' + (number - 1) % 26);
        QString minister = "M" + suffix;
        QString household = "H" + suffix;
        expected.insert(minister, household);
        QList<PdfTextField> content = number == 1 ? districtStart("District 1") : QList<PdfTextField>();
        content.append({
            text(minister + ", Andrew", MinisterX, 165.4, true),
            text(household, FamilyX, 169.3, true),
            text(household + ", Debra", ParentX, 170.0),
            text("Female", GenderX, 170.0),
            text("6 May", BirthdayX, 170.0),
            text("801-555-0100", MinisterX, 175.4),
            text("12 Elm St", FamilyX, 179.2),
            separator(226.0),
        });
        pages.append(page(number, content));
    }

    MinisteringPdfParser::ParseResult result = MinisteringPdfParser::parseFields(pages);

    QVERIFY2(result.success, qPrintable(result.errors.join("; ")));
    QCOMPARE(result.groups.size(), qsizetype(PageCount));
    QHash<QString, QString> found;
    for (const MinisteringGroup& group : result.groups)
    {
        QCOMPARE(group.ministerIds().size(), qsizetype(1));
        QCOMPARE(group.familyIds().size(), qsizetype(1));
        QString ministerSurname;
        for (const Family& family : result.ministerFamilies)
        {
            if (family.members().first().id() == *group.ministerIds().begin())
            {
                ministerSurname = family.members().first().surname();
            }
        }
        found.insert(ministerSurname, result.ministeredFamilies.value(*group.familyIds().begin()).surname());
    }
    QCOMPARE(found, expected);
}
