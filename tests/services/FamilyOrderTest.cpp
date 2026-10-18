#include "FamilyOrderTest.h"
#include "Family.h"
#include "TestWard.h"

#include <QTest>

FamilyOrderTest::FamilyOrderTest(QObject* parent)
    : QObject(parent)
{
}

void FamilyOrderTest::childGoesAmongTheChildrenOldestToYoungest()
{
    QCOMPARE(TestWard::lee(2010).insertionIndexFor(TestWard::gus()), qsizetype(3));
}

void FamilyOrderTest::childYoungerThanEveryoneGoesAtTheEnd()
{
    QCOMPARE(TestWard::lee(2006).insertionIndexFor(TestWard::gus()), qsizetype(4));
}

void FamilyOrderTest::fatherGoesAfterTheFathersWhateverHisAge()
{
    Person jim = TestWard::leeMember("person-jim", "Jim", Gender::Male, 1970, true);

    QCOMPARE(TestWard::lee(2010).insertionIndexFor(jim), qsizetype(1));
}

void FamilyOrderTest::motherGoesAfterTheMothers()
{
    Person ann = TestWard::leeMember("person-ann", "Ann", Gender::Female, 1970, true);

    QCOMPARE(TestWard::lee(2010).insertionIndexFor(ann), qsizetype(2));
}

void FamilyOrderTest::otherParentGoesAfterTheMothersAndBeforeTheChildren()
{
    Person sam = TestWard::leeMember("person-sam", "Sam", std::nullopt, 1970, true);

    QCOMPARE(TestWard::lee(2010).insertionIndexFor(sam), qsizetype(2));
}

void FamilyOrderTest::endWhenAMembersBirthYearIsUnknown()
{
    QCOMPARE(TestWard::lee(std::nullopt).insertionIndexFor(TestWard::gus()), qsizetype(4));
}

void FamilyOrderTest::endWhenThePersonsBirthYearIsUnknown()
{
    Person jim = TestWard::leeMember("person-jim", "Jim", Gender::Male, std::nullopt, true);

    QCOMPARE(TestWard::lee(2010).insertionIndexFor(jim), qsizetype(4));
}
