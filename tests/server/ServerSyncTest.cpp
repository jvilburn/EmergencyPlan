#include "ServerSyncTest.h"

#include "AccessCommands.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "FakeServer.h"
#include "TestClient.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardFile.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
const Coordinates ElmPosition(40.2338, -111.6585);

/// One ward that every client holds as an admin, on the server, with every client sharing it
/// and the server settled: each holds version 2, which the first client created there.
DocumentId shared(FakeServer& server, const QList<TestClient*>& clients)
{
    DocumentId document = TestSharing::share(clients, TestSharing::accessOf(clients, {}));
    for (TestClient* client : clients)
    {
        client->startSync();
    }
    server.settle();
    return document;
}

CommitStatus commitAlicesPhone(TestClient& client, const QString& phone)
{
    return TestSharing::commitPhone(client, TestWard::smithId(), TestWard::aliceId(), phone);
}

QString alicesPhone(TestClient& client)
{
    return TestSharing::phoneOf(client, TestWard::aliceId());
}

/// Hands over the server's replies and events until it has received count requests of method.
bool deliverUntil(FakeServer& server, const QByteArray& method, int count)
{
    while (server.calls(method) < count)
    {
        if (!server.deliverNext())
        {
            return false;
        }
    }
    return true;
}

QSet<QByteArray> fingerprints(const QList<TestClient*>& clients)
{
    QSet<QByteArray> fingerprints;
    for (const TestClient* client : clients)
    {
        fingerprints.insert(client->identity().fingerprint());
    }
    return fingerprints;
}
}  // namespace

ServerSyncTest::ServerSyncTest(QObject* parent)
    : QObject(parent)
{
}

void ServerSyncTest::aNewWardIsCreatedOnTheServerAndConnected_data()
{
    QTest::addColumn<bool>("repliesFirst");
    QTest::newRow("events before the reply") << false;
    QTest::newRow("the reply before events") << true;
}

void ServerSyncTest::aNewWardIsCreatedOnTheServerAndConnected()
{
    QFETCH(bool, repliesFirst);
    FakeServer server;
    server.setRepliesFirst(repliesFirst);
    TestClient alice("Alice", &server);
    alice.createWard(TestWard::document());
    QVERIFY(!alice.session().isOnServer());
    QSignalSpy fetched(&alice.sync(), &WardSync::versionFetched);

    alice.startSync();

    DocumentId document = alice.session().documentId();
    QCOMPARE(server.number(document), qint64(2));
    QCOMPARE(alice.session().version().number, qint64(2));
    QVERIFY(alice.session().isOnServer());
    QCOMPARE(server.calls("POST"), 1);
    QCOMPARE(server.calls("PUT"), 0);
    // The stored create is held as it stands, with nothing fetched back.
    QCOMPARE(fetched.size(), qsizetype(0));
    // The server refuses a channel to a ward it does not have, so the channel opens again
    // once the ward is created.
    QCOMPARE(server.calls("OPEN"), 2);
    QVERIFY(alice.sync().isSyncing());
    // The server lists her, and she is told of no one else.
    QCOMPARE(server.present(document), fingerprints({&alice}));
    QVERIFY(alice.sync().present().isEmpty());
}

void ServerSyncTest::aNewWardIsConnectedThoughItsFirstChannelIsRefusedAfterTheCreateAnswers()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    alice.createWard(TestWard::document());

    // The head and the create answer before the refusal of the channel opened at the start.
    alice.sync().start();
    QVERIFY(server.deliverNextReply());
    QCOMPARE(server.calls("POST"), 1);
    QVERIFY(server.deliverNextReply());
    QVERIFY(alice.session().isOnServer());
    server.settle();

    QVERIFY(alice.sync().isSyncing());
    QCOMPARE(server.present(alice.session().documentId()), fingerprints({&alice}));
}

void ServerSyncTest::aCreateWhoseAnswerIsLostIsSettledByTheNextLook()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    alice.createWard(TestWard::document());
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QSignalSpy fetched(&alice.sync(), &WardSync::versionFetched);
    server.dropNextWriteReply();

    alice.startSync();

    DocumentId document = alice.session().documentId();
    // The look found the create on the server and took it: its mark for Alice emptied the
    // journal with nothing replayed, and nothing more was sent.
    QCOMPARE(fetched.size(), qsizetype(1));
    QVERIFY(fetched[0][0].value<TakeResult>().replay.status == ReplayStatus::FastForwarded);
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
    QCOMPARE(server.calls("POST"), 1);
    QCOMPARE(server.calls("PUT"), 0);
    QCOMPARE(server.number(document), qint64(2));
    QCOMPARE(alice.session().version().number, qint64(2));
    QVERIFY(alice.session().isOnServer());
    QVERIFY(alice.sync().isSyncing());
}

