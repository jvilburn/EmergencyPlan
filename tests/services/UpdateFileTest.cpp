#include "UpdateFileTest.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "FamilyCommands.h"
#include "TestClient.h"
#include "TestWard.h"
#include "UpdateFile.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QJsonDocument>
#include <QTest>

namespace
{

/// Everything a session holds and its store keeps for its ward, to show that something
/// changed nothing.
struct Held
{
    WardPayload payload;
    VersionRank version;
    QByteArray file;
    int writes;

    bool operator==(const Held& other) const
    {
        return payload == other.payload
            && version == other.version
            && file == other.file
            && writes == other.writes;
    }
};

Held heldBy(TestClient& client)
{
    return Held{client.session().payload(), client.session().version(),
                client.store().file(client.session().documentId()), client.store().writes()};
}

/// Gives every one of clients the test ward under a fresh document id and data key.
void share(const QList<TestClient*>& clients)
{
    TestClient::holdWard(clients, clients, DocumentId::generate(), Crypto::randomBytes(Crypto::KeyBytes),
                         TestWard::document());
}

/// Commits a phone edit to the member, opened on the session's current family.
CommitStatus commitPhone(TestClient& client, const FamilyId& familyId, const PersonId& personId, const QString& phone)
{
    Family opened = TestWard::family(client.session().document(), familyId);
    return client.session()
        .commit(UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, personId, phone),
                                              client.session().document()))
        .status;
}

QString phoneOf(TestClient& client, const PersonId& id)
{
    return TestWard::person(client.session().document(), id).phone();
}

VersionRank versionBy(qint64 number, const TestClient& writer)
{
    return VersionRank{number, writer.identity().fingerprint()};
}

/// An update file of document carrying ward, with those landed marks, under dataKey, as
/// version number written and signed by writer.
QByteArray updateOf(const TestClient& writer, const DocumentId& document, qint64 number, const QByteArray& dataKey,
                    const Document& ward, const QHash<QByteArray, qint64>& landed)
{
    QByteArray version = WardFile::write(document, versionBy(number, writer), dataKey,
                                         WardPayload{ward, landed, std::nullopt});
    return UpdateFile::write(document, version, writer.identity());
}

/// ward with an access list holding only the users whose devices include client's.
Document onlyWith(const Document& ward, const TestClient& client)
{
    QList<AccessUser> users;
    for (const AccessUser& user : ward.access().users())
    {
        if (user.devices().first().publicKey() == client.identity().publicKey())
        {
            users.append(user);
        }
    }
    Document only = ward;
    only.setAccess(AccessList(users, ward.access().invites(), ward.access().formerKeys()));
    return only;
}

QByteArray versionIn(const QByteArray& update)
{
    return update.mid(update.indexOf('\n') + 1);
}

}  // namespace

UpdateFileTest::UpdateFileTest(QObject* parent)
    : QObject(parent)
{
}

void UpdateFileTest::updateFileIsTheSignedNextVersion()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(alice.session().version() == versionBy(1, alice));

    QByteArray update = alice.session().writeUpdate();

    QByteArray signatureLine = update.left(update.indexOf('\n'));
    QCOMPARE(signatureLine.size(), qsizetype(86));
    QVERIFY(Base64Url::decode(QString::fromLatin1(signatureLine), Crypto::SignatureBytes).has_value());
    QJsonObject header = WardFileEdits::headerOf(versionIn(update));
    QCOMPARE(header["document"].toString(), alice.session().documentId().toString());
    QCOMPARE(header["number"].toInteger(), qint64(2));
    QCOMPARE(header["writer"].toString(), Base64Url::encode(alice.identity().fingerprint()));
    WardRead read = WardFile::read(versionIn(update), bob.identity(), PayloadKind::Version);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.payload->ward == alice.session().document());
    QCOMPARE(read.payload->landed, (QHash<QByteArray, qint64>{{alice.identity().fingerprint(), 1}}));
    QVERIFY(!read.payload->device.has_value());

    // The written version is the one this device holds, kept in its local file; the journal keeps its entry.
    QVERIFY(alice.session().version() == versionBy(2, alice));
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().version() == versionBy(2, alice));
    QCOMPARE(alice.session().payload().landed, (QHash<QByteArray, qint64>{{alice.identity().fingerprint(), 1}}));
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(WardFileEdits::headerOf(alice.store().file(alice.session().documentId()))["number"].toInteger(), qint64(2));
}

