#include "FilterTest.h"
#include "Filter.h"
#include "TestWard.h"

#include <QTest>

FilterTest::FilterTest(QObject* parent)
    : QObject(parent)
{
}

void FilterTest::searchMatchesAPersonsNote()
{
    Filter filter(nullptr);
    filter.setSearchText("VAN");
    QVERIFY(!filter.passes(TestWard::document(), TestWard::smith()));
    QVERIFY(!filter.passes(TestWard::document(), TestWard::bob()));

    Person bob = TestWard::bob();
    bob.setNote("Drives the ward van");
    Family smith = TestWard::withMember(TestWard::smith(), bob);
    Document document = TestWard::withFamily(TestWard::document(), smith);

    QVERIFY(filter.passes(document, smith));
    QVERIFY(filter.passes(document, bob));
    QVERIFY(!filter.passes(document, TestWard::alice()));
    QVERIFY(!filter.passes(document, TestWard::jones()));
}

void FilterTest::searchMatchesAFamilysNote()
{
    Filter filter(nullptr);
    filter.setSearchText("gate");
    QVERIFY(!filter.passes(TestWard::document(), TestWard::smith()));
    QVERIFY(!filter.passes(TestWard::document(), TestWard::alice()));

    Family smith = TestWard::smith();
    smith.setNote("Gate code is 1234");
    Document document = TestWard::withFamily(TestWard::document(), smith);

    QVERIFY(filter.passes(document, smith));
    QVERIFY(filter.passes(document, TestWard::alice()));
    QVERIFY(!filter.passes(document, TestWard::jones()));
    QVERIFY(!filter.passes(document, TestWard::carol()));
}