void ServerSyncTest::aWardTheServerNoLongerHasIsNoLongerShared()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});
    server.setReachable(false);
    server.settle();
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    server.setReachable(true);

    server.forget(document);
    alice.sync().retry();
    server.settle();

    QVERIFY(alice.session().isNoLongerShared());
    QVERIFY(!alice.sync().isRunning());
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-3333") == CommitStatus::Refused);
    QCOMPARE(server.calls("POST"), 1);
    QCOMPARE(server.number(document), qint64(0));
}

void ServerSyncTest::aCommandIsPushedAndTheOtherClientTakesIt_data()
{
    QTest::addColumn<bool>("repliesFirst");
    QTest::newRow("events before the reply") << false;
    QTest::newRow("the reply before events") << true;
}

void ServerSyncTest::aCommandIsPushedAndTheOtherClientTakesIt()
{
    QFETCH(bool, repliesFirst);
    FakeServer server;
    server.setRepliesFirst(repliesFirst);
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    QCOMPARE(server.number(document), qint64(2));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));
    QSignalSpy alicesFetches(&alice.sync(), &WardSync::versionFetched);
    QSignalSpy bobsFetches(&bob.sync(), &WardSync::versionFetched);

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();

    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(server.calls("PUT"), 1);
    QVERIFY(alice.session().journal().isEmpty());
    // Alice holds the version she pushed, with nothing fetched back.
    QCOMPARE(alicesFetches.size(), qsizetype(0));
    // Bob heard of it on his channel and fetched it; the test asked nothing of him.
    QCOMPARE(bobsFetches.size(), qsizetype(1));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QVERIFY(bob.session().version() == alice.session().version());
}

void ServerSyncTest::anUpdateFileTakenWhileAPushIsOnItsWayIsSettledByALook()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    bob.sync().stop();
    server.settle();
    QVERIFY(commitAlicesPhone(bob, "555-7777") == CommitStatus::Applied);
    bob.session().writeUpdate();
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(bob.session().version().number, qint64(4));

    // Alice's push of version 3 is stored, and she takes Bob's file before its answer arrives.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(deliverUntil(server, "PUT", 1));
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(alice.session().version().number, qint64(4));
    server.settle();

    // She did not hold the push she no longer builds on; her next version went over it.
    QCOMPARE(server.number(document), qint64(5));
    QCOMPARE(alice.session().version().number, qint64(5));
    QVERIFY(alice.session().journal().isEmpty());
    bob.startSync();
    for (TestClient* client : {&alice, &bob})
    {
        QCOMPARE(TestSharing::bobsPhone(*client), QString("555-2222"));
        QCOMPARE(alicesPhone(*client), QString("555-7777"));
    }
}

void ServerSyncTest::aPushLandedAndThenChangedByAnotherIsNotReplayed()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});

    // Alice's push is stored, and she goes away before its answer or any notice reaches her.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(deliverUntil(server, "PUT", 1));
    alice.sync().stop();
    server.settle();
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    // Bob changes the same field on top of it.
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.number(document), qint64(4));
    QSignalSpy fetched(&alice.sync(), &WardSync::versionFetched);

    alice.startSync();

    // Her entry had landed, so it is dropped by her mark and not replayed over Bob's value.
    QCOMPARE(fetched.size(), qsizetype(1));
    QVERIFY(fetched[0][0].value<TakeResult>().replay.status == ReplayStatus::FastForwarded);
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-3333"));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-3333"));
    QCOMPARE(server.number(document), qint64(4));
}

void ServerSyncTest::aCommandMadeWhileAPushIsOnItsWayGoesOutWithTheNext()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(deliverUntil(server, "PUT", 1));
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(commitAlicesPhone(alice, "555-7777") == CommitStatus::Applied);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(2));
    server.settle();

    // The second command did not ride in the first push: it went out as its own version.
    QCOMPARE(server.number(document), qint64(4));
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(alicesPhone(bob), QString("555-7777"));
}

