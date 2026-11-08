#include "WardListTest.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"
#include "WardList.h"

#include <QTest>

namespace
{

/// The test ward with one ward record, named name.
Document named(const QString& name)
{
    Ward ward;
    ward.setUnitNumber("100000");
    ward.setName(name);
    Document document = TestWard::document();
    document.metadata().updateWard(ward);
    return document;
}

/// first and second, the one whose id is less first.
QList<DocumentId> byId(const DocumentId& first, const DocumentId& second)
{
    if (second.toString() < first.toString())
    {
        return {second, first};
    }
    return {first, second};
}

}  // namespace

WardListTest::WardListTest(QObject* parent)
    : QObject(parent)
{
}

void WardListTest::listNamesEachWardAndMarksWaitingChanges()
{
    TestClient client("Laptop");
    client.createWard(named("Tanglewood"));
    DocumentId tanglewood = client.session().documentId();
    Family smith = TestWard::family(client.session().document(), TestWard::smithId());
    Family edited = TestWard::withPhone(smith, TestWard::bobId(), "555-0100");
    QVERIFY(client.session().commit(UpdateFamilyCommand::fromEdit(smith, edited, client.session().document())).status
            == CommitStatus::Applied);
    client.createWard(named("Oak"));
    DocumentId firstOak = client.session().documentId();
    client.createWard(named("Oak"));
    DocumentId secondOak = client.session().documentId();
    DocumentId firstDamaged = DocumentId::generate();
    DocumentId secondDamaged = DocumentId::generate();
    client.store().put(firstDamaged, QByteArray("not a ward file"));
    client.store().put(secondDamaged, QByteArray("not a ward file either"));

    QList<WardListEntry> entries = WardList::read(client.identity(), client.store());

    // The wards that read by name, wards of one name by id, then the rest by id.
    QList<DocumentId> oaks = byId(firstOak, secondOak);
    QList<DocumentId> damaged = byId(firstDamaged, secondDamaged);
    QCOMPARE(entries.size(), 5);
    for (int i = 0; i < 2; ++i)
    {
        QVERIFY(entries[i].document == oaks[i]);
        QVERIFY(entries[i].status == OpenStatus::Opened);
        QCOMPARE(entries[i].name, QString("Oak Ward"));
        QVERIFY(!entries[i].changesWaiting);
    }
    QVERIFY(entries[2].document == tanglewood);
    QVERIFY(entries[2].status == OpenStatus::Opened);
    QCOMPARE(entries[2].name, QString("Tanglewood Ward"));
    QVERIFY(entries[2].changesWaiting);
    for (int i = 0; i < 2; ++i)
    {
        QVERIFY(entries[3 + i].document == damaged[i]);
        QVERIFY(entries[3 + i].status == OpenStatus::CannotRead);
        QVERIFY(entries[3 + i].name.isEmpty());
        QVERIFY(!entries[3 + i].changesWaiting);
    }
}
