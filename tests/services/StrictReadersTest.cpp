#include "StrictReadersTest.h"
#include "CommandRegistry.h"
#include "EveryField.h"
#include "ImportMinisteringCommand.h"
#include "WardFileEdits.h"

#include <QJsonArray>
#include <QTest>

namespace
{

/// One place in the payload's JSON: the steps WardFileEdits::edited takes to reach it, its
/// path as a reader names it in a problem, and what is there. A key of an object also has
/// the object's path and its number of keys.
struct Place
{
    QStringList steps;
    QString path;
    QJsonValue value;
    QString parentPath;
    qsizetype parentKeys;
};

/// The places in a payload's JSON, collected in one walk.
struct Places
{
    QList<Place> objects;  ///< every object, the payload itself first
    QList<Place> fields;   ///< every key of every object
    QList<Place> leaves;   ///< every string, number and boolean, in an object or an array
};

void collect(const Place& place, Places& places)
{
    if (place.value.isObject())
    {
        places.objects.append(place);
        const QJsonObject object = place.value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            Place field{place.steps + QStringList(it.key()),
                        place.path.isEmpty() ? it.key() : place.path + "." + it.key(),
                        it.value(),
                        place.path,
                        object.size()};
            places.fields.append(field);
            collect(field, places);
        }
    }
    else if (place.value.isArray())
    {
        const QJsonArray array = place.value.toArray();
        for (int i = 0; i < array.size(); ++i)
        {
            collect(Place{place.steps + QStringList(QString::number(i)), QString("%1[%2]").arg(place.path).arg(i), array[i],
                          QString(), 0},
                    places);
        }
    }
    else if (place.value.isString() || place.value.isDouble() || place.value.isBool())
    {
        places.leaves.append(place);
    }
}

Places placesIn(const QJsonObject& json)
{
    Places places;
    collect(Place{QStringList(), QString(), json, QString(), 0}, places);
    return places;
}

/// What reading json as a local file's payload gave: the payload, or the problem.
struct Reading
{
    QString problem;
    WardPayload payload;
};

Reading read(const QJsonObject& json)
{
    JsonReader reader(json);
    WardPayload payload = WardPayload::fromJson(reader, PayloadKind::LocalFile);
    return Reading{reader.problem(), payload};
}

/// payload written again, with each journal entry's command rebuilt and written again
/// rather than kept as it was read, so the command readers are checked too.
QJsonObject rewritten(const WardPayload& payload)
{
    QJsonObject json = payload.toJson();
    const QList<JournalEntry>& entries = payload.device->journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(JsonReader(entries[i].command));
        json = WardFileEdits::edited(json, {"device", "journal", "entries", QString::number(i), "command"},
                                     CommandRegistry::toJson(*command));
    }
    return json;
}

/// The index in the journal of EveryField::payload of the first entry of that type.
int journalIndexOf(const QString& type)
{
    const QList<CommandType>& types = CommandRegistry::types();
    for (int i = 0; i < types.size(); ++i)
    {
        if (types[i].name == type)
        {
            return i;
        }
    }
    return -1;
}

}  // namespace

StrictReadersTest::StrictReadersTest(QObject* parent)
    : QObject(parent)
{
}

void StrictReadersTest::initTestCase()
{
    m_payload = EveryField::payload(Identity::generate(), Identity::generate());
    m_json = m_payload.toJson();
}

void StrictReadersTest::everyFieldRoundTrips()
{
    Reading reading = read(m_json);

    QCOMPARE(reading.problem, QString());
    QVERIFY(reading.payload == m_payload);
    QVERIFY(rewritten(reading.payload) == m_json);
}