void ServerSyncTest::aPushWhoseAnswerIsLostIsNotAppliedTwice()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    QSignalSpy fetched(&alice.sync(), &WardSync::versionFetched);
    server.dropNextWriteReply();

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();

    // Alice fetched her own stored version, and its mark for her emptied the journal with
    // nothing replayed.
    QCOMPARE(fetched.size(), qsizetype(1));
    TakeResult taken = fetched[0][0].value<TakeResult>();
    QVERIFY(taken.status == TakeStatus::Taken);
    QVERIFY(taken.replay.status == ReplayStatus::FastForwarded);
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(alice.sync().problem() == SyncProblem::None);
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(server.calls("PUT"), 1);
    QVERIFY(bob.session().version() == alice.session().version());
}

void ServerSyncTest::aPushWhoseAnswerIsLostIsSettledAfterARestart()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(deliverUntil(server, "PUT", 1));
    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));

    // The app closes before the answer arrives.
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QCOMPARE(alice.session().version().number, qint64(2));
    alice.startSync();

    QCOMPARE(alice.session().version().number, qint64(3));
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
    QCOMPARE(server.number(document), qint64(3));
}

void ServerSyncTest::twoPushesOfOneNumberTheSecondReplaysAndPushesTheNext()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    server.setReachable(false);
    server.settle();
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(commitAlicesPhone(bob, "555-7777") == CommitStatus::Applied);
    server.settle();
    server.setReachable(true);
    QSignalSpy fetched(&bob.sync(), &WardSync::versionFetched);

    // Both read the head before either pushes, so both push version 3.
    alice.sync().retry();
    bob.sync().retry();
    server.settle();

    // The server kept Alice's and refused Bob's; he took hers, replayed, and pushed 4.
    QCOMPARE(server.calls("PUT"), 3);
    QCOMPARE(fetched.size(), qsizetype(1));
    QVERIFY(fetched[0][0].value<TakeResult>().replay.status == ReplayStatus::Replayed);
    QCOMPARE(server.number(document), qint64(4));
    for (TestClient* client : {&alice, &bob})
    {
        QCOMPARE(TestSharing::bobsPhone(*client), QString("555-2222"));
        QCOMPARE(alicesPhone(*client), QString("555-7777"));
        QVERIFY(client->session().journal().isEmpty());
        QCOMPARE(client->session().version().number, qint64(4));
    }
}

void ServerSyncTest::aConflictAbandonsTheJournalAndTheNextCommandPushes()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    server.setReachable(false);
    server.settle();
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    server.settle();
    server.setReachable(true);
    QSignalSpy fetched(&bob.sync(), &WardSync::versionFetched);

    alice.sync().retry();
    server.settle();
    bob.sync().retry();
    server.settle();

    QCOMPARE(fetched.size(), qsizetype(1));
    TakeResult taken = fetched[0][0].value<TakeResult>();
    QVERIFY(taken.replay.status == ReplayStatus::Abandoned);
    QVERIFY2(taken.replay.toRedo.contains("555-3333"), qPrintable(taken.replay.toRedo));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QVERIFY(bob.session().journal().isEmpty());
    QCOMPARE(server.number(document), qint64(3));

    QVERIFY(commitAlicesPhone(bob, "555-7777") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.number(document), qint64(4));
    QCOMPARE(alicesPhone(alice), QString("555-7777"));
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
}

void ServerSyncTest::anAutomaticUpdateIsPushedOnce()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    alice.session().automaticUpdates().run();
    QStringList requests = alice.geocoder().requests();
    QCOMPARE(requests.size(), qsizetype(2));
    QVERIFY(alice.session().document().addressCoordinates().isEmpty());

    alice.geocoder().resolve(requests[0], ElmPosition);
    server.settle();
    QCOMPARE(alice.session().document().addressCoordinates().size(), qsizetype(1));
    QCOMPARE(server.number(document), qint64(2));
    alice.geocoder().resolve(requests[1], ElmPosition);
    server.settle();

    // The batch pushed once, when it finished, though it is in no journal.
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(bob.session().document().addressCoordinates().size(), qsizetype(2));
    QCOMPARE(bob.session().version().number, qint64(3));
}

