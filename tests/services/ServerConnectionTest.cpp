#include "ServerConnectionTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "FakeServer.h"
#include "ServerConnection.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QtTest>

using namespace ServerContract;
using namespace ServerTestSupport;
using namespace SessionTestSupport;
using namespace std::placeholders;

namespace
{

Result<QStringList> myDocuments(ServerConnection& connection, QObject* context)
{
    return wait<QStringList>([&](Reply<QStringList> reply) { connection.myDocuments(context, reply); });
}

void countReply(int& replies, ServerError& last, const Result<QStringList>& result)
{
    ++replies;
    last = result.error;
}

/// The first version of a ward of one person, as a device registers it.
Upload firstUpload(const DeviceIdentity& identity, const QString& documentId)
{
    AccessRoster roster;
    roster.addPerson(identity.asNewPerson());
    Document document = Document::empty();
    document.setAccessRoster(roster);

    DocumentHeader first;
    first.documentId = documentId;
    const DocumentFile::Sealed sealed =
        DocumentFile::seal(document, first, DocumentCrypto::generateDataKey(), identity).value();
    return Upload{sealed.header, sealed.ciphertext};
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

} // namespace

void ServerConnectionTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void ServerConnectionTest::theFirstCallSignsInAndSucceeds()
{
    FakeServer server;
    ServerConnection connection(server, newIdentity("Alice", "Laptop"));
    QObject context;

    QCOMPARE(server.challengesIssued(), 0);
    const Result<QStringList> documents = myDocuments(connection, &context);
    QVERIFY(documents.ok());
    QCOMPARE(server.challengesIssued(), 1);

    // Signed in: the next call needs no challenge.
    QVERIFY(myDocuments(connection, &context).ok());
    QCOMPARE(server.challengesIssued(), 1);
}

void ServerConnectionTest::anExpiredSessionIsSignedIntoAgain()
{
    FakeServer server;
    ServerConnection connection(server, newIdentity("Alice", "Laptop"));
    QObject context;

    QVERIFY(myDocuments(connection, &context).ok());
    QCOMPARE(server.challengesIssued(), 1);

    QVERIFY(wait<Done>([&](Reply<Done> reply) { server.advanceClock(FakeServer::Limits().sessionSeconds + 1, reply); })
                .ok());

    QVERIFY(myDocuments(connection, &context).ok());
    QCOMPARE(server.challengesIssued(), 2);
}

void ServerConnectionTest::aRefusedSignInGivesOneUnauthenticatedReplyAndNoRetry()
{
    FakeServer server;

    // A device whose signing key is not the one its verify key names: every login
    // it signs fails to verify.
    DeviceIdentity broken = newIdentity("Mallory", "Laptop");
    broken.signing.signingKey = DocumentCrypto::generateSigningKeypair().signingKey;
    ServerConnection connection(server, broken);
    QObject context;

    int replies = 0;
    ServerError last = ServerError::None;
    connection.myDocuments(&context, std::bind(&countReply, std::ref(replies), std::ref(last), _1));
    settle(server);

    QCOMPARE(replies, 1);
    QCOMPARE(last, ServerError::Unauthenticated);
    QCOMPARE(server.challengesIssued(), 1);
}

void ServerConnectionTest::callsMadeBeforeTheFirstSignInShareOneChallenge()
{
    FakeServer server;
    ServerConnection connection(server, newIdentity("Alice", "Laptop"));
    QObject context;

    int replies = 0;
    ServerError last = ServerError::Unreachable;
    for (int call = 0; call < 3; ++call)
    {
        connection.myDocuments(&context, std::bind(&countReply, std::ref(replies), std::ref(last), _1));
    }
    settle(server);

    QCOMPARE(replies, 3);
    QCOMPARE(last, ServerError::None);
    QCOMPARE(server.challengesIssued(), 1);
}

void ServerConnectionTest::everyMutatingCallReachesTheServerSigned()
{
    // FakeServer refuses any mutating request whose signature requestVerifies
    // does not accept, so each success here is a signature it accepted.
    FakeServer server;
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    ServerConnection connection(server, alice);
    QObject context;

    const QString documentId = ChainTestSupport::newDocumentId();
    const Upload first = firstUpload(alice, documentId);
    const Result<qint64> registered =
        wait<qint64>([&](Reply<qint64> reply) { connection.registerDocument(first, &context, reply); });
    QVERIFY(registered.ok());

    // The next version, sealed on the first under the same key.
    AccessRoster roster;
    roster.addPerson(alice.asNewPerson());
    Document document = Document::empty();
    document.setAccessRoster(roster);
    const std::optional<DataKey> dataKey = DocumentCrypto::unsealDataKey(first.header.recipients.first().sealedKey,
                                                                         alice.sealing);
    QVERIFY(dataKey.has_value());
    const DocumentFile::Sealed second = DocumentFile::seal(document, first.header, dataKey.value(), alice).value();

    const Result<qint64> uploaded = wait<qint64>([&](Reply<qint64> reply)
    {
        connection.upload(documentId, registered.value.value(), Upload{second.header, second.ciphertext}, &context, reply);
    });
    QVERIFY(uploaded.ok());
    QCOMPARE(uploaded.value.value(), qint64(2));

    QVERIFY(wait<Lease>([&](Reply<Lease> reply) { connection.takeLease(documentId, &context, reply); }).ok());
    QVERIFY(wait<Done>([&](Reply<Done> reply) { connection.releaseLease(documentId, &context, reply); }).ok());

    // Anchor: the same request signed by another device's key is refused, so the
    // successes above are not the server accepting anything.
    const Session session = signIn(server, asTestDevice(alice)).value();
    const QByteArray foreign = signRequest(Endpoint::TakeLease,
                                           session,
                                           documentId,
                                           emptyBody(),
                                           DocumentCrypto::generateSigningKeypair().signingKey);
    QCOMPARE(wait<Lease>([&](Reply<Lease> reply) { server.takeLease(session, documentId, foreign, reply); }).error,
             ServerError::Unauthenticated);
}

void ServerConnectionTest::aReplyWhoseContextWasDestroyedIsDropped()
{
    FakeServer server;
    ServerConnection connection(server, newIdentity("Alice", "Laptop"));
    QObject survivor;

    server.holdReplies(true);

    int droppedReplies = 0;
    ServerError droppedError = ServerError::None;
    QObject* doomed = new QObject;
    connection.myDocuments(doomed, std::bind(&countReply, std::ref(droppedReplies), std::ref(droppedError), _1));

    int keptReplies = 0;
    ServerError keptError = ServerError::Unreachable;
    connection.myDocuments(&survivor, std::bind(&countReply, std::ref(keptReplies), std::ref(keptError), _1));

    QVERIFY(server.waitingReplies() > 0);
    delete doomed;

    server.holdReplies(false);
    settle(server);

    QCOMPARE(droppedReplies, 0);
    QCOMPARE(keptReplies, 1);
    QCOMPARE(keptError, ServerError::None);
}
