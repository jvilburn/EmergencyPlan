#include "ConformanceTest.h"

#include "AccessList.h"
#include "Crypto.h"
#include "FakeServer.h"
#include "Identity.h"
#include "TestWard.h"
#include "WardFile.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
/// A device as an access-list entry.
AccessDevice deviceOf(const Identity& identity)
{
    return AccessDevice("Device", identity.publicKey(), identity.verifyKey());
}

/// Version number of document, written by writer and sealed to named, each an admin with one
/// device, under key.
QByteArray version(const DocumentId& document, qint64 number, const Identity& writer, const QList<Identity>& named,
                   const QByteArray& key)
{
    QList<AccessUser> users;
    for (const Identity& identity : named)
    {
        users.append(AccessUser(UserId::generate(), {"User", ""}, AccessRole::Admin, {deviceOf(identity)}));
    }
    Document ward = TestWard::document();
    ward.setAccess(AccessList(users, {}, {}));
    return WardFile::write(document, VersionRank{number, writer.fingerprint()}, key,
                           WardPayload{ward, {}, std::nullopt});
}

QByteArray headerLineOf(const QByteArray& version)
{
    return version.left(version.indexOf('\n'));
}

QSet<QByteArray> fingerprintsOf(const QList<Identity>& identities)
{
    QSet<QByteArray> fingerprints;
    for (const Identity& identity : identities)
    {
        fingerprints.insert(identity.fingerprint());
    }
    return fingerprints;
}

QSet<QByteArray> presentIn(const QList<QVariant>& told)
{
    return told[0].value<QSet<QByteArray>>();
}
}  // namespace

ConformanceTest::ConformanceTest(Transport* transport, FakeServer* fake, QObject* parent)
    : QObject(parent)
    , m_server(transport)
    , m_fake(fake)
{
}

bool ConformanceTest::heard(QSignalSpy& spy, int count)
{
    if (m_fake != nullptr)
    {
        m_fake->settle();
    }
    while (spy.size() < count)
    {
        if (m_fake != nullptr || !spy.wait(WaitMilliseconds))
        {
            return false;
        }
    }
    return true;
}

bool ConformanceTest::stored(ServerWrite* write)
{
    QSignalSpy spy(write, &ServerWrite::answered);
    if (!heard(spy, 1))
    {
        qWarning("A write got no answer");
        return false;
    }
    return spy[0][0].toBool();
}

ServerRead ConformanceTest::answered(ServerReading* reading)
{
    QSignalSpy spy(reading, &ServerReading::answered);
    if (!heard(spy, 1))
    {
        return ServerRead{ServerReadStatus::NoAnswer, QByteArray()};
    }
    return spy[0][0].value<ServerRead>();
}

void ConformanceTest::createAndRead()
{
    Identity alice = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QByteArray second = version(document, 2, alice, {alice}, key);

    QVERIFY(answered(m_server.head(document, this)).status == ServerReadStatus::Missing);
    QVERIFY(answered(m_server.fetch(document, this)).status == ServerReadStatus::Missing);

    QVERIFY(stored(m_server.create(document, second, alice, this)));
    QVERIFY(!stored(m_server.create(document, version(document, 9, alice, {alice}, key), alice, this)));

    ServerRead head = answered(m_server.head(document, this));
    QVERIFY(head.status == ServerReadStatus::Found);
    QCOMPARE(head.bytes, headerLineOf(second));
    ServerRead whole = answered(m_server.fetch(document, this));
    QVERIFY(whole.status == ServerReadStatus::Found);
    QCOMPARE(whole.bytes, second);
}

void ConformanceTest::whoMayWriteAndOnlyAHigherNumberIsKept()
{
    Identity alice = Identity::generate();
    Identity bob = Identity::generate();
    Identity mallory = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QByteArray second = version(document, 2, alice, {alice, bob}, key);
    QVERIFY(stored(m_server.create(document, second, alice, this)));

    // A device the current header does not name, though the version it sends names it.
    QVERIFY(!stored(m_server.push(document, version(document, 3, mallory, {alice, bob, mallory}, key), mallory, this)));
    // A named device signing a version whose writer is another.
    QVERIFY(!stored(m_server.push(document, version(document, 3, bob, {alice, bob}, key), alice, this)));
    // A version of another ward.
    DocumentId other = DocumentId::generate();
    QVERIFY(!stored(m_server.push(document, version(other, 3, alice, {alice, bob}, key), alice, this)));
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, second);

    QByteArray third = version(document, 3, bob, {alice, bob}, key);
    QVERIFY(stored(m_server.push(document, third, bob, this)));
    // The current number from another named device, and a lower one, are not kept.
    QVERIFY(!stored(m_server.push(document, version(document, 3, alice, {alice, bob}, key), alice, this)));
    QVERIFY(!stored(m_server.push(document, version(document, 1, alice, {alice, bob}, key), alice, this)));
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, third);

    // A number two above is.
    QByteArray fifth = version(document, 5, alice, {alice, bob}, key);
    QVERIFY(stored(m_server.push(document, fifth, alice, this)));
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, fifth);
}