void ServerSyncTest::anAutomaticUpdateWhoseAnswerIsLostIsNotPushedAgain()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    alice.session().automaticUpdates().run();
    QStringList requests = alice.geocoder().requests();
    QCOMPARE(requests.size(), qsizetype(2));
    server.dropNextWriteReply();

    alice.geocoder().resolve(requests[0], ElmPosition);
    alice.geocoder().resolve(requests[1], ElmPosition);
    server.settle();

    // The push was stored; the look after the lost answer took it, and sent nothing more.
    QCOMPARE(server.calls("PUT"), 1);
    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(alice.session().version().number, qint64(3));
    QCOMPARE(alice.session().document().addressCoordinates().size(), qsizetype(2));
    QCOMPARE(bob.session().document().addressCoordinates().size(), qsizetype(2));
}

void ServerSyncTest::aSaveThatFailsPushesNothing()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    QVERIFY(alice.sync().isSyncing());
    alice.store().setFailWrites(true);

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();

    QCOMPARE(server.number(document), qint64(2));
    QCOMPARE(server.calls("PUT"), 0);
    QVERIFY(alice.session().savePending());
    QVERIFY(!alice.sync().isSyncing());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));

    alice.store().setFailWrites(false);
    alice.sync().retry();
    server.settle();
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.sync().isSyncing());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
}

void ServerSyncTest::aSaveThatFailsWhileALookIsOnItsWayIsNotPushed()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});
    int heads = server.calls("GET");

    alice.sync().look();
    QCOMPARE(server.calls("GET"), heads + 1);
    alice.store().setFailWrites(true);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();

    QCOMPARE(server.calls("PUT"), 0);
    QCOMPARE(server.number(document), qint64(2));

    alice.store().setFailWrites(false);
    alice.sync().retry();
    server.settle();
    QCOMPARE(server.calls("PUT"), 1);
    QCOMPARE(server.number(document), qint64(3));
}

void ServerSyncTest::aServerAheadWithAnEmptyJournalFastForwards()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    bob.sync().stop();
    server.settle();
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();
    QVERIFY(commitAlicesPhone(alice, "555-7777") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.number(document), qint64(4));
    QCOMPARE(bob.session().version().number, qint64(2));
    QSignalSpy fetched(&bob.sync(), &WardSync::versionFetched);
    int puts = server.calls("PUT");

    bob.startSync();

    QCOMPARE(fetched.size(), qsizetype(1));
    QVERIFY(fetched[0][0].value<TakeResult>().replay.status == ReplayStatus::FastForwarded);
    QCOMPARE(bob.session().version().number, qint64(4));
    QCOMPARE(alicesPhone(bob), QString("555-7777"));
    QCOMPARE(server.calls("PUT"), puts);
}

void ServerSyncTest::aHeadThatRanksBelowIsPushedOver()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});
    QByteArray second = server.version(document);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.session().journal().isEmpty());

    // The server is restored to the version before Alice's push.
    server.serve(document, second);
    alice.sync().retry();
    server.settle();

    QCOMPARE(server.number(document), qint64(4));
    QCOMPARE(alice.session().version().number, qint64(4));
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
    WardRead head = WardFile::read(server.version(document), alice.identity(), PayloadKind::Version);
    QVERIFY2(head.status == WardReadStatus::Read, qPrintable(head.problem));
    QCOMPARE(TestWard::person(head.payload->ward, TestWard::bobId()).phone(), QString("555-2222"));
}

void ServerSyncTest::aHeadFromBeforeThisDeviceJoinedIsRetriedUntilAMemberPushes()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    // A version of the ward from before Bob was in it, as a restored server would hold.
    Document before = alice.session().document();
    before.setAccess(TestSharing::accessOf({&alice}, {}));
    QByteArray old = WardFile::write(document, VersionRank{1, alice.identity().fingerprint()},
                                     TestSharing::keyOf(alice), WardPayload{before, {}, std::nullopt});
    server.serve(document, old);

    bob.sync().retry();
    server.settle();

    QVERIFY(bob.sync().problem() == SyncProblem::PushFault);
    QVERIFY(!bob.session().isNoLongerShared());
    QCOMPARE(server.number(document), qint64(1));

    alice.sync().retry();
    server.settle();

    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(bob.sync().problem() == SyncProblem::None);
    QVERIFY(bob.session().version() == alice.session().version());
}

