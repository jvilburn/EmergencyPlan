#include "ConformanceTest.h"

#include "AccessList.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "FakeServer.h"
#include "Identity.h"
#include "SignedLine.h"
#include "TestWard.h"
#include "WardFile.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
// The statuses server/README.md gives.
constexpr int Created = 201;
constexpr int Stored = 200;
constexpr int Malformed = 400;
constexpr int Forbidden = 403;
constexpr int Conflict = 409;

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

QString pathOf(const DocumentId& document)
{
    return "/documents/" + document.toString();
}
}  // namespace

StatusRecorder::StatusRecorder(Transport* transport, QObject* parent)
    : QObject(parent)
    , m_transport(transport)
{
}

TransportCall* StatusRecorder::send(const TransportRequest& request, QObject* parent)
{
    TransportCall* call = m_transport->send(request, parent);
    connect(call, &TransportCall::answered, this, &StatusRecorder::onAnswered);
    return call;
}

TransportChannel* StatusRecorder::open(const TransportRequest& request, QObject* parent)
{
    return m_transport->open(request, parent);
}

void StatusRecorder::onAnswered(const TransportReply& reply)
{
    m_lastStatus = reply.status;
}

ConformanceTest::ConformanceTest(Transport* transport, FakeServer* fake, QObject* parent)
    : QObject(parent)
    , m_recorder(transport, nullptr)
    , m_server(&m_recorder)
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

bool ConformanceTest::toldAtLast(QSignalSpy& told, const QSet<QByteArray>& present, qint64 number)
{
    if (m_fake != nullptr)
    {
        m_fake->settle();
    }
    while (told.isEmpty()
           || told.last()[0].value<QSet<QByteArray>>() != present
           || told.last()[1].toLongLong() != number)
    {
        if (m_fake != nullptr || !told.wait(WaitMilliseconds))
        {
            return false;
        }
    }
    return true;
}

int ConformanceTest::statusOf(ServerWrite* write)
{
    QSignalSpy spy(write, &ServerWrite::answered);
    return heard(spy, 1) ? m_recorder.lastStatus() : TransportReply::NoAnswer;
}

int ConformanceTest::statusOf(const TransportRequest& request)
{
    TransportCall* call = m_recorder.send(request, this);
    QSignalSpy spy(call, &TransportCall::answered);
    return heard(spy, 1) ? m_recorder.lastStatus() : TransportReply::NoAnswer;
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

QList<QPair<QByteArray, QByteArray>> ConformanceTest::signedBy(const Identity& signer, const QByteArray& line)
{
    QByteArray keys = Base64Url::encode(signer.publicKey()).toLatin1() + '.'
        + Base64Url::encode(signer.verifyKey()).toLatin1();
    return {{"Ward-Signer", keys}, {"Ward-Signature", Base64Url::encode(signer.sign(line)).toLatin1()}};
}

void ConformanceTest::createAndRead()
{
    Identity alice = Identity::generate();
    Identity bob = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QByteArray second = version(document, 2, alice, {alice}, key);

    QVERIFY(answered(m_server.head(document, this)).status == ServerReadStatus::Missing);
    QVERIFY(answered(m_server.fetch(document, this)).status == ServerReadStatus::Missing);

    // A create is kept only when its signature verifies, its signer is its writer, and its
    // own header names that signer.
    TransportRequest forged{"POST", "/documents",
                            signedBy(alice, SignedLine::ofVersion(SignedCall::Push, document, second)), second};
    QCOMPARE(statusOf(forged), Forbidden);
    QCOMPARE(statusOf(m_server.create(document, version(document, 2, bob, {alice, bob}, key), alice, this)), Forbidden);
    QCOMPARE(statusOf(m_server.create(document, version(document, 2, alice, {bob}, key), alice, this)), Forbidden);
    QVERIFY(answered(m_server.head(document, this)).status == ServerReadStatus::Missing);

    QCOMPARE(statusOf(m_server.create(document, second, alice, this)), Created);
    QCOMPARE(statusOf(m_server.create(document, version(document, 9, alice, {alice}, key), alice, this)), Conflict);

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
    QCOMPARE(statusOf(m_server.create(document, second, alice, this)), Created);

    // A device the current header does not name, though the version it sends names it.
    QByteArray mallorys = version(document, 3, mallory, {alice, bob, mallory}, key);
    QCOMPARE(statusOf(m_server.push(document, mallorys, mallory, this)), Forbidden);
    // A named device signing a version whose writer is another.
    QCOMPARE(statusOf(m_server.push(document, version(document, 3, bob, {alice, bob}, key), alice, this)), Forbidden);
    // A named writer whose signature is over another line.
    QByteArray third = version(document, 3, bob, {alice, bob}, key);
    TransportRequest forged{"PUT", pathOf(document),
                            signedBy(bob, SignedLine::ofVersion(SignedCall::Create, document, third)), third};
    QCOMPARE(statusOf(forged), Forbidden);
    // A version of another ward.
    DocumentId other = DocumentId::generate();
    QCOMPARE(statusOf(m_server.push(document, version(other, 3, alice, {alice, bob}, key), alice, this)), Malformed);
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, second);

    QCOMPARE(statusOf(m_server.push(document, third, bob, this)), Stored);
    // The current number from another named device, and a lower one, are not kept.
    QCOMPARE(statusOf(m_server.push(document, version(document, 3, alice, {alice, bob}, key), alice, this)), Conflict);
    QCOMPARE(statusOf(m_server.push(document, version(document, 1, alice, {alice, bob}, key), alice, this)), Conflict);
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, third);

    // A number two above is.
    QByteArray fifth = version(document, 5, alice, {alice, bob}, key);
    QCOMPARE(statusOf(m_server.push(document, fifth, alice, this)), Stored);
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, fifth);
}

