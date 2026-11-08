#include "WardSessionSavingTest.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QSignalSpy>
#include <QTest>

namespace
{

const Coordinates ElmPosition(40.2338, -111.6585);

QString smithAddress()
{
    return TestWard::smith().address().full();
}

QString jonesAddress()
{
    return TestWard::jones().address().full();
}

CommandPtr phoneEdit(const Document& document, const PersonId& person, const QString& phone)
{
    Family opened = TestWard::family(document, document.familyIdForPerson(person).value());
    return UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, person, phone), document);
}

CommitResult commitPhone(WardSession& session, const PersonId& person, const QString& phone)
{
    return session.commit(phoneEdit(session.document(), person, phone));
}

QString phoneOf(const Document& document, const PersonId& person)
{
    return TestWard::person(document, person).phone();
}

/// The ward as another of its devices leaves it after setting the person's phone: the same
/// ward, access list included, with that change on top.
Document changedElsewhere(const Document& document, const PersonId& person, const QString& phone)
{
    Document version = document;
    phoneEdit(document, person, phone)->execute(version);
    return version;
}

/// The client's local file for its open ward, read as its own device reads it.
WardRead storedWard(TestClient& client)
{
    return WardFile::read(client.store().file(client.session().documentId()), client.identity(), PayloadKind::LocalFile);
}

}  // namespace

WardSessionSavingTest::WardSessionSavingTest(QObject* parent)
    : QObject(parent)
{
}

void WardSessionSavingTest::createdWardIsSavedSealedToThisDeviceAlone()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());

    QCOMPARE(client.store().writes(), 1);
    WardRead read = storedWard(client);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.header->document() == client.session().documentId());
    Document expected = TestWard::document();
    expected.setAccess(read.payload->ward.access());
    QVERIFY(read.payload->ward == expected);
    QCOMPARE(read.header->recipients().size(), qsizetype(1));
    QCOMPARE(read.header->recipients()[0].fingerprint, client.identity().fingerprint());
    const QList<AccessUser>& users = read.payload->ward.access().users();
    QCOMPARE(users.size(), qsizetype(1));
    QCOMPARE(users[0].name(), QString("Alice Smith"));
    QVERIFY(users[0].role() == AccessRole::Admin);
    QCOMPARE(users[0].devices().size(), qsizetype(1));
    QCOMPARE(users[0].devices()[0].name(), QString("Laptop"));
    QCOMPARE(users[0].devices()[0].publicKey(), client.identity().publicKey());
    QCOMPARE(users[0].devices()[0].verifyKey(), client.identity().verifyKey());
    QVERIFY(read.payload->ward.access().invites().isEmpty());
    QVERIFY(read.payload->ward.access().formerKeys().isEmpty());
    QVERIFY(read.payload->landed.isEmpty());
    QVERIFY(read.payload->device.value() == DeviceState());
    QVERIFY(read.payload->ward == client.session().document());

    Identity stranger = Identity::generate();
    QVERIFY(WardFile::read(client.store().file(client.session().documentId()), stranger, PayloadKind::LocalFile).status
            == WardReadStatus::NotNamed);
}

void WardSessionSavingTest::everyCommitSavesTheDocumentAndJournalInOneWrite()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());

    QVERIFY(commitPhone(client.session(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);

    QCOMPARE(client.store().writes(), 2);
    WardRead read = storedWard(client);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QCOMPARE(phoneOf(read.payload->ward, TestWard::bobId()), QString("555-2222"));
    QVERIFY(read.payload->ward == client.session().document());
    QCOMPARE(read.payload->device->journal.entries().size(), qsizetype(1));
    QVERIFY(read.payload->device->journal == client.session().journal());
}

void WardSessionSavingTest::undoRedoAndReplayEachSave()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    Document theirs = changedElsewhere(mine.session().document(), TestWard::bobId(), "555-9999");
    commitPhone(mine.session(), TestWard::bobId(), "555-2222");

    QVERIFY(mine.session().undo().status == CommitStatus::Applied);
    QCOMPARE(mine.store().writes(), 3);
    QCOMPARE(phoneOf(storedWard(mine).payload->ward, TestWard::bobId()), QString("555-1111"));

    QVERIFY(mine.session().redo().status == CommitStatus::Applied);
    QCOMPARE(mine.store().writes(), 4);
    QCOMPARE(storedWard(mine).payload->device->journal.entries().size(), qsizetype(3));

    QVERIFY(mine.session().replayOnto(theirs).status == ReplayStatus::Abandoned);
    QCOMPARE(mine.store().writes(), 5);
    WardRead abandoned = storedWard(mine);
    QCOMPARE(phoneOf(abandoned.payload->ward, TestWard::bobId()), QString("555-9999"));
    QVERIFY(abandoned.payload->device->journal.isEmpty());
}

