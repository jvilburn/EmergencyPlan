#include "JsonReaderTest.h"
#include "Base64Url.h"
#include "Id.h"
#include "JsonReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

namespace
{

QJsonObject objectOf(const char* text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

/// 1 is small and 2 large.
QString sizeWord(int size)
{
    return size == 1 ? "small" : "large";
}

}  // namespace

JsonReaderTest::JsonReaderTest(QObject* parent)
    : QObject(parent)
{
}

void JsonReaderTest::everyFieldTakenByItsTypeReads()
{
    JsonReader json(objectOf(R"({"name":"Bob","count":3,"ratio":1.5,"on":true,"key":"AAAAAAAAAAAAAAAAAAAAAA",)"
                             R"("inner":{"x":"y"},"list":[{"a":1}],"words":["a","b"]})"));

    QCOMPARE(json.string("name"), QString("Bob"));
    QCOMPARE(json.integer("count"), qint64(3));
    QCOMPARE(json.number("ratio"), 1.5);
    QVERIFY(json.boolean("on"));
    QCOMPARE(json.bytes("key", 16), QByteArray(16, '\0'));
    JsonReader inner = json.object("inner");
    QCOMPARE(inner.string("x"), QString("y"));
    inner.finish();
    QList<JsonReader> list = json.objects("list");
    QCOMPARE(list.size(), qsizetype(1));
    QCOMPARE(list[0].integer("a"), qint64(1));
    list[0].finish();
    QCOMPARE(json.strings("words"), QStringList({"a", "b"}));
    json.finish();

    QVERIFY(!json.failed());
    QCOMPARE(json.problem(), QString());
}

void JsonReaderTest::requiredFieldThatIsMissingFails()
{
    JsonReader json(objectOf("{}"));

    QCOMPARE(json.string("name"), QString());

    QVERIFY(json.failed());
    QCOMPARE(json.problem(), QString("name: missing"));
}

void JsonReaderTest::fieldOfTheWrongTypeFails()
{
    JsonReader text(objectOf(R"({"name":3})"));
    text.string("name");
    QCOMPARE(text.problem(), QString("name: is not a string"));

    JsonReader quoted(objectOf(R"({"count":"3"})"));
    quoted.integer("count");
    QCOMPARE(quoted.problem(), QString("count: is not an integer"));

    JsonReader fraction(objectOf(R"({"count":1.5})"));
    fraction.integer("count");
    QCOMPARE(fraction.problem(), QString("count: is not an integer"));

    JsonReader ratio(objectOf(R"({"ratio":"1.5"})"));
    ratio.number("ratio");
    QCOMPARE(ratio.problem(), QString("ratio: is not a number"));

    JsonReader flag(objectOf(R"({"on":1})"));
    flag.boolean("on");
    QCOMPARE(flag.problem(), QString("on: is not a boolean"));

    JsonReader list(objectOf(R"({"list":{"a":1}})"));
    list.objects("list");
    QCOMPARE(list.problem(), QString("list: is not an array"));

    JsonReader words(objectOf(R"({"words":["a",2]})"));
    words.strings("words");
    QCOMPARE(words.problem(), QString("words[1]: is not a string"));
}

void JsonReaderTest::fieldLeftUntakenFailsAtFinish()
{
    JsonReader exact(objectOf(R"({"name":"Bob"})"));
    exact.string("name");
    exact.finish();
    QVERIFY(!exact.failed());

    JsonReader extra(objectOf(R"({"name":"Bob","nickname":"Rob"})"));
    extra.string("name");
    QVERIFY(!extra.failed());
    extra.finish();
    QCOMPARE(extra.problem(), QString("nickname: is not expected"));
}

void JsonReaderTest::optionalFieldReadsItsAbsenceAsEmptyAndRefusesEmptyWrittenOut()
{
    JsonReader absent(objectOf("{}"));
    QCOMPARE(absent.optionalString("note"), QString());
    QVERIFY(!absent.optionalFlag("flag"));
    QVERIFY(absent.optionalObjects("list").isEmpty());
    QVERIFY(!absent.optionalInteger("version").has_value());
    QVERIFY(!absent.optionalNumber("weight").has_value());
    absent.finish();
    QVERIFY(!absent.failed());

    JsonReader present(objectOf(R"({"note":"Oxygen at night","flag":true,"version":4,"weight":2.5})"));
    QCOMPARE(present.optionalString("note"), QString("Oxygen at night"));
    QVERIFY(present.optionalFlag("flag"));
    QCOMPARE(present.optionalInteger("version").value(), qint64(4));
    QCOMPARE(present.optionalNumber("weight").value(), 2.5);
    present.finish();
    QVERIFY(!present.failed());

    JsonReader emptyNote(objectOf(R"({"note":""})"));
    emptyNote.optionalString("note");
    QCOMPARE(emptyNote.problem(), QString("note: is written out empty"));

    JsonReader falseFlag(objectOf(R"({"flag":false})"));
    falseFlag.optionalFlag("flag");
    QCOMPARE(falseFlag.problem(), QString("flag: is written out empty"));

    JsonReader emptyList(objectOf(R"({"list":[]})"));
    emptyList.optionalObjects("list");
    QCOMPARE(emptyList.problem(), QString("list: is written out empty"));

    JsonReader emptyObject(objectOf(R"({"inner":{}})"));
    QVERIFY(!emptyObject.optionalObject("inner").has_value());
    QCOMPARE(emptyObject.problem(), QString("inner: is written out empty"));

    JsonReader presentObject(objectOf(R"({"inner":{"x":"y"}})"));
    std::optional<JsonReader> inner = presentObject.optionalObject("inner");
    QVERIFY(inner.has_value());
    QCOMPARE(inner->string("x"), QString("y"));
    QVERIFY(!presentObject.failed());
}

void JsonReaderTest::firstFailureIsKeptWithItsPath()
{
    JsonReader json(objectOf(R"({"users":[{"name":"Alice"},{"name":3}]})"));

    QList<JsonReader> users = json.objects("users");
    QCOMPARE(users[0].string("name"), QString("Alice"));
    QVERIFY(!json.failed());
    users[1].string("name");
    json.integer("missing");

    QVERIFY(json.failed());
    QVERIFY(users[0].failed());
    QCOMPARE(json.problem(), QString("users[1].name: is not a string"));
}

void JsonReaderTest::wordIsOneOfItsWords()
{
    JsonReader json(objectOf(R"({"shade":"dark","size":"large"})"));
    QCOMPARE(json.word("shade", {"light", "dark"}), QString("dark"));
    QVERIFY(json.optionalWord("size", QList<int>{1, 2}, &sizeWord) == std::optional<int>(2));
    QVERIFY(!json.optionalWord("fit", QList<int>{1, 2}, &sizeWord).has_value());
    QCOMPARE(json.optionalWord("tone", {"warm", "cool"}), QString());
    json.finish();
    QVERIFY(!json.failed());

    JsonReader other(objectOf(R"({"size":"Large"})"));
    QCOMPARE(other.word("size", QList<int>{1, 2}, &sizeWord), 1);
    QCOMPARE(other.problem(), QString("size: is not one of small, large"));

    JsonReader missing(objectOf("{}"));
    missing.word("size", {"small", "large"});
    QCOMPARE(missing.problem(), QString("size: missing"));
}

void JsonReaderTest::datesAndTimesReadBackAsWritten_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("date") << "date" << "2026-10-01" << true;
    QTest::newRow("date out of range") << "date" << "2024-13-45" << false;
    QTest::newRow("date without leading zeros") << "date" << "2026-1-5" << false;
    QTest::newRow("date with a time") << "date" << "2026-10-01T08:00:00" << false;
    QTest::newRow("time") << "time" << "2026-10-01T08:00:00.000Z" << true;
    QTest::newRow("time without milliseconds") << "time" << "2026-10-01T08:00:00Z" << false;
    QTest::newRow("time of a date alone") << "time" << "2026-10-01" << false;
    QTest::newRow("time in words") << "time" << "yesterday" << false;
}

