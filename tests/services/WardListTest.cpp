#include "WardListTest.h"
#include "AccessCommands.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardList.h"

#include <QTest>

#include <algorithm>

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

/// The entry of entries for document.
WardListEntry entryOf(const QList<WardListEntry>& entries, const DocumentId& document)
{
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&](const WardListEntry& entry) { return entry.document == document; });
    Q_ASSERT(it != entries.end());
    return *it;
}

}  // namespace

WardListTest::WardListTest(QObject* parent)
    : QObject(parent)
{
}

void WardListTest::listNamesEachWardAndMarksChangesToShare()
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
        QVERIFY(!entries[i].changesToShare);
    }
    QVERIFY(entries[2].document == tanglewood);
    QVERIFY(entries[2].status == OpenStatus::Opened);
    QCOMPARE(entries[2].name, QString("Tanglewood Ward"));
    QVERIFY(entries[2].changesToShare);
    for (int i = 0; i < 2; ++i)
    {
        QVERIFY(entries[3 + i].document == damaged[i]);
        QVERIFY(entries[3 + i].status == OpenStatus::CannotRead);
        QVERIFY(entries[3 + i].name.isEmpty());
        QVERIFY(!entries[3 + i].changesToShare);
    }
}

void WardListTest::listMarksChangesToShareAndWardsNoLongerShared()
{
    TestClient laptop("Laptop");
    TestClient bob("Bob");
    DocumentId unsent = TestSharing::share({&laptop}, TestSharing::accessOf({&laptop}, {}));
    QVERIFY(TestSharing::commitBobsPhone(laptop, "555-0100") == CommitStatus::Applied);
    DocumentId sent = TestSharing::share({&laptop}, TestSharing::accessOf({&laptop}, {}));
    QVERIFY(TestSharing::commitBobsPhone(laptop, "555-0101") == CommitStatus::Applied);
    laptop.session().writeUpdate();
    QCOMPARE(laptop.session().journal().entries().size(), qsizetype(1));
    DocumentId gone = TestSharing::share({&laptop, &bob}, TestSharing::accessOf({&bob}, {&laptop}));
    QVERIFY(bob.session()
                .commit(RemoveUserCommand::create(bob.identity().fingerprint(), TestSharing::userNamed(bob, "Laptop")))
                .status
            == CommitStatus::Applied);
    QVERIFY(TestSharing::send(bob, {&laptop}) == QList<TakeStatus>{TakeStatus::NotNamed});

    QList<WardListEntry> entries = WardList::read(laptop.identity(), laptop.store());

    QCOMPARE(entries.size(), 3);
    WardListEntry unsentEntry = entryOf(entries, unsent);
    WardListEntry sentEntry = entryOf(entries, sent);
    WardListEntry goneEntry = entryOf(entries, gone);
    QVERIFY(unsentEntry.status == OpenStatus::Opened);
    QVERIFY(sentEntry.status == OpenStatus::Opened);
    QVERIFY(goneEntry.status == OpenStatus::Opened);
    QVERIFY(unsentEntry.changesToShare);
    QVERIFY(!sentEntry.changesToShare);
    QVERIFY(!goneEntry.changesToShare);
    QVERIFY(!unsentEntry.noLongerShared);
    QVERIFY(!sentEntry.noLongerShared);
    QVERIFY(goneEntry.noLongerShared);
}
