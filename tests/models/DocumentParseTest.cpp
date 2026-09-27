#include "DocumentParseTest.h"

#include "Document.h"
#include "DocumentCrypto.h"
#include "JsonService.h"
#include "ModelSamples.h"

#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

/// Every collection a payload carries, by its top-level key.
const QStringList Collections = {"stakes",
                                 "wards",
                                 "families",
                                 "teams",
                                 "tags",
                                 "emergencyResources",
                                 "eqDistricts",
                                 "eqGroups",
                                 "rsDistricts",
                                 "rsGroups"};

DocumentParse parse(const QJsonObject& json)
{
    Document out;
    return Document::parse(json, out);
}

/// The payload with the first element of one collection replaced.
QJsonObject withFirst(QJsonObject json, const QString& collection, const QJsonObject& element)
{
    QJsonArray array = json[collection].toArray();
    array.replace(0, element);
    json[collection] = array;
    return json;
}

QJsonObject withField(QJsonObject object, const char* key, const QJsonValue& value)
{
    object[QLatin1String(key)] = value;
    return object;
}

} // namespace

void DocumentParseTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentParseTest::aFullPayloadReadsAndWritesBackUnchanged()
{
    const QJsonObject payload = ModelSamples::document();

    Document document;
    QCOMPARE(Document::parse(payload, document), DocumentParse::Ok);

    // Every collection arrived, and the payload written back reads as the same
    // document with the same top-level fields.
    QCOMPARE(document.families().size(), qsizetype(1));
    QCOMPARE(document.teams().size(), qsizetype(1));
    QCOMPARE(document.tags().size(), qsizetype(1));
    QCOMPARE(document.stakes().size(), qsizetype(1));
    QCOMPARE(document.wards().size(), qsizetype(1));
    QVERIFY(document.emergencyResponse().has_value());
    QVERIFY(document.wardDirectoryPdfDate().has_value());

    const QJsonObject written = document.toJson();
    QCOMPARE(written.keys(), payload.keys());

    Document reread;
    QCOMPARE(Document::parse(written, reread), DocumentParse::Ok);
    QVERIFY(reread == document);
}

void DocumentParseTest::aDamagedElementInAnyCollectionRefusesTheWhole()
{
    const QJsonObject payload = ModelSamples::document();
    QCOMPARE(parse(payload), DocumentParse::Ok);

    for (const QString& collection : Collections)
    {
        const QJsonObject first = payload[collection].toArray().first().toObject();
        const QJsonObject damaged = withFirst(payload, collection, withField(first, "unexpected", true));
        QVERIFY2(parse(damaged) == DocumentParse::Malformed, qPrintable(collection));
    }

    QJsonObject damagedResponse = payload;
    damagedResponse["responseData"] = withField(payload["responseData"].toObject(), "unexpected", true);
    QCOMPARE(parse(damagedResponse), DocumentParse::Malformed);

    QJsonObject notAnArray = payload;
    notAnArray["families"] = QJsonObject();
    QCOMPARE(parse(notAnArray), DocumentParse::Malformed);
}

void DocumentParseTest::aRepeatedIdInAnyCollectionRefusesTheWhole()
{
    const QJsonObject payload = ModelSamples::document();

    for (const QString& collection : Collections)
    {
        QJsonObject repeated = payload;
        QJsonArray array = payload[collection].toArray();
        array.append(array.first());
        repeated[collection] = array;
        QVERIFY2(parse(repeated) == DocumentParse::Malformed, qPrintable(collection));
    }
}

void DocumentParseTest::aPersonInTwoFamiliesIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    // The control: a second family of its own people reads.
    QJsonObject otherFamily = withField(ModelSamples::family(), "id", "f2");
    otherFamily["members"] = QJsonArray{withField(ModelSamples::secondPerson(), "id", "p3")};
    QJsonObject twoFamilies = payload;
    twoFamilies["families"] = QJsonArray{ModelSamples::family(), otherFamily};
    QCOMPARE(parse(twoFamilies), DocumentParse::Ok);

    // The same second family claiming Alice.
    otherFamily["members"] = QJsonArray{ModelSamples::person()};
    twoFamilies["families"] = QJsonArray{ModelSamples::family(), otherFamily};
    QCOMPARE(parse(twoFamilies), DocumentParse::Malformed);
}

void DocumentParseTest::aTagNamingAnUnknownPersonOrFamilyIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    QCOMPARE(parse(withFirst(payload, "tags", withField(ModelSamples::tag(), "personIds", QJsonArray{"p9"}))),
             DocumentParse::Malformed);
    QCOMPARE(parse(withFirst(payload, "tags", withField(ModelSamples::tag(), "familyIds", QJsonArray{"f9"}))),
             DocumentParse::Malformed);
}

void DocumentParseTest::aTeamNamingAnUnknownPersonIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    QCOMPARE(parse(withFirst(payload, "teams", withField(ModelSamples::team(), "leaderId", "p9"))),
             DocumentParse::Malformed);
    QCOMPARE(parse(withFirst(payload, "teams", withField(ModelSamples::team(), "memberIds", QJsonArray{"p1", "p9"}))),
             DocumentParse::Malformed);
}

void DocumentParseTest::aDistrictNamingAGroupNotInItsOrganisationIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    // m2 is a Relief Society group; an elders quorum district cannot hold it.
    QCOMPARE(parse(withFirst(payload, "eqDistricts", ModelSamples::district("d1", "m2"))),
             DocumentParse::Malformed);
    QCOMPARE(parse(withFirst(payload, "rsDistricts", ModelSamples::district("d2", "m9"))),
             DocumentParse::Malformed);
}

void DocumentParseTest::anIllFormedImportDateIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    QCOMPARE(parse(withField(payload, "wardDirectoryPdfDate", "2026-8-15")), DocumentParse::Malformed);
    QCOMPARE(parse(withField(payload, "ministeringPdfDate", 20260820)), DocumentParse::Malformed);
}

void DocumentParseTest::jsonServiceReadsWhatItWrites()
{
    Document document;
    QCOMPARE(Document::parse(ModelSamples::document(), document), DocumentParse::Ok);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("ward.wardplanning");

    QString error;
    QVERIFY2(JsonService::saveDocument(path, document, &error), qPrintable(error));

    const JsonResult loaded = JsonService::loadDocument(path);
    QVERIFY2(loaded.success, qPrintable(loaded.errorMessage));
    QVERIFY(loaded.document == document);
}