void JsonReaderTest::datesAndTimesReadBackAsWritten()
{
    QFETCH(QString, kind);
    QFETCH(QString, text);
    QFETCH(bool, accepted);
    QJsonObject object;
    object["when"] = text;
    JsonReader json(object);

    if (kind == "date")
    {
        QDate date = json.date("when");
        QCOMPARE(date.isValid(), accepted);
        if (accepted)
        {
            QCOMPARE(date.toString(Qt::ISODate), text);
        }
    }
    else
    {
        QDateTime time = json.time("when");
        QCOMPARE(time.isValid(), accepted);
        if (accepted)
        {
            QCOMPARE(time.toString(Qt::ISODateWithMs), text);
        }
    }

    QCOMPARE(json.failed(), !accepted);
    if (!accepted)
    {
        QCOMPARE(json.problem(), QString("when: is not a %1").arg(kind));
    }
}

void JsonReaderTest::idsAreNonEmptyStrings()
{
    JsonReader json(objectOf(R"({"id":"family-smith","leader":"person-bob","members":["person-bob","person-alice"]})"));
    QVERIFY(json.id<FamilyId>("id") == FamilyId::fromString("family-smith"));
    QVERIFY(json.optionalId<PersonId>("leader") == std::optional<PersonId>(PersonId::fromString("person-bob")));
    QVERIFY(!json.optionalId<PersonId>("deputy").has_value());
    QVERIFY(json.ids<PersonId>("members")
            == QSet<PersonId>({PersonId::fromString("person-bob"), PersonId::fromString("person-alice")}));
    QVERIFY(json.optionalIds<PersonId>("helpers").isEmpty());
    json.finish();
    QVERIFY(!json.failed());

    JsonReader emptyId(objectOf(R"({"id":""})"));
    emptyId.id<FamilyId>("id");
    QCOMPARE(emptyId.problem(), QString("id: is an empty id"));

    JsonReader emptyMember(objectOf(R"({"members":["person-bob",""]})"));
    QVERIFY(emptyMember.ids<PersonId>("members").isEmpty());
    QCOMPARE(emptyMember.problem(), QString("members[1]: is an empty id"));
}