void ConformanceTest::removalAndDeletion()
{
    Identity alice = Identity::generate();
    Identity bob = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QVERIFY(stored(m_server.create(document, version(document, 2, alice, {alice, bob}, key), alice, this)));

    // Alice's version drops Bob, so his next push is not kept.
    QByteArray third = version(document, 3, alice, {alice}, key);
    QVERIFY(stored(m_server.push(document, third, alice, this)));
    QVERIFY(!stored(m_server.push(document, version(document, 4, bob, {alice, bob}, key), bob, this)));
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, third);

    // A final version naming no one deletes the ward: nothing can be written to its id again.
    QByteArray final = version(document, 4, alice, {}, key);
    final.truncate(final.indexOf('\n') + 1);
    QVERIFY(stored(m_server.push(document, final, alice, this)));
    QVERIFY(!stored(m_server.push(document, version(document, 5, alice, {alice}, key), alice, this)));
    QVERIFY(!stored(m_server.create(document, version(document, 5, alice, {alice}, key), alice, this)));
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, final);
}

void ConformanceTest::theChannel()
{
    Identity alice = Identity::generate();
    Identity bob = Identity::generate();
    Identity mallory = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QVERIFY(stored(m_server.create(document, version(document, 2, alice, {alice, bob}, key), alice, this)));

    // An open by a device the header does not name closes with no message.
    ServerChannel* strangers = m_server.openChannel(document, mallory, this);
    QSignalSpy strangersTold(strangers, &ServerChannel::told);
    QSignalSpy strangersClosed(strangers, &ServerChannel::closed);
    QVERIFY(heard(strangersClosed, 1));
    QCOMPARE(strangersTold.size(), qsizetype(0));

    // A named device is told who is here and the version's number.
    ServerChannel* alices = m_server.openChannel(document, alice, this);
    QSignalSpy alicesTold(alices, &ServerChannel::told);
    QVERIFY(heard(alicesTold, 1));
    QCOMPARE(presentIn(alicesTold[0]), fingerprintsOf({alice}));
    QCOMPARE(alicesTold[0][1].toLongLong(), qint64(2));

    // A second device's open is told to both.
    ServerChannel* bobs = m_server.openChannel(document, bob, this);
    QSignalSpy bobsTold(bobs, &ServerChannel::told);
    QSignalSpy bobsClosed(bobs, &ServerChannel::closed);
    QVERIFY(heard(bobsTold, 1));
    QVERIFY(heard(alicesTold, 2));
    QCOMPARE(presentIn(bobsTold[0]), fingerprintsOf({alice, bob}));
    QCOMPARE(presentIn(alicesTold[1]), fingerprintsOf({alice, bob}));

    // A stored push is told to both, with its number.
    QVERIFY(stored(m_server.push(document, version(document, 3, alice, {alice, bob}, key), alice, this)));
    QVERIFY(heard(alicesTold, 3));
    QVERIFY(heard(bobsTold, 2));
    QCOMPARE(alicesTold[2][1].toLongLong(), qint64(3));
    QCOMPARE(bobsTold[1][1].toLongLong(), qint64(3));
    QCOMPARE(presentIn(bobsTold[1]), fingerprintsOf({alice, bob}));
    QCOMPARE(bobsClosed.size(), qsizetype(0));

    // A push that drops a device closes its channel, and tells the one that remains.
    QVERIFY(stored(m_server.push(document, version(document, 4, alice, {alice}, key), alice, this)));
    QVERIFY(heard(bobsClosed, 1));
    QVERIFY(heard(alicesTold, 4));
    QCOMPARE(presentIn(alicesTold.last()), fingerprintsOf({alice}));
    QCOMPARE(alicesTold.last()[1].toLongLong(), qint64(4));

    alices->close();
    delete alices;
    delete bobs;
    delete strangers;
}
