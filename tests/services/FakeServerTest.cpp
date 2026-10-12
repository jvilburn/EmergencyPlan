#include "FakeServerTest.h"

#include "DocumentCrypto.h"
#include "FakeServer.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ServerContract;
using namespace ServerTestSupport;
using namespace std::placeholders;

namespace
{

Result<Head> headOf(FakeServer& server, const Session& session, const QString& documentId)
{
    return wait<Head>(std::bind(&ServerApi::head, &server, session, documentId, _1));
}

Result<Snapshot> downloadOf(FakeServer& server, const Session& session, const QString& documentId)
{
    return wait<Snapshot>(std::bind(&ServerApi::download, &server, session, documentId, _1));
}

void keepUpload(std::optional<Result<qint64>>& out, const Result<qint64>& result)
{
    out = result;
}

void keepHead(std::optional<Result<Head>>& out, const Result<Head>& result)
{
    out = result;
}

void noteArrival(QList<QString>& arrived, const QString& name)
{
    arrived.append(name);
}

} // namespace

void FakeServerTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void FakeServerTest::offlineRefusesEverything()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());

    server.setOffline(true);
    QCOMPARE(wait<QByteArray>(std::bind(&ServerApi::challenge, &server, _1)).error, ServerError::Unreachable);
    QCOMPARE(headOf(server, ward->aliceSession, ward->id).error, ServerError::Unreachable);
    QCOMPARE(downloadOf(server, ward->aliceSession, ward->id).error, ServerError::Unreachable);
    QCOMPARE(wait<Me>(std::bind(&ServerApi::me, &server, ward->aliceSession, _1)).error, ServerError::Unreachable);

    server.setOffline(false);
    QVERIFY(headOf(server, ward->aliceSession, ward->id).ok());
}

void FakeServerTest::aForcedConflictRefusesTheNextUploadOnly()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.forceConflict(w.id);
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};
    QCOMPARE(publish(server, w, w.aliceSession, everyone), ServerError::VersionConflict);
    QCOMPARE(publish(server, w, w.aliceSession, everyone), ServerError::None);
}

void FakeServerTest::aLeaseCanBeHeldByAnotherMember()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.holdLeaseAs(w.id, w.carol.fingerprint());

    QCOMPARE(wait<Lease>(std::bind(&ServerApi::takeLease, &server, w.aliceSession, w.id, _1)).error,
             ServerError::LeaseHeld);
    QVERIFY(headOf(server, w.aliceSession, w.id).value->leaseHolder == w.carol.fingerprint());

    // A held lease never blocks a sealing addition or a response record: the lease
    // is advisory at the server, which cannot tell a directory edit from either.
    QCOMPARE(publish(server, w, w.aliceSession, {w.alice, w.bob, w.carol}), ServerError::None);
}

void FakeServerTest::limitsAreConfigurable()
{
    FakeServer::Limits limits;
    limits.registrationsPerKey = 2;
    limits.registrationsPerHour = 100;
    FakeServer server(limits);

    const TestDevice device = newPerson("Alice");
    const std::optional<Session> session = join(server, device);
    QVERIFY(session.has_value());

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        QVERIFY(wait<qint64>(std::bind(
            &ServerApi::registerDocument, &server, session.value(), uploadFor(newDocumentId(), {device}, 1), _1)).ok());
    }
    QCOMPARE(wait<qint64>(std::bind(
                 &ServerApi::registerDocument, &server, session.value(), uploadFor(newDocumentId(), {device}, 1), _1))
                 .error,
             ServerError::RateLimited);
}

// --- Replies ---------------------------------------------------------------------

void FakeServerTest::aReplyNeverArrivesBeforeTheCallReturns()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());

    bool answered = false;
    server.head(ward->aliceSession, ward->id, [&answered](const Result<Head>&) { answered = true; });
    QVERIFY(!answered);

    // A failure is answered later too.
    bool refused = false;
    server.head(Session{"not a session"}, ward->id, [&refused](const Result<Head>&) { refused = true; });
    QVERIFY(!refused);

    QTRY_VERIFY(answered);
    QTRY_VERIFY(refused);
}

void FakeServerTest::heldRepliesArriveInCallOrderOneAtATime()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.holdReplies(true);

    QList<QString> arrived;
    server.head(w.aliceSession, w.id, std::bind(&noteArrival, std::ref(arrived), QString("head")));
    server.download(w.aliceSession, w.id, std::bind(&noteArrival, std::ref(arrived), QString("download")));
    server.me(w.aliceSession, std::bind(&noteArrival, std::ref(arrived), QString("me")));

    // Held means held, however long the event loop runs.
    QTest::qWait(50);
    QVERIFY(arrived.isEmpty());

    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, QList<QString>{"head"});
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download"}));
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download", "me"}));
    QVERIFY(!server.deliverNext());
}

void FakeServerTest::anUploadTakesEffectWhileItsReplyIsHeld()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload upload = uploadFor(w.id, {w.alice, w.bob, w.carol}, 5);

    server.holdReplies(true);
    std::optional<Result<qint64>> uploaded;
    server.upload(w.aliceSession, w.id, w.version, upload, std::bind(&keepUpload, std::ref(uploaded), _1));

    std::optional<Result<Head>> head;
    server.head(w.bobSession, w.id, std::bind(&keepHead, std::ref(head), _1));

    // Both were decided when made: going offline before they are delivered
    // changes neither. The head asked after the upload sees it, though the
    // uploader has not heard back.
    server.setOffline(true);
    QVERIFY(server.deliverNext());
    QVERIFY(server.deliverNext());
    QVERIFY(uploaded.has_value() && uploaded->ok());
    QVERIFY(head.has_value() && head->ok());
    QCOMPARE(head->value->version, w.version + 1);
    QCOMPARE(uploaded->value.value(), w.version + 1);
}

void FakeServerTest::aLostReplysUploadIsStillTheCurrentVersion()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload upload = uploadFor(w.id, {w.alice, w.bob, w.carol}, 5);

    server.loseReplies(true);
    const Result<qint64> lost =
        wait<qint64>(std::bind(&ServerApi::upload, &server, w.aliceSession, w.id, w.version, upload, _1));
    QCOMPARE(lost.error, ServerError::Unreachable);
    server.loseReplies(false);

    const Result<Snapshot> snapshot = downloadOf(server, w.aliceSession, w.id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, w.version + 1);
    QCOMPARE(snapshot.value->header, upload.header.toJson());
    QCOMPARE(snapshot.value->blob, upload.blob);
}