void ServerSyncTest::aVersionOfAnotherWardReplacesNothing()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient mallory("Mallory");
    DocumentId document = shared(server, {&alice});
    QByteArray second = server.version(document);
    mallory.createWard(TestWard::document());
    mallory.session().writeUpdate();
    mallory.session().writeUpdate();
    QByteArray other = mallory.session().nextVersion().bytes;
    Document aliceBefore = alice.session().document();

    server.serve(document, other);
    alice.sync().retry();
    server.settle();

    QVERIFY(alice.sync().problem() == SyncProblem::Unreadable);
    QVERIFY(alice.session().document() == aliceBefore);
    QCOMPARE(alice.session().version().number, qint64(2));
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.calls("PUT"), 0);
    QVERIFY(alice.sync().problem() == SyncProblem::Unreadable);

    // The server has a readable version again.
    server.serve(document, second);
    alice.sync().retry();
    server.settle();
    QVERIFY(alice.sync().problem() == SyncProblem::None);
    QCOMPARE(server.number(document), qint64(3));
}

void ServerSyncTest::aNewerFormatOnTheServerStopsPushesAndKeepsEdits()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});
    QByteArray second = server.version(document);

    server.serve(document, QByteArray("{\"format\":2}\nfrom a newer app"));
    alice.sync().retry();
    server.settle();
    QVERIFY(alice.sync().problem() == SyncProblem::NewerFormat);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();

    QCOMPARE(server.calls("PUT"), 0);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QVERIFY(!alice.session().savePending());
    QVERIFY(!alice.sync().isSyncing());

    server.serve(document, second);
    alice.sync().retry();
    server.settle();
    QVERIFY(alice.sync().problem() == SyncProblem::None);
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.session().journal().isEmpty());
}

void ServerSyncTest::aRemovedDeviceIsNoLongerSharedAndStopsAtOnce()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    QVERIFY(bob.sync().isSyncing());

    CommandPtr removal = RemoveUserCommand::create(alice.identity().fingerprint(), TestSharing::userNamed(alice, "Bob"));
    QVERIFY(alice.session().commit(removal).status == CommitStatus::Applied);
    server.settle();

    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(bob.session().isNoLongerShared());
    QVERIFY(!bob.sync().isRunning());
    QVERIFY(!bob.sync().isConnected());
    QCOMPARE(server.present(document), fingerprints({&alice}));
    int calls = server.calls();
    bob.sync().retry();
    server.settle();
    QCOMPARE(server.calls(), calls);
}

void ServerSyncTest::editsMadeWhileUnreachableGoUpOnReconnect()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    server.setReachable(false);
    server.settle();
    QVERIFY(!alice.sync().isConnected());

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(commitAlicesPhone(alice, "555-7777") == CommitStatus::Applied);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-4444") == CommitStatus::Applied);
    server.settle();
    QVERIFY(alice.sync().problem() == SyncProblem::Unreachable);
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(3));
    QCOMPARE(server.number(document), qint64(2));

    server.setReachable(true);
    alice.sync().retry();
    server.settle();

    // One version carries all three.
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(alice.sync().isSyncing());
    bob.sync().retry();
    server.settle();
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-4444"));
    QCOMPARE(alicesPhone(bob), QString("555-7777"));
}

void ServerSyncTest::afterTheServerComesBackTheWriteIsSentAgain()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    DocumentId document = shared(server, {&alice});

    // The head answers, and the server goes away before the push it leads to.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.setReachable(false);
    server.settle();
    QCOMPARE(server.calls("PUT"), 1);
    QCOMPARE(server.number(document), qint64(2));
    QVERIFY(alice.sync().problem() == SyncProblem::Unreachable);

    server.setReachable(true);
    alice.sync().retry();
    server.settle();

    QVERIFY(alice.sync().problem() == SyncProblem::None);
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(alice.session().journal().isEmpty());
}

