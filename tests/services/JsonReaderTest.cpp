#include "JsonReaderTest.h"
#include "Base64Url.h"
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

}  // namespace

JsonReaderTest::JsonReaderTest(QObject* parent)
    : QObject(parent)
{
}

void JsonReaderTest::everyFieldTakenByItsTypeReads()
{
    JsonReader json(objectOf(R"({"name":"Bob","count":3,"ratio":1.5,"on":true,"key":"AAAAAAAAAAAAAAAAAAAAAA",)"
                             R"("inner":{"x":"y"},"list":[{"a":1}],"words":["a","b"],"raw":[1,2]})"));

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
    QCOMPARE(json.value("raw"), QJsonValue(QJsonArray({1, 2})));
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
