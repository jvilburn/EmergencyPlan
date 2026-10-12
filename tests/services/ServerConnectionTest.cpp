#include "ServerConnectionTest.h"

#include "DocumentCrypto.h"
#include "FakeServer.h"
#include "ServerConnection.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QtTest>

using namespace ServerContract;
using namespace ServerTestSupport;
using SessionTestSupport::asTestDevice;
using SessionTestSupport::newIdentity;
using namespace std::placeholders;

namespace
{

void countReply(int& replies, ServerError& last, const Result<Head>& result)
{
    ++replies;
    last = result.error;
}

/// Lets every reply arrive, held or not, until nothing is waiting.
void settle(FakeServer& server)
{
    int quietRounds = 0;
    while (quietRounds < 2)
    {
        QCoreApplication::processEvents();
        bool delivered = false;
        while (server.deliverNext())
        {
            delivered = true;
        }
        quietRounds = delivered ? 0 : quietRounds + 1;
    }
}

Result<qint64> registerThrough(ServerConnection& connection, const Upload& upload, QObject* context)
{
    return wait<qint64>(std::bind(&ServerConnection::registerDocument, &connection, upload, context, _1));
}

Result<qint64> uploadThrough(ServerConnection& connection,
                             const QString& documentId,
                             qint64 baseVersion,
                             const Upload& upload,
                             QObject* context)
{
    return wait<qint64>(std::bind(&ServerConnection::upload, &connection, documentId, baseVersion, upload, context, _1));
}

} // namespace

void ServerConnectionTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void ServerConnectionTest::writesAreSignedByThisDevice()
{
    FakeServer server;
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    ServerConnection connection(server, alice);
    QObject context;
    const QString id = newDocumentId();

    const Result<qint64> registered = registerThrough(connection, uploadFor(id, {asTestDevice(alice)}, 1), &context);
    QVERIFY(registered.ok());
    QCOMPARE(registered.value.value(), 1);

    const Result<qint64> uploaded = uploadThrough(connection, id, 1, uploadFor(id, {asTestDevice(alice)}, 2), &context);
    QVERIFY(uploaded.ok());
    QCOMPARE(uploaded.value.value(), 2);

    const Result<Lease> lease = wait<Lease>(std::bind(&ServerConnection::takeLease, &connection, id, &context, _1));
    QVERIFY(lease.ok());
    QCOMPARE(lease.value->holder, alice.fingerprint());

    const Result<Head> head = wait<Head>(std::bind(&ServerConnection::head, &connection, id, &context, _1));
    QCOMPARE(head.value->version, 2);
    QCOMPARE(head.value->published.device, alice.fingerprint());
    QVERIFY(head.value->leaseHolder == alice.fingerprint());

    QVERIFY(wait<Done>(std::bind(&ServerConnection::releaseLease, &connection, id, &context, _1)).ok());
    QVERIFY(!headOf(server, id).value->leaseHolder.has_value());
}

void ServerConnectionTest::aDeviceSigningWithAnotherKeyIsRefused()
{
    FakeServer server;

    // A device whose signing key is not the one its verify key names: every write
    // it signs fails to verify.
    DeviceIdentity broken = newIdentity("Mallory", "Laptop");
    broken.signing.signingKey = DocumentCrypto::generateSigningKeypair().signingKey;
    ServerConnection connection(server, broken);
    QObject context;
    const QString id = newDocumentId();

    QCOMPARE(registerThrough(connection, uploadFor(id, {asTestDevice(broken)}, 1), &context).error,
             ServerError::InvalidRequest);
    QCOMPARE(headOf(server, id).error, ServerError::UnknownDocument);

    // Anchor: the same identity with its own signing key is accepted.
    const DeviceIdentity sound = newIdentity("Alice", "Laptop");
    ServerConnection soundConnection(server, sound);
    QVERIFY(registerThrough(soundConnection, uploadFor(id, {asTestDevice(sound)}, 1), &context).ok());
}

void ServerConnectionTest::uploadAsSignsAsTheWriterGiven()
{
    FakeServer server;
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    const TestDevice invite = newDevice("Invite");
    const TestDevice stranger = newDevice("Stranger");
    ServerConnection connection(server, alice);
    QObject context;
    const QString id = newDocumentId();
    QVERIFY(registerThrough(connection, uploadFor(id, {asTestDevice(alice), invite}, 1), &context).ok());

    // Signed as the invite, which the header names, though this connection is Alice's.
    const ServerConnection::Writer asInvite{invite.keys(), invite.signing.signingKey};
    const Result<qint64> joined = wait<qint64>(std::bind(&ServerConnection::uploadAs,
                                                         &connection,
                                                         asInvite,
                                                         id,
                                                         1,
                                                         uploadFor(id, {asTestDevice(alice)}, 2),
                                                         &context,
                                                         _1));
    QVERIFY(joined.ok());
    QCOMPARE(joined.value.value(), 2);
    QCOMPARE(headOf(server, id).value->published.device, invite.fingerprint());

    // The invite is gone from the header it wrote, so it cannot write again; nor can a
    // writer the header never named.
    const ServerConnection::Writer asStranger{stranger.keys(), stranger.signing.signingKey};
    for (const ServerConnection::Writer& writer : {asInvite, asStranger})
    {
        const Result<qint64> refused = wait<qint64>(std::bind(&ServerConnection::uploadAs,
                                                              &connection,
                                                              writer,
                                                              id,
                                                              2,
                                                              uploadFor(id, {asTestDevice(alice)}, 3),
                                                              &context,
                                                              _1));
        QCOMPARE(refused.error, ServerError::NotAKeyHolder);
    }
    QCOMPARE(headOf(server, id).value->version, 2);
    QVERIFY(connection.self().keys == alice.keys());
}

void ServerConnectionTest::aReplyWhoseContextWasDestroyedIsDropped()
{
    FakeServer server;
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    ServerConnection connection(server, alice);
    QObject survivor;
    const QString id = newDocumentId();
    QVERIFY(registerThrough(connection, uploadFor(id, {asTestDevice(alice)}, 1), &survivor).ok());

    server.holdReplies(true);

    int droppedReplies = 0;
    ServerError droppedError = ServerError::None;
    QObject* doomed = new QObject;
    connection.head(id, doomed, std::bind(&countReply, std::ref(droppedReplies), std::ref(droppedError), _1));

    int keptReplies = 0;
    ServerError keptError = ServerError::Unreachable;
    connection.head(id, &survivor, std::bind(&countReply, std::ref(keptReplies), std::ref(keptError), _1));

    QCOMPARE(server.waitingReplies(), qsizetype(2));
    delete doomed;

    server.holdReplies(false);
    settle(server);

    QCOMPARE(droppedReplies, 0);
    QCOMPARE(keptReplies, 1);
    QCOMPARE(keptError, ServerError::None);
}