void ServerSyncTest::theChannelSaysWhoIsHere()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    TestClient carol("Carol", &server);
    DocumentId document = TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));
    alice.startSync();
    QVERIFY(alice.sync().isConnected());
    QVERIFY(alice.sync().present().isEmpty());

    // Each is told of the other, and never of itself.
    bob.startSync();
    QCOMPARE(server.present(document), fingerprints({&alice, &bob}));
    QCOMPARE(alice.sync().present(), fingerprints({&bob}));
    QCOMPARE(bob.sync().present(), fingerprints({&alice}));
    QVERIFY(bob.sync().isConnected());

    bob.sync().stop();
    server.settle();
    QVERIFY(alice.sync().present().isEmpty());
    QVERIFY(bob.sync().present().isEmpty());
    QVERIFY(!bob.sync().isConnected());

    // Carol holds a copy of the ward that names her, but the server's version does not, so
    // her channel is refused and nobody is told she is here.
    AccessList access = TestSharing::accessOf({&alice, &bob, &carol}, {});
    TestClient::holdWard({&carol}, access, alice, document, TestSharing::keyOf(alice), alice.session().document());
    QSignalSpy presence(&alice.sync(), &WardSync::presenceChanged);
    int opens = server.calls("OPEN");
    carol.sync().start();
    QCOMPARE(server.calls("OPEN"), opens + 1);
    server.settle();
    QVERIFY(!carol.sync().isConnected());
    QCOMPARE(server.present(document), fingerprints({&alice}));
    QCOMPARE(presence.size(), qsizetype(0));
    // Her look then found that the server's version leaves her out.
    QVERIFY(carol.session().isNoLongerShared());
}

void ServerSyncTest::anInvitationCarriesTheServersVersionAndAnEditAfterItSurvivesTheJoin_data()
{
    QTest::addColumn<bool>("repliesFirst");
    QTest::newRow("events before the reply") << false;
    QTest::newRow("the reply before events") << true;
}

void ServerSyncTest::anInvitationCarriesTheServersVersionAndAnEditAfterItSurvivesTheJoin()
{
    QFETCH(bool, repliesFirst);
    FakeServer server;
    server.setRepliesFirst(repliesFirst);
    TestClient alice("Alice", &server);
    TestClient bob("Bob's phone", &server);
    DocumentId document = shared(server, {&alice});
    QSignalSpy invitations(&alice.sync(), &WardSync::invitationFetched);

    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(made.commit.status == CommitStatus::Applied);
    alice.sync().fetchInvitation(made.invite);
    // The invite's push is still on its way, so there is no invitation yet.
    QCOMPARE(invitations.size(), qsizetype(0));
    QCOMPARE(server.number(document), qint64(2));
    server.settle();

    QCOMPARE(invitations.size(), qsizetype(1));
    QCOMPARE(invitations[0][0].toByteArray(), made.invite);
    QByteArray invitation = invitations[0][1].toByteArray();
    QCOMPARE(server.number(document), qint64(3));
    QCOMPARE(TestSharing::versionIn(invitation), server.version(document));

    // Alice makes one more edit, so the head is one above the invitation's version.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    server.settle();
    QCOMPARE(server.number(document), qint64(4));

    InvitedWard invited = bob.session().readInvitation(invitation);
    QVERIFY2(invited.status == InvitationStatus::Joinable, qPrintable(invited.problem));
    bob.session().join(invited, {"Bob Brown", ""}, "Bob's phone");
    // Until a version names his device, Bob signs as the invite, and is here under it.
    bob.sync().start();
    while (!bob.sync().isConnected())
    {
        QVERIFY(server.deliverNext());
    }
    QCOMPARE(alice.sync().present(), QSet<QByteArray>{made.invite});
    QCOMPARE(bob.sync().present(), fingerprints({&alice}));
    server.settle();

    QCOMPARE(server.number(document), qint64(5));
    for (TestClient* client : {&alice, &bob})
    {
        QVERIFY(TestSharing::userNames(*client).contains("Bob Brown"));
        QCOMPARE(TestSharing::bobsPhone(*client), QString("555-2222"));
        QCOMPARE(client->session().version().number, qint64(5));
        QVERIFY(client->session().journal().isEmpty());
    }
    // Bob's channel was opened as the invite, and again as his device once the join landed.
    QVERIFY(bob.sync().isSyncing());
    QCOMPARE(server.present(document), fingerprints({&alice, &bob}));
    QCOMPARE(alice.sync().present(), fingerprints({&bob}));
    QCOMPARE(bob.sync().present(), fingerprints({&alice}));
}