void ConformanceTest::theChannelRemovalAndDeletion()
{
    Identity alice = Identity::generate();
    Identity bob = Identity::generate();
    Identity mallory = Identity::generate();
    DocumentId document = DocumentId::generate();
    QByteArray key = Crypto::randomBytes(Crypto::KeyBytes);
    QCOMPARE(statusOf(m_server.create(document, version(document, 2, alice, {alice, bob}, key), alice, this)), Created);

    // An open by a device the header does not name closes with no message, and so does one by
    // a named device whose signature is over another ward's line.
    ServerChannel* strangers = m_server.openChannel(document, mallory, this);
    QSignalSpy strangersTold(strangers, &ServerChannel::told);
    QSignalSpy strangersClosed(strangers, &ServerChannel::closed);
    QVERIFY(heard(strangersClosed, 1));
    QCOMPARE(strangersTold.size(), qsizetype(0));
    TransportRequest forged{"GET", pathOf(document) + "/channel",
                            signedBy(alice, SignedLine::ofChannel(DocumentId::generate())), QByteArray()};
    TransportChannel* forgers = m_recorder.open(forged, this);
    QSignalSpy forgersTold(forgers, &TransportChannel::messageReceived);
    QSignalSpy forgersClosed(forgers, &TransportChannel::closed);
    QVERIFY(heard(forgersClosed, 1));
    QCOMPARE(forgersTold.size(), qsizetype(0));

    // A named device is told who is here and the version's number.
    ServerChannel* alices = m_server.openChannel(document, alice, this);
    QSignalSpy alicesTold(alices, &ServerChannel::told);
    QSignalSpy alicesClosed(alices, &ServerChannel::closed);
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice}), 2));

    // A second device's open is told to both.
    ServerChannel* bobs = m_server.openChannel(document, bob, this);
    QSignalSpy bobsTold(bobs, &ServerChannel::told);
    QVERIFY(toldAtLast(bobsTold, fingerprintsOf({alice, bob}), 2));
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice, bob}), 2));

    // A stored push is told to both, with its number.
    QCOMPARE(statusOf(m_server.push(document, version(document, 3, alice, {alice, bob}, key), alice, this)), Stored);
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice, bob}), 3));
    QVERIFY(toldAtLast(bobsTold, fingerprintsOf({alice, bob}), 3));

    // A device that closes its channel is no longer here, and is here again when it opens one.
    bobs->close();
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice}), 3));
    ServerChannel* bobsNext = m_server.openChannel(document, bob, this);
    QSignalSpy bobsNextTold(bobsNext, &ServerChannel::told);
    QSignalSpy bobsNextClosed(bobsNext, &ServerChannel::closed);
    QVERIFY(toldAtLast(bobsNextTold, fingerprintsOf({alice, bob}), 3));
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice, bob}), 3));

    // A device's second channel leaves its first open, and its open is told to both, with
    // the device here once.
    int alicesToldBefore = alicesTold.size();
    ServerChannel* alicesNext = m_server.openChannel(document, alice, this);
    QSignalSpy alicesNextTold(alicesNext, &ServerChannel::told);
    QSignalSpy alicesNextClosed(alicesNext, &ServerChannel::closed);
    QVERIFY(toldAtLast(alicesNextTold, fingerprintsOf({alice, bob}), 3));
    QVERIFY(heard(alicesTold, alicesToldBefore + 1));
    QVERIFY(toldAtLast(alicesTold, fingerprintsOf({alice, bob}), 3));
    QCOMPARE(alicesClosed.size(), qsizetype(0));

    // The device closes its first channel and is still here through its second.
    int bobsToldBefore = bobsNextTold.size();
    alices->close();
    QVERIFY(heard(bobsNextTold, bobsToldBefore + 1));
    QVERIFY(toldAtLast(bobsNextTold, fingerprintsOf({alice, bob}), 3));
    QCOMPARE(alicesNextClosed.size(), qsizetype(0));

    // A push that drops a device closes its channel and tells the one that remains, and the
    // dropped device's next push is not kept.
    QByteArray fourth = version(document, 4, alice, {alice}, key);
    QCOMPARE(statusOf(m_server.push(document, fourth, alice, this)), Stored);
    QVERIFY(heard(bobsNextClosed, 1));
    QVERIFY(toldAtLast(alicesNextTold, fingerprintsOf({alice}), 4));
    QCOMPARE(statusOf(m_server.push(document, version(document, 5, bob, {alice, bob}, key), bob, this)), Forbidden);
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, fourth);

    // A final version naming no one deletes the ward: its channels close, and nothing can be
    // written to its id again.
    QByteArray final = version(document, 5, alice, {}, key);
    final.truncate(final.indexOf('\n') + 1);
    QCOMPARE(statusOf(m_server.push(document, final, alice, this)), Stored);
    QVERIFY(heard(alicesNextClosed, 1));
    QCOMPARE(statusOf(m_server.push(document, version(document, 6, alice, {alice}, key), alice, this)), Forbidden);
    QCOMPARE(statusOf(m_server.create(document, version(document, 6, alice, {alice}, key), alice, this)), Conflict);
    QCOMPARE(answered(m_server.fetch(document, this)).bytes, final);

    delete strangers;
    delete forgers;
    delete alices;
    delete bobs;
    delete bobsNext;
    delete alicesNext;
}
