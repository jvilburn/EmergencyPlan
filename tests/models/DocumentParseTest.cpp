#include "DocumentParseTest.h"

#include "Document.h"
#include "DocumentCrypto.h"
#include "ModelSamples.h"

#include <QJsonArray>
#include <QJsonDocument>
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

bool reads(const QJsonObject& json)
{
    return Document::parse(json).has_value();
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

    const std::optional<Document> document = Document::parse(payload);
    QVERIFY(document.has_value());

    // Written back, it is the payload exactly: every collection in its place, so a
    // reader that filed elders quorum districts under Relief Society fails here.
    const QJsonObject written = document.value().toJson();
    QVERIFY2(written == payload, QJsonDocument(written).toJson().constData());
}

void DocumentParseTest::aDamagedElementInAnyCollectionRefusesTheWhole()
{
    const QJsonObject payload = ModelSamples::document();
    QVERIFY(reads(payload));

    for (const QString& collection : Collections)
    {
        const QJsonObject first = payload[collection].toArray().first().toObject();
        const QJsonObject damaged = withFirst(payload, collection, withField(first, "unexpected", true));
        QVERIFY2(!reads(damaged), qPrintable(collection));
    }

    QVERIFY(!reads(withField(payload, "responseData", withField(payload["responseData"].toObject(), "unexpected", true))));
    QVERIFY(!reads(withField(payload, "families", QJsonObject())));
    QVERIFY(!reads(withField(payload, "families", QJsonArray{"f1"})));
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
        QVERIFY2(!reads(repeated), qPrintable(collection));
    }
}

void DocumentParseTest::aCollectionPresentButEmptyIsRefused()
{
    // The writer omits an empty collection.
    const QJsonObject payload = ModelSamples::document();

    for (const QString& collection : Collections)
    {
        QVERIFY2(!reads(withField(payload, collection.toLatin1().constData(), QJsonArray())),
                 qPrintable(collection));

        QJsonObject absent = payload;
        absent.remove(collection);
        QVERIFY2(reads(absent), qPrintable(collection));
    }
}

void DocumentParseTest::anIllFormedImportDateIsRefused()
{
    const QJsonObject payload = ModelSamples::document();

    QVERIFY(!reads(withField(payload, "wardDirectoryPdfDate", "2026-8-15")));
    QVERIFY(!reads(withField(payload, "wardDirectoryPdfDate", "2026-02-30")));
    QVERIFY(!reads(withField(payload, "ministeringPdfDate", 20260820)));
}

void DocumentParseTest::anIdNamingSomethingNotInTheDocumentIsRead()
{
    // Removing a family member leaves them named in teams and tags, and the app
    // has always tolerated that, so the reader does too.
    const QJsonObject payload = ModelSamples::document();

    QVERIFY(reads(withFirst(payload, "tags", withField(ModelSamples::tag(), "personIds", QJsonArray{"p9"}))));
    QVERIFY(reads(withFirst(payload, "teams", withField(ModelSamples::team(), "leaderId", "p9"))));
    QVERIFY(reads(withFirst(payload, "eqDistricts", ModelSamples::district("d1", "m9"))));
}