void UpdateFileTest::fileNameIsTheWardAndTheDay()
{
    QCOMPARE(UpdateFile::fileName("Tanglewood Ward", QDate(2026, 11, 8)), QString("Tanglewood Ward 2026-11-08.wardupdate"));
}

void UpdateFileTest::changesOnEachSideExchangedBothWaysConverge()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(commitPhone(bob, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QVERIFY(bob.session().version() == versionBy(2, alice));
    QCOMPARE(phoneOf(bob, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(bob, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));

    TakeResult aliceTakes = alice.session().takeUpdate(bob.session().writeUpdate());

    QVERIFY(aliceTakes.status == TakeStatus::Taken);
    QVERIFY(aliceTakes.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(alice.session().version() == versionBy(3, bob));
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(alice.session().document() == bob.session().document());
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(alice, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(alice.session().payload().landed,
             (QHash<QByteArray, qint64>{{alice.identity().fingerprint(), 1}, {bob.identity().fingerprint(), 1}}));

    // Bob's entry leaves his journal once a version he takes carries it.
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    TakeResult bobTakesAgain = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakesAgain.status == TakeStatus::Taken);
    QVERIFY(bobTakesAgain.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(bob.session().journal().isEmpty());
    QVERIFY(bob.session().version() == versionBy(4, alice));
    QVERIFY(bob.session().document() == alice.session().document());
}

void UpdateFileTest::threeDevicesConverge()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    TestClient carol("Desktop");
    share({&alice, &bob, &carol});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(commitPhone(bob, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QVERIFY(commitPhone(carol, TestWard::smithId(), TestWard::aliceId(), "555-7777") == CommitStatus::Applied);

    QByteArray alices = alice.session().writeUpdate();
    QVERIFY(bob.session().takeUpdate(alices).replay.status == ReplayStatus::Replayed);
    QVERIFY(carol.session().takeUpdate(alices).replay.status == ReplayStatus::Replayed);
    QVERIFY(carol.session().takeUpdate(bob.session().writeUpdate()).replay.status == ReplayStatus::Replayed);
    QByteArray carols = carol.session().writeUpdate();
    TakeResult aliceTakes = alice.session().takeUpdate(carols);
    TakeResult bobTakes = bob.session().takeUpdate(carols);

    QVERIFY(aliceTakes.status == TakeStatus::Taken);
    QVERIFY(aliceTakes.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(alice.session().document() == carol.session().document());
    QVERIFY(bob.session().document() == carol.session().document());
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(alice, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(phoneOf(alice, TestWard::aliceId()), QString("555-7777"));
    QVERIFY(alice.session().version() == versionBy(4, carol));
    QVERIFY(bob.session().version() == versionBy(4, carol));
    QVERIFY(carol.session().version() == versionBy(4, carol));
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(bob.session().journal().isEmpty());
    QCOMPARE(carol.session().journal().entries().size(), qsizetype(1));
}

void UpdateFileTest::crossedUpdatesConvergeOnTheHigherWriter()
{
    TestClient first("Laptop");
    TestClient second("Phone");
    share({&first, &second});
    bool firstIsHigher = first.identity().fingerprint() > second.identity().fingerprint();
    TestClient& high = firstIsHigher ? first : second;
    TestClient& low = firstIsHigher ? second : first;
    QVERIFY(commitPhone(high, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(commitPhone(low, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QByteArray highs = high.session().writeUpdate();
    QByteArray lows = low.session().writeUpdate();
    QVERIFY(high.session().version() == versionBy(2, high));
    QVERIFY(low.session().version() == versionBy(2, low));

    // The higher writer sets the lower 2 aside.
    Held highBefore = heldBy(high);
    TakeResult setAside = high.session().takeUpdate(lows);

    QVERIFY(setAside.status == TakeStatus::NotNewer);
    QVERIFY(heldBy(high) == highBefore);
    QCOMPARE(phoneOf(high, TestWard::carolId()), QString("555-4444"));

    // The lower writer takes the higher 2, its own change goes on top, and it sends 3.
    TakeResult lowTakes = low.session().takeUpdate(highs);

    QVERIFY(lowTakes.status == TakeStatus::Taken);
    QVERIFY(lowTakes.replay.status == ReplayStatus::Replayed);
    QVERIFY(low.session().version() == versionBy(2, high));
    QCOMPARE(phoneOf(low, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(low, TestWard::carolId()), QString("555-5555"));

    QByteArray lows3 = low.session().writeUpdate();
    QVERIFY(low.session().version() == versionBy(3, low));
    TakeResult highTakes = high.session().takeUpdate(lows3);

    QVERIFY(highTakes.status == TakeStatus::Taken);
    QVERIFY(highTakes.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(high.session().journal().isEmpty());
    QVERIFY(high.session().version() == versionBy(3, low));
    QVERIFY(high.session().document() == low.session().document());
    QCOMPARE(phoneOf(high, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(high, TestWard::carolId()), QString("555-5555"));
}

void UpdateFileTest::updateNotAboveTheHeldVersionChangesNothing()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QByteArray second = alice.session().writeUpdate();
    QVERIFY(commitPhone(alice, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QByteArray third = alice.session().writeUpdate();
    QVERIFY(commitPhone(bob, TestWard::smithId(), TestWard::aliceId(), "555-7777") == CommitStatus::Applied);
    QVERIFY(bob.session().takeUpdate(third).status == TakeStatus::Taken);
    Held before = heldBy(bob);

    TakeResult older = bob.session().takeUpdate(second);
    TakeResult again = bob.session().takeUpdate(third);

    QVERIFY(older.status == TakeStatus::NotNewer);
    QVERIFY(again.status == TakeStatus::NotNewer);
    QVERIFY(heldBy(bob) == before);
    QVERIFY(bob.session().version() == versionBy(3, alice));
    QCOMPARE(phoneOf(bob, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(phoneOf(bob, TestWard::aliceId()), QString("555-7777"));
}

void UpdateFileTest::updateFromADeviceNotOnTheAccessListIsRefused()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    TestClient mallory("Tablet");
    DocumentId document = DocumentId::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    TestClient::holdWard({&alice, &bob}, {&alice, &bob}, document, dataKey, TestWard::document());
    TestClient::holdWard({&mallory}, {&alice, &bob, &mallory}, document, dataKey, TestWard::document());
    QVERIFY(commitPhone(mallory, TestWard::smithId(), TestWard::bobId(), "555-6666") == CommitStatus::Applied);
    QByteArray mallorys = mallory.session().writeUpdate();
    QVERIFY(WardFile::read(versionIn(mallorys), alice.identity(), PayloadKind::Version).status == WardReadStatus::Read);
    Held before = heldBy(alice);

    TakeResult result = alice.session().takeUpdate(mallorys);

    QVERIFY(result.status == TakeStatus::UnknownWriter);
    QVERIFY(heldBy(alice) == before);
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-1111"));
}

void UpdateFileTest::updateWhoseSignatureDoesNotVerifyIsRefused()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QByteArray update = alice.session().writeUpdate();
    // Alice's version signed by Bob, whom the access list names, rather than by Alice, its writer.
    QByteArray resigned = UpdateFile::write(alice.session().documentId(), versionIn(update), bob.identity());
    // Alice's signature over a version whose last byte changed.
    QByteArray changed = update;
    changed[changed.size() - 1] = static_cast<char>(changed[changed.size() - 1] ^ 1);
    Held before = heldBy(bob);

    const QList<QByteArray> refused = {resigned, changed};
    for (const QByteArray& file : refused)
    {
        TakeResult result = bob.session().takeUpdate(file);

        QVERIFY(result.status == TakeStatus::Unreadable);
        QCOMPARE(result.problem, QString("the signature does not verify"));
        QVERIFY(heldBy(bob) == before);
    }
    QVERIFY(bob.session().takeUpdate(update).status == TakeStatus::Taken);
    QCOMPARE(phoneOf(bob, TestWard::bobId()), QString("555-2222"));
}

void UpdateFileTest::conflictDuringTheReplayIsHandedBack()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(bob, TestWard::smithId(), TestWard::bobId(), "555-9999") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QVERIFY(commitPhone(alice, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(alice.session().canUndo());

    TakeResult result = alice.session().takeUpdate(bobs);

    QVERIFY(result.status == TakeStatus::Taken);
    QVERIFY(result.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(result.replay.toRedo,
             QString("Your changes could not be applied, because the ward changed first.\n"
                     "These need doing again:\n"
                     "\n"
                     "Jones family\n"
                     "- Carol, phone: 555-5555\n"
                     "\n"
                     "Smith family (stopped here)\n"
                     "- Bob, phone: 555-2222\n"));
    QVERIFY(alice.session().document() == bob.session().document());
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-9999"));
    QCOMPARE(phoneOf(alice, TestWard::carolId()), QString("555-4444"));
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(alice.session().journal().lastSequence(), qint64(2));
    QVERIFY(!alice.session().canUndo());
    QVERIFY(alice.session().version() == versionBy(2, bob));
}

void UpdateFileTest::landedMarkDropsWhatLandedAndKeepsWhatCameAfter()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    share({&alice, &bob});
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(commitPhone(alice, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QVERIFY(bob.session().takeUpdate(alice.session().writeUpdate()).status == TakeStatus::Taken);
    // Made after Alice sent, so in no version Bob holds.
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::aliceId(), "555-7777") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(WardFile::read(versionIn(bobs), alice.identity(), PayloadKind::Version).payload->landed,
             (QHash<QByteArray, qint64>{{alice.identity().fingerprint(), 2}, {bob.identity().fingerprint(), 0}}));

    TakeResult result = alice.session().takeUpdate(bobs);

    QVERIFY(result.status == TakeStatus::Taken);
    QVERIFY(result.replay.status == ReplayStatus::Replayed);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(alice.session().journal().entries()[0].sequence, qint64(3));
    QCOMPARE(alice.session().journal().lastSequence(), qint64(3));
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(alice, TestWard::carolId()), QString("555-5555"));
    QCOMPARE(phoneOf(alice, TestWard::aliceId()), QString("555-7777"));
}

void UpdateFileTest::takingAVersionRaisesTheCounterToThisDevicesMark()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    DocumentId document = DocumentId::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    TestClient::holdWard({&alice, &bob}, {&alice, &bob}, document, dataKey, TestWard::document());
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::bobId(), "555-2222") == CommitStatus::Applied);
    QVERIFY(commitPhone(alice, TestWard::jonesId(), TestWard::carolId(), "555-5555") == CommitStatus::Applied);
    QVERIFY(commitPhone(alice, TestWard::smithId(), TestWard::aliceId(), "555-7777") == CommitStatus::Applied);
    QVERIFY(bob.session().takeUpdate(alice.session().writeUpdate()).status == TakeStatus::Taken);
    QByteArray bobs = bob.session().writeUpdate();
    // Alice starts over from a fresh copy of the ward, whose journal numbers from 0.
    TestClient::holdWard({&alice}, {&alice, &bob}, document, dataKey, TestWard::document());
    QCOMPARE(alice.session().journal().lastSequence(), qint64(0));

    TakeResult result = alice.session().takeUpdate(bobs);

    QVERIFY(result.status == TakeStatus::Taken);
    QVERIFY(result.replay.status == ReplayStatus::FastForwarded);
    QCOMPARE(alice.session().journal().lastSequence(), qint64(3));
    QCOMPARE(phoneOf(alice, TestWard::aliceId()), QString("555-7777"));
    QVERIFY(commitPhone(alice, TestWard::jonesId(), TestWard::carolId(), "555-6666") == CommitStatus::Applied);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(alice.session().journal().entries()[0].sequence, qint64(4));
}

void UpdateFileTest::writtenVersionDropsTheMarksOfDevicesNoLongerListed()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    DocumentId document = DocumentId::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    TestClient::holdWard({&alice, &bob}, {&alice, &bob}, document, dataKey, TestWard::document());
    QByteArray gone = Identity::generate().fingerprint();
    QHash<QByteArray, qint64> landed = {{bob.identity().fingerprint(), 4}, {gone, 7}};
    QVERIFY(alice.session().takeUpdate(updateOf(bob, document, 2, dataKey, bob.session().document(), landed)).status
            == TakeStatus::Taken);
    QCOMPARE(alice.session().payload().landed, landed);

    QByteArray alices = alice.session().writeUpdate();

    QHash<QByteArray, qint64> written = {{bob.identity().fingerprint(), 4}, {alice.identity().fingerprint(), 0}};
    QCOMPARE(WardFile::read(versionIn(alices), bob.identity(), PayloadKind::Version).payload->landed, written);
    QCOMPARE(alice.session().payload().landed, written);
}

void UpdateFileTest::versionUnderAnotherKeyMustShareAKeyWithTheHeldOne()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    DocumentId document = DocumentId::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    TestClient::holdWard({&alice, &bob}, {&alice, &bob}, document, dataKey, TestWard::document());
    QByteArray newKey = Crypto::randomBytes(Crypto::KeyBytes);
    Document changed = TestWard::withFamily(bob.session().document(),
                                            TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-8888"));
    const AccessList& access = changed.access();
    Document descended = changed;
    descended.setAccess(AccessList(access.users(), access.invites(),
                                   {Crypto::hash(dataKey, Crypto::HashBytes, "wp-former-key")}));
    Document unrelated = changed;
    unrelated.setAccess(AccessList(access.users(), access.invites(),
                                   {Crypto::hash(Crypto::randomBytes(Crypto::KeyBytes), Crypto::HashBytes, "wp-former-key")}));
    Held before = heldBy(alice);

    TakeResult refused = alice.session().takeUpdate(updateOf(bob, document, 2, newKey, unrelated, {}));

    QVERIFY(refused.status == TakeStatus::Unreadable);
    QCOMPARE(refused.problem, QString("the version does not descend from the one this device holds"));
    QVERIFY(heldBy(alice) == before);

    TakeResult taken = alice.session().takeUpdate(updateOf(bob, document, 2, newKey, descended, {}));

    QVERIFY(taken.status == TakeStatus::Taken);
    QCOMPARE(phoneOf(alice, TestWard::bobId()), QString("555-8888"));
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QCOMPARE(WardFile::read(alice.store().file(document), alice.identity(), PayloadKind::LocalFile).dataKey, newKey);

    // Bob, still under the key before, takes Alice's next version, which names it.
    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QCOMPARE(phoneOf(bob, TestWard::bobId()), QString("555-8888"));
}

void UpdateFileTest::versionThatLeavesThisDeviceOutEndsItsShare()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    DocumentId document = DocumentId::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    TestClient::holdWard({&alice, &bob}, {&alice, &bob}, document, dataKey, TestWard::document());
    Document withoutAlice = onlyWith(bob.session().document(), bob);
    QCOMPARE(withoutAlice.access().users().size(), qsizetype(1));
    Held before = heldBy(alice);

    TakeResult result = alice.session().takeUpdate(updateOf(bob, document, 2, dataKey, withoutAlice, {}));

    QVERIFY(result.status == TakeStatus::NotNamed);
    QVERIFY(alice.session().isNoLongerShared());
    QVERIFY(alice.session().version() == before.version);
    QVERIFY(alice.session().document() == before.payload.ward);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().isNoLongerShared());
}

void UpdateFileTest::updateOfAnotherWardOrFormatOrNotAnUpdateChangesNothing()
{
    TestClient alice("Laptop");
    TestClient bob("Phone");
    TestClient carol("Desktop");
    share({&alice, &bob});
    share({&carol, &bob});
    QByteArray carols = carol.session().writeUpdate();
    QByteArray newer = UpdateFile::write(carol.session().documentId(),
                                         WardFileEdits::withHeader(versionIn(carols), QJsonObject{{"format", 2}}),
                                         carol.identity());
    QByteArray wardFile = alice.store().file(alice.session().documentId());
    Held before = heldBy(alice);

    TakeResult otherWard = alice.session().takeUpdate(carols);
    TakeResult newerFormat = alice.session().takeUpdate(newer);
    TakeResult notAnUpdate = alice.session().takeUpdate(wardFile);

    QVERIFY(otherWard.status == TakeStatus::OtherWard);
    QVERIFY(newerFormat.status == TakeStatus::Newer);
    QVERIFY(notAnUpdate.status == TakeStatus::Unreadable);
    QCOMPARE(notAnUpdate.problem, QString("the signature line is not base64url of 64 bytes"));
    QVERIFY(heldBy(alice) == before);
}
