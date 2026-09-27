#include "WardDirectoryImportTest.h"

#include "Address.h"
#include "Family.h"
#include "WardDirectoryImportService.h"

#include <QSet>
#include <QtTest>

namespace
{

Person member(const char* givenNames)
{
    return Person::create(Name("Reed", givenNames), false, Phone(), Phone(), Email(), std::nullopt, Birthday(), {});
}

} // namespace

void WardDirectoryImportTest::aMatchedMemberKeepsTheirId()
{
    const Person existingJohn = member("John");
    const Family existing = Family::create(40.0, -111.0, Address(), {existingJohn});
    const Family parsed = Family::create(40.0, -111.0, Address(), {member("John Paul")});

    const Family merged = WardDirectoryImportService::preserveIds(parsed, existing);
    QVERIFY(merged.id() == existing.id());
    QCOMPARE(merged.members().size(), qsizetype(1));
    QVERIFY(merged.members().first().id() == existingJohn.id());
    QCOMPARE(merged.members().first().givenNames(), QString("John Paul"));
}

void WardDirectoryImportTest::twoMembersSharingAFirstNameNeverShareAnId()
{
    // One John on record; the new directory lists John and John Jr. Only one of
    // them is the John already known, and the other is someone new.
    const Person existingJohn = member("John");
    const Family existing = Family::create(40.0, -111.0, Address(), {existingJohn});
    const Family parsed =
        Family::create(40.0, -111.0, Address(), {member("John"), member("John Jr.")});

    const Family merged = WardDirectoryImportService::preserveIds(parsed, existing);
    QCOMPARE(merged.members().size(), qsizetype(2));

    QSet<PersonId> ids;
    for (const Person& person : merged.members())
    {
        ids.insert(person.id());
    }
    QCOMPARE(ids.size(), qsizetype(2));
    QVERIFY(ids.contains(existingJohn.id()));

    // And the family it makes is one the strict reader accepts.
    QVERIFY(Family::fromJson(merged.toJson()).has_value());
}