void WardSessionSavingTest::automaticBatchSavesOnceWithTheFailedListAndItSurvivesReopening()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.session().automaticUpdates().run();
    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress(), jonesAddress()}));

    client.geocoder().fail(jonesAddress());
    QCOMPARE(client.store().writes(), 1);
    client.geocoder().resolve(smithAddress(), ElmPosition);

    QCOMPARE(client.store().writes(), 2);
    WardRead read = storedWard(client);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.payload->ward.coordinatesFor(TestWard::smith().address()) == ElmPosition);
    QCOMPARE(read.payload->device->failed, QSet<QString>({jonesAddress()}));

    QVERIFY(client.reopen().status == OpenStatus::Opened);
    client.session().automaticUpdates().run();

    QCOMPARE(client.geocoder().requests(), QStringList({smithAddress(), jonesAddress()}));
    QCOMPARE(client.store().writes(), 2);
}

void WardSessionSavingTest::saveThatFailsKeepsTheChangeInMemoryAndTheFileAsItWas()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    DocumentId document = client.session().documentId();
    QByteArray before = client.store().file(document);
    QSignalSpy failures(&client.session(), &WardSession::saveFailed);
    client.store().setFailWrites(true);

    QVERIFY(commitPhone(client.session(), TestWard::bobId(), "555-2222").status == CommitStatus::Applied);
    QVERIFY(commitPhone(client.session(), TestWard::carolId(), "555-5555").status == CommitStatus::Applied);

    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(client.session().document(), TestWard::carolId()), QString("555-5555"));
    QCOMPARE(client.session().journal().entries().size(), qsizetype(2));
    QVERIFY(client.session().savePending());
    QCOMPARE(client.store().file(document), before);
    QCOMPARE(client.store().writes(), 1);
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures[0][0].toString(), QString("the disk is full"));
}

void WardSessionSavingTest::retryingAPendingSaveWritesEverythingKept()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    QSignalSpy failures(&client.session(), &WardSession::saveFailed);
    QVERIFY(client.session().retryPendingSave());
    QCOMPARE(client.store().writes(), 1);
    client.store().setFailWrites(true);
    commitPhone(client.session(), TestWard::bobId(), "555-2222");
    commitPhone(client.session(), TestWard::carolId(), "555-5555");
    QVERIFY(!client.session().retryPendingSave());
    client.store().setFailWrites(false);

    QVERIFY(client.session().retryPendingSave());

    QVERIFY(!client.session().savePending());
    QCOMPARE(client.store().writes(), 2);
    WardRead read = storedWard(client);
    QVERIFY(read.payload->ward == client.session().document());
    QCOMPARE(read.payload->device->journal.entries().size(), qsizetype(2));

    client.store().setFailWrites(true);
    commitPhone(client.session(), TestWard::bobId(), "555-3333");
    QCOMPARE(failures.count(), 2);
}