void JsonReaderTest::nullIsTakenOnlyWhereWritten()
{
    JsonReader json(objectOf(R"({"before":null,"after":"Bob"})"));
    QVERIFY(json.readNull("before"));
    QVERIFY(!json.readNull("after"));
    QVERIFY(!json.readNull("missing"));
    json.finish();
    QCOMPARE(json.problem(), QString("after: is not expected"));

    JsonReader read(objectOf(R"({"after":"Bob"})"));
    QVERIFY(!read.readNull("after"));
    QCOMPARE(read.string("after"), QString("Bob"));
    read.finish();
    QVERIFY(!read.failed());
}

void JsonReaderTest::bytesAreUnpaddedBase64UrlOfTheirExactLength_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("sixteen zero bytes") << QString("AAAAAAAAAAAAAAAAAAAAAA") << true;
    QTest::newRow("url alphabet") << QString("-_AAAAAAAAAAAAAAAAAAAA") << true;
    QTest::newRow("padded") << QString("AAAAAAAAAAAAAAAAAAAAAA==") << false;
    QTest::newRow("standard alphabet") << QString("+/AAAAAAAAAAAAAAAAAAAA") << false;
    QTest::newRow("fifteen bytes") << QString("AAAAAAAAAAAAAAAAAAAA") << false;
    QTest::newRow("trailing bits set") << QString("AAAAAAAAAAAAAAAAAAAAAB") << false;
    QTest::newRow("whitespace") << QString("AAAAAAAAAAA AAAAAAAAAA") << false;
}

void JsonReaderTest::bytesAreUnpaddedBase64UrlOfTheirExactLength()
{
    QFETCH(QString, text);
    QFETCH(bool, accepted);

    std::optional<QByteArray> decoded = Base64Url::decode(text, 16);

    QCOMPARE(decoded.has_value(), accepted);
    if (accepted)
    {
        QCOMPARE(decoded->size(), qsizetype(16));
        QCOMPARE(Base64Url::encode(*decoded), text);
    }
}
