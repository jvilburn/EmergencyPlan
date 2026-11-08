#include "WardListTest.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"
#include "WardList.h"

#include <QTest>

namespace
{

Ward ward(const QString& unitNumber, const QString& name)
{
    Ward ward;
    ward.setUnitNumber(unitNumber);
    ward.setName(name);
    return ward;
}

/// The test ward with one ward record for each name, under made-up unit numbers.
Document withWards(const QStringList& names)
{
    Document document = TestWard::document();
    for (int i = 0; i < names.size(); ++i)
    {
        document.metadata().updateWard(ward(QString::number(100000 + i), names[i]));
    }
    return document;
}

}  // namespace

WardListTest::WardListTest(QObject* parent)
    : QObject(parent)
{
}

void WardListTest::wardNameListsImportedWardsAlphabetically()
{
    QCOMPARE(withWards({}).wardName(), QString("Untitled ward"));
    QCOMPARE(withWards({"Tanglewood"}).wardName(), QString("Tanglewood Ward"));
    QCOMPARE(withWards({"Tanglewood", "Oak"}).wardName(), QString("Oak and Tanglewood Wards"));
    QCOMPARE(withWards({"Tanglewood", "Oak", "Elm"}).wardName(), QString("Elm, Oak and Tanglewood Wards"));
}

void WardListTest::listNamesEachWardAndMarksWaitingChanges()
{
    TestClient client("Laptop");
    client.createWard(withWards({"Tanglewood"}));
    DocumentId tanglewood = client.session().documentId();
    Family smith = TestWard::family(client.session().document(), TestWard::smithId());
    Family edited = TestWard::withPhone(smith, TestWard::bobId(), "555-0100");
    QVERIFY(client.session().commit(UpdateFamilyCommand::fromEdit(smith, edited, client.session().document())).status
            == CommitStatus::Applied);
    client.createWard(withWards({"Oak"}));
    DocumentId oak = client.session().documentId();
    DocumentId damaged = DocumentId::generate();
    client.store().put(damaged, QByteArray("not a ward file"));

    QList<WardListEntry> entries = WardList::read(client.identity(), client.store());

    QCOMPARE(entries.size(), 3);
    QVERIFY(entries[0].document == oak);
    QVERIFY(entries[0].status == OpenStatus::Opened);
    QCOMPARE(entries[0].name, QString("Oak Ward"));
    QVERIFY(!entries[0].changesWaiting);
    QVERIFY(entries[1].document == tanglewood);
    QVERIFY(entries[1].status == OpenStatus::Opened);
    QCOMPARE(entries[1].name, QString("Tanglewood Ward"));
    QVERIFY(entries[1].changesWaiting);
    QVERIFY(entries[2].document == damaged);
    QVERIFY(entries[2].status == OpenStatus::CannotRead);
    QVERIFY(entries[2].name.isEmpty());
    QVERIFY(!entries[2].changesWaiting);
}