void WardSessionSavingTest::nextCommandSavesWhatAFailedSaveKept()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    client.store().setFailWrites(true);
    commitPhone(client.session(), TestWard::bobId(), "555-2222");
    client.store().setFailWrites(false);

    QVERIFY(commitPhone(client.session(), TestWard::carolId(), "555-5555").status == CommitStatus::Applied);

    QVERIFY(!client.session().savePending());
    WardRead read = storedWard(client);
    QCOMPARE(phoneOf(read.payload->ward, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(read.payload->ward, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(read.payload->device->journal.entries().size(), qsizetype(2));
}

void WardSessionSavingTest::reopenedWardHasItsJournalAndAnEmptyUndoStack()
{
    TestClient mine("Laptop");
    mine.createWard(TestWard::document());
    Document theirs = changedElsewhere(mine.session().document(), TestWard::bobId(), "555-9999");
    commitPhone(mine.session(), TestWard::bobId(), "555-2222");
    QVERIFY(mine.session().replayOnto(theirs).status == ReplayStatus::Abandoned);
    commitPhone(mine.session(), TestWard::carolId(), "555-5555");
    Document document = mine.session().document();
    Journal journal = mine.session().journal();
    QVERIFY(mine.session().canUndo());

    OpenResult reopened = mine.reopen();

    QVERIFY2(reopened.status == OpenStatus::Opened, qPrintable(reopened.problem));
    QVERIFY(mine.session().document() == document);
    QVERIFY(mine.session().journal() == journal);
    QCOMPARE(mine.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(mine.session().journal().entries()[0].sequence, qint64(2));
    QVERIFY(!mine.session().canUndo());

    commitPhone(mine.session(), TestWard::carolId(), "555-6666");
    QCOMPARE(mine.session().journal().entries().last().sequence, qint64(3));
}

void WardSessionSavingTest::readFaultChangesNothingAndOpeningAgainRetries()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    commitPhone(client.session(), TestWard::bobId(), "555-2222");
    DocumentId document = client.session().documentId();
    QByteArray stored = client.store().file(document);
    int writes = client.store().writes();
    client.store().setFailReads(true);

    OpenResult result = client.reopen();

    QVERIFY(result.status == OpenStatus::Fault);
    QCOMPARE(result.problem, QString("the disk did not answer"));
    QCOMPARE(client.store().file(document), stored);
    QCOMPARE(client.store().writes(), writes);

    client.store().setFailReads(false);
    QVERIFY(client.session().openWard(document).status == OpenStatus::Opened);
    QCOMPARE(phoneOf(client.session().document(), TestWard::bobId()), QString("555-2222"));
}

void WardSessionSavingTest::fileThatDoesNotOpenCannotBeReadAndIsNotOverwritten()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    QVERIFY(client.reopen().status == OpenStatus::Opened);
    DocumentId document = client.session().documentId();
    QByteArray damaged = client.store().file(document);
    damaged[damaged.size() - 1] = char(damaged[damaged.size() - 1] ^ 0x01);
    client.store().put(document, damaged);
    int writes = client.store().writes();

    OpenResult result = client.reopen();

    QVERIFY(result.status == OpenStatus::CannotRead);
    QCOMPARE(result.problem, QString("the payload does not decrypt"));
    QCOMPARE(client.store().file(document), damaged);
    QCOMPARE(client.store().writes(), writes);
}

void WardSessionSavingTest::fileWhoseHeaderIdAndNameAreChangedToAnotherWardFailsToDecrypt()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    DocumentId first = client.session().documentId();
    client.createWard(Document::empty());
    DocumentId second = client.session().documentId();
    QVERIFY(first != second);
    QVERIFY(client.session().openWard(first).status == OpenStatus::Opened);

    QByteArray bytes = client.store().file(first);
    QJsonObject header = WardFileEdits::headerOf(bytes);
    header["document"] = second.toString();
    QByteArray tampered = WardFileEdits::withHeader(bytes, header);
    client.store().put(second, tampered);

    OpenResult result = client.session().openWard(second);

    QVERIFY(result.status == OpenStatus::CannotRead);
    QCOMPARE(result.problem, QString("the payload does not decrypt"));
    QVERIFY(client.session().documentId() == first);
    QCOMPARE(client.store().file(second), tampered);
}

void WardSessionSavingTest::fileRenamedToAnotherWardIsRefused()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    DocumentId first = client.session().documentId();
    client.createWard(Document::empty());
    DocumentId second = client.session().documentId();
    client.store().put(second, client.store().file(first));

    OpenResult result = client.session().openWard(second);

    QVERIFY(result.status == OpenStatus::CannotRead);
    QCOMPARE(result.problem, QString("the file holds another ward"));
    QVERIFY(client.session().openWard(first).status == OpenStatus::Opened);
}

void WardSessionSavingTest::fileSealedToAnotherDeviceCannotBeRead()
{
    TestClient alice("Laptop");
    alice.createWard(TestWard::document());
    TestClient bob("Desktop");
    DocumentId document = alice.session().documentId();
    bob.store().put(document, alice.store().file(document));

    OpenResult result = bob.session().openWard(document);

    QVERIFY(result.status == OpenStatus::CannotRead);
    QCOMPARE(result.problem, QString("the header does not name this device"));
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
}

void WardSessionSavingTest::fileFromANewerAppIsReportedAsNewer()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    DocumentId document = client.session().documentId();
    QByteArray newer("{\"format\":2}\nwhatever a newer app writes");
    client.store().put(document, newer);

    OpenResult result = client.reopen();

    QVERIFY(result.status == OpenStatus::Newer);
    QCOMPARE(client.store().file(document), newer);
}

void WardSessionSavingTest::wardWithNoFileIsMissing()
{
    TestClient client("Laptop");
    client.createWard(TestWard::document());
    DocumentId open = client.session().documentId();
    Document document = client.session().document();

    OpenResult result = client.session().openWard(DocumentId::generate());

    QVERIFY(result.status == OpenStatus::Missing);
    QVERIFY(client.session().documentId() == open);
    QVERIFY(client.session().document() == document);
}