void ServerSyncTest::anInvitationIsNotGivenWhileNothingCanBeShared()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    shared(server, {&alice});
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    server.settle();
    QSignalSpy invitations(&alice.sync(), &WardSync::invitationFetched);

    server.setReachable(false);
    server.settle();
    alice.sync().fetchInvitation(made.invite);
    QCOMPARE(invitations.size(), qsizetype(1));
    QVERIFY(invitations[0][1].toByteArray().isEmpty());

    // Reachable again, the invitation is given; once the invite is withdrawn, it is not.
    server.setReachable(true);
    alice.sync().retry();
    server.settle();
    alice.sync().fetchInvitation(made.invite);
    server.settle();
    QCOMPARE(invitations.size(), qsizetype(2));
    QVERIFY(!invitations[1][1].toByteArray().isEmpty());

    AccessInvite invite = *alice.session().document().access().findInvite(made.invite);
    CommandPtr withdrawal = WithdrawInviteCommand::create(alice.identity().fingerprint(), invite,
                                                          alice.session().document().access());
    QVERIFY(alice.session().commit(withdrawal).status == CommitStatus::Applied);
    alice.sync().fetchInvitation(made.invite);
    server.settle();
    QCOMPARE(invitations.size(), qsizetype(3));
    QVERIFY(invitations[2][1].toByteArray().isEmpty());
}

void ServerSyncTest::aSyncThatIsNotStartedSendsNothing()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    server.settle();

    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(server.calls(), 0);
    QVERIFY(!alice.sync().isRunning());
}

void ServerSyncTest::anUpdateFileAboveTheHeadIsPushedOver()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    alice.sync().stop();
    server.settle();
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QByteArray file = alice.session().writeUpdate();
    QCOMPARE(server.number(document), qint64(2));

    QVERIFY(bob.session().takeUpdate(file).status == TakeStatus::Taken);
    QCOMPARE(bob.session().version().number, qint64(3));
    bob.sync().look();
    server.settle();

    // Bob's next version went over the head, though he had nothing of his own to share.
    QCOMPARE(server.number(document), qint64(4));
    alice.startSync();
    QCOMPARE(alice.session().version().number, qint64(4));
    QVERIFY(alice.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-2222"));
}

void ServerSyncTest::membersWhoTradedFilesTurnTheServerOn()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = shared(server, {&alice, &bob});
    alice.sync().stop();
    bob.sync().stop();
    server.settle();
    // With the server off they trade files, and Alice ends two versions ahead of Bob.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(commitAlicesPhone(bob, "555-7777") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(bob, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-4444") == CommitStatus::Applied);
    alice.session().writeUpdate();
    alice.session().writeUpdate();
    QCOMPARE(bob.session().version().number, qint64(4));
    QCOMPARE(alice.session().version().number, qint64(6));
    QCOMPARE(server.number(document), qint64(2));

    // Bob is first to look, and pushes over the older head.
    bob.startSync();
    QCOMPARE(server.number(document), qint64(5));
    QCOMPARE(server.calls("PUT"), 1);
    // Alice holds a version that ranks above his, and pushes over it in turn; Bob takes hers.
    alice.startSync();
    server.settle();

    QCOMPARE(server.calls("PUT"), 2);
    QCOMPARE(server.number(document), qint64(7));
    QVERIFY(alice.session().version() == bob.session().version());
    QVERIFY(alice.session().document() == bob.session().document());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-4444"));
    QCOMPARE(alicesPhone(bob), QString("555-7777"));
    QVERIFY(alice.sync().isSyncing() && bob.sync().isSyncing());
}

void ServerSyncTest::aWardSharedByFilesIsCreatedByTheFirstToLook()
{
    FakeServer server;
    TestClient alice("Alice", &server);
    TestClient bob("Bob", &server);
    DocumentId document = TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(alice.session().version().number, qint64(2));

    // Neither has seen the ward on the server, and both look before either create lands.
    bob.sync().start();
    alice.sync().start();
    server.settle();

    // The server kept Bob's create and refused Alice's, and she took his.
    QCOMPARE(server.calls("POST"), 2);
    QCOMPARE(server.calls("PUT"), 0);
    QCOMPARE(server.number(document), qint64(3));
    QVERIFY(server.version(document).contains(Base64Url::encode(bob.identity().fingerprint()).toLatin1()));
    QVERIFY(alice.session().version() == bob.session().version());
    QVERIFY(alice.session().document() == bob.session().document());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QVERIFY(alice.sync().isSyncing() && bob.sync().isSyncing());
}
