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
using SessionTestSupport::newDeviceOf;
using SessionTestSupport::newIdentity;
using namespace std::placeholders;

namespace
{

Result<Me> meOf(ServerConnection& connection, QObject* context)
{
    return wait<Me>(std::bind(&ServerConnection::me, &connection, context, _1));
}

void countReply(int& replies, ServerError& last, const Result<Me>& result)
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
    const Result<Me> documents = meOf(connection, &context);
    QVERIFY(documents.ok());
    QCOMPARE(server.challengesIssued(), 1);

    // Signed in: the next call needs no challenge.
    QVERIFY(meOf(connection, &context).ok());
    QCOMPARE(server.challengesIssued(), 1);
}

void ServerConnectionTest::anExpiredSessionIsSignedIntoAgain()
{
    FakeServer server;
    ServerConnection connection(server, newIdentity("Alice", "Laptop"));
    QObject context;

    QVERIFY(meOf(connection, &context).ok());
    QCOMPARE(server.challengesIssued(), 1);

    QVERIFY(wait<Done>(std::bind(&ServerApi::advanceClock, &server, FakeServer::Limits().sessionSeconds + 1, _1)).ok());

    QVERIFY(meOf(connection, &context).ok());
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
    connection.me(&context, std::bind(&countReply, std::ref(replies), std::ref(last), _1));
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
        connection.me(&context, std::bind(&countReply, std::ref(replies), std::ref(last), _1));
    }
    settle(server);

    QCOMPARE(replies, 3);
    QCOMPARE(last, ServerError::None);
    QCOMPARE(server.challengesIssued(), 1);
}

void ServerConnectionTest::signingInRegistersThePerson()
{
    FakeServer server;
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    ServerConnection connection(server, alice);
    QObject context;

    const Result<Me> me = meOf(connection, &context);
    QVERIFY(me.ok());
    QCOMPARE(me.value->person.member, alice.member);
    QCOMPARE(me.value->person.displayName, QString("Alice"));
    QCOMPARE(me.value->person.devices.size(), 1);
    QCOMPARE(me.value->person.devices.first().fingerprint, alice.fingerprint());
    QCOMPARE(me.value->person.devices.first().name, QString("Laptop"));

    // A second connection for the same device registers again and changes nothing.
    ServerConnection again(server, alice);
    QCOMPARE(meOf(again, &context).value->person.devices.size(), 1);
}

void ServerConnectionTest::aDeviceWithNoPersonSignsInToLink()
{
    FakeServer server;
    DeviceIdentity unlinked = newIdentity("", "Phone");
    unlinked.member = MemberId();
    ServerConnection connection(server, unlinked);
    QObject context;

    const Result<QString> code = wait<QString>(std::bind(&ServerConnection::startLink, &connection, &context, _1));
    QVERIFY(code.ok());
    QVERIFY(!code.value->isEmpty());

    // Without a person it has nothing to read about itself.
    QCOMPARE(meOf(connection, &context).error, ServerError::InvalidRequest);
}

void ServerConnectionTest::anUnlinkedDeviceHearsItFromEveryCallWithoutSigningInAgain()
{
    FakeServer server;
    const DeviceIdentity laptop = newIdentity("Alice", "Laptop");
    const DeviceIdentity phone = newDeviceOf(laptop, "Phone");
    const std::optional<Session> laptopSession = join(server, asTestDevice(laptop));
    QVERIFY(laptopSession.has_value());
    QVERIFY(link(server, asTestDevice(phone), laptopSession.value()).has_value());
    QVERIFY(wait<Done>(std::bind(&ServerApi::unlinkDevice, &server, laptopSession.value(), phone.fingerprint(), _1)).ok());

    ServerConnection connection(server, phone);
    QList<LostAccess> told;
    connection.setUnlinkedHandler([&told](const LostAccess& lost) { told.append(lost); });
    QObject context;
    const int before = server.challengesIssued();

    const Result<Me> first = meOf(connection, &context);
    QCOMPARE(first.error, ServerError::AccessLost);
    QVERIFY(first.lost.has_value());
    QCOMPARE(first.lost->how, LostAccess::How::Unlinked);
    QCOMPARE(first.lost->byWhom, QString("Laptop"));

    const Result<Head> second =
        wait<Head>(std::bind(&ServerConnection::head, &connection, ServerTestSupport::newDocumentId(), &context, _1));
    QCOMPARE(second.error, ServerError::AccessLost);
    QCOMPARE(server.challengesIssued(), before + 1);

    // The app hears it once, however many calls do.
    QCOMPARE(told.size(), 1);
    QCOMPARE(told.first().how, LostAccess::How::Unlinked);
    QCOMPARE(told.first().byWhom, QString("Laptop"));
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
    connection.me(doomed, std::bind(&countReply, std::ref(droppedReplies), std::ref(droppedError), _1));

    int keptReplies = 0;
    ServerError keptError = ServerError::Unreachable;
    connection.me(&survivor, std::bind(&countReply, std::ref(keptReplies), std::ref(keptError), _1));

    QVERIFY(server.waitingReplies() > 0);
    delete doomed;

    server.holdReplies(false);
    settle(server);

    QCOMPARE(droppedReplies, 0);
    QCOMPARE(keptReplies, 1);
    QCOMPARE(keptError, ServerError::None);
}
