#include "PersonJsonTest.h"
#include "TestWard.h"

#include <QTest>

namespace
{

Person bobBornOn(const Birthday& birthday)
{
    Person bob = TestWard::bob();
    bob.setBirthday(birthday);
    return bob;
}

}  // namespace

PersonJsonTest::PersonJsonTest(QObject* parent)
    : QObject(parent)
{
}

void PersonJsonTest::yearOnlyBirthdayRoundTrips()
{
    Person bob = bobBornOn(Birthday::create(1980, std::nullopt, std::nullopt));

    QJsonObject json = bob.toJson();

    QJsonObject birthday = json["birthday"].toObject();
    QCOMPARE(birthday["year"].toInt(), 1980);
    QVERIFY(!birthday.contains("month"));
    QVERIFY(Person::fromJson(json) == bob);
}

void PersonJsonTest::monthAndDayBirthdayRoundTrips()
{
    Person bob = bobBornOn(Birthday::create(std::nullopt, 7, 4));

    QVERIFY(Person::fromJson(bob.toJson()) == bob);
}

void PersonJsonTest::fullBirthdayRoundTrips()
{
    Person bob = bobBornOn(Birthday::create(1980, 7, 4));

    QVERIFY(Person::fromJson(bob.toJson()) == bob);
}

void PersonJsonTest::emptyBirthdayRoundTrips()
{
    Person bob = bobBornOn(Birthday());

    QJsonObject json = bob.toJson();

    QVERIFY(!json.contains("birthday"));
    QVERIFY(Person::fromJson(json) == bob);
}