void StrictReadersTest::everyUnexpectedFieldIsRefusedAtItsPath()
{
    Places places = placesIn(m_json);
    QVERIFY(places.objects.size() > 200);

    QStringList mismatches;
    for (const Place& object : places.objects)
    {
        QJsonObject json = WardFileEdits::edited(m_json, object.steps + QStringList("unexpected"), QJsonValue("hello"));
        QString expected = object.path.isEmpty() ? QString("unexpected: is not expected")
                         : object.path == "landed" ? QString("landed.unexpected: is not a fingerprint")
                                                   : object.path + ".unexpected: is not expected";
        QString problem = read(json).problem;
        if (problem != expected)
        {
            mismatches.append(QString("%1 gave \"%2\"").arg(object.path, problem));
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
}

void StrictReadersTest::everyRemovedFieldIsRefusedOrReadsAsEmpty()
{
    Places places = placesIn(m_json);
    QVERIFY(places.fields.size() > 700);

    QStringList mismatches;
    int missing = 0;
    int leftEmpty = 0;
    int readAsEmpty = 0;
    for (const Place& field : places.fields)
    {
        QJsonObject json = WardFileEdits::edited(m_json, field.steps, QJsonValue(QJsonValue::Undefined));
        Reading reading = read(json);
        if (reading.problem == field.path + ": missing")
        {
            ++missing;
        }
        else if (field.parentKeys == 1 && reading.problem == field.parentPath + ": is written out empty")
        {
            ++leftEmpty;
        }
        else if (reading.problem.isEmpty() && rewritten(reading.payload) == json)
        {
            ++readAsEmpty;
        }
        else
        {
            mismatches.append(QString("%1 gave \"%2\"").arg(field.path, reading.problem));
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
    QVERIFY(missing > 400);
    QVERIFY(leftEmpty > 0);
    QVERIFY(readAsEmpty > 250);
}

void StrictReadersTest::everyLeafOfTheWrongTypeIsRefused()
{
    Places places = placesIn(m_json);
    QVERIFY(places.leaves.size() > 600);

    QStringList mismatches;
    for (const Place& leaf : places.leaves)
    {
        QJsonObject json = WardFileEdits::edited(m_json, leaf.steps, QJsonObject());
        QString problem = read(json).problem;
        if (!problem.startsWith(leaf.path + ": "))
        {
            mismatches.append(QString("%1 gave \"%2\"").arg(leaf.path, problem));
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
}

void StrictReadersTest::unknownWordsAndUnparseableDatesAreRefused_data()
{
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("problem");

    QString importMinistering = QString::number(journalIndexOf(ImportMinisteringCommand::TypeName));

    QTest::newRow("gender") << QStringList({"ward", "families", "0", "members", "0", "gender"}) << "unknown"
                            << "ward.families[0].members[0].gender: is not one of male, female";
    QTest::newRow("response area") << QStringList({"ward", "emergencyResources", "0", "responseArea"}) << "Nowhere"
                                   << "ward.emergencyResources[0].responseArea: is not one of med, rec, com";
    QTest::newRow("contact method") << QStringList({"ward", "emergencyResponse", "contactAttempts", "1", "method"}) << "fax"
                                    << "ward.emergencyResponse.contactAttempts[1].method: is not one of phone, text, "
                                       "email, visit, other";
    QTest::newRow("ministering organization")
        << QStringList({"device", "journal", "entries", importMinistering, "command", "fields", "org"}) << "highPriests"
        << QString("device.journal.entries[%1].command.fields.org: is not one of eldersQuorum, reliefSociety")
               .arg(importMinistering);
    QTest::newRow("directory PDF date") << QStringList({"ward", "wardDirectoryPdfDate"}) << "2024-13-45"
                                        << "ward.wardDirectoryPdfDate: is not a date";
    QTest::newRow("start time") << QStringList({"ward", "emergencyResponse", "startedAt"}) << "yesterday"
                                << "ward.emergencyResponse.startedAt: is not a time";
}

void StrictReadersTest::unknownWordsAndUnparseableDatesAreRefused()
{
    QFETCH(QStringList, steps);
    QFETCH(QString, value);
    QFETCH(QString, problem);
    QVERIFY(read(m_json).problem.isEmpty());

    QCOMPARE(read(WardFileEdits::edited(m_json, steps, value)).problem, problem);
}

void StrictReadersTest::emptyIdIsRefused_data()
{
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QString>("problem");

    QTest::newRow("family id") << QStringList({"ward", "families", "0", "id"}) << "ward.families[0].id: is an empty id";
    QTest::newRow("team member") << QStringList({"ward", "teams", "0", "memberIds", "0"})
                                 << "ward.teams[0].memberIds[0]: is an empty id";
}

void StrictReadersTest::emptyIdIsRefused()
{
    QFETCH(QStringList, steps);
    QFETCH(QString, problem);
    QVERIFY(read(m_json).problem.isEmpty());

    QCOMPARE(read(WardFileEdits::edited(m_json, steps, QString())).problem, problem);
}
