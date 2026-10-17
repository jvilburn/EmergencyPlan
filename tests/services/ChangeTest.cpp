#include "ChangeTest.h"
#include "Change.h"
#include "Id.h"

#include <QTest>

namespace
{

PersonId a() { return PersonId::fromString("a"); }
PersonId b() { return PersonId::fromString("b"); }
PersonId c() { return PersonId::fromString("c"); }

}  // namespace

ChangeTest::ChangeTest(QObject* parent)
    : QObject(parent)
{
}

void ChangeTest::changeHoldsAtBeforeOrAfterOnly()
{
    Change<QString> change("555-1111", "555-2222");
    QVERIFY(change.holds("555-1111"));
    QVERIFY(change.holds("555-2222"));
    QVERIFY(!change.holds("555-9999"));
    QVERIFY(!change.holds(QString()));
}

void ChangeTest::changeSwappedExchangesItsEnds()
{
    Change<QString> swapped = Change<QString>("555-1111", "555-2222").swapped();
    QCOMPARE(swapped.before(), QString("555-2222"));
    QCOMPARE(swapped.after(), QString("555-1111"));
}

void ChangeTest::changeBetweenEqualValuesIsNone()
{
    QVERIFY(!changeBetween(QString("x"), QString("x")).has_value());
    std::optional<Change<QString>> change = changeBetween(QString("x"), QString("y"));
    QVERIFY(change.has_value());
    QCOMPARE(change->before(), QString("x"));
    QCOMPARE(change->after(), QString("y"));
}

void ChangeTest::setChangeBetweenHoldsTheIdsAddedAndRemoved()
{
    SetChange<PersonId> change = SetChange<PersonId>::between({a(), b()}, {b(), c()});
    QVERIFY(change.added() == QSet<PersonId>({c()}));
    QVERIFY(change.removed() == QSet<PersonId>({a()}));
}

void ChangeTest::setChangeAdditionPresentAndRemovalAbsentAreDone()
{
    SetChange<PersonId> change({c()}, {a()});
    QVERIFY(change.appliedTo({b(), c()}) == QSet<PersonId>({b(), c()}));
    QVERIFY(change.appliedTo({a(), b()}) == QSet<PersonId>({b(), c()}));
}

void ChangeTest::setChangesAddingDifferentIdsBothLand()
{
    SetChange<PersonId> mine = SetChange<PersonId>::between({a()}, {a(), b()});
    SetChange<PersonId> theirs = SetChange<PersonId>::between({a()}, {a(), c()});
    QVERIFY(mine.appliedTo(theirs.appliedTo({a()})) == QSet<PersonId>({a(), b(), c()}));
    QVERIFY(theirs.appliedTo(mine.appliedTo({a()})) == QSet<PersonId>({a(), b(), c()}));
}

void ChangeTest::setChangeSwappedTakesItselfBack()
{
    SetChange<PersonId> change = SetChange<PersonId>::between({a(), b()}, {b(), c()});
    QSet<PersonId> changed = change.appliedTo({a(), b()});
    QVERIFY(changed == QSet<PersonId>({b(), c()}));
    QVERIFY(change.swapped().appliedTo(changed) == QSet<PersonId>({a(), b()}));
}
