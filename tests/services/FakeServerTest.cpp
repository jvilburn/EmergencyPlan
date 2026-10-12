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
    QCOMPARE(headOf(server, ward->id).error, ServerError::Unreachable);
    QCOMPARE(downloadOf(server, ward->id).error, ServerError::Unreachable);
    QCOMPARE(publish(server, ward.value(), ward->alice, {ward->alice}), ServerError::Unreachable);
    QCOMPARE(takeLeaseAs(server, ward->alice, ward->id).error, ServerError::Unreachable);
    const TestDevice dave = newDevice("Dave");
    QCOMPARE(registerAs(server, dave, uploadFor(newDocumentId(), {dave}, 1)).error, ServerError::Unreachable);

    server.setOffline(false);
    QCOMPARE(headOf(server, ward->id).value->version, 2);
}

void FakeServerTest::aForcedConflictRefusesTheNextUploadOnly()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.forceConflict(w.id);
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};
    QCOMPARE(publish(server, w, w.alice, everyone), ServerError::VersionConflict);
    QCOMPARE(w.version, 2);
    QCOMPARE(publish(server, w, w.alice, everyone), ServerError::None);
    QCOMPARE(w.version, 3);
}

void FakeServerTest::aLeaseCanBeHeldByAnotherDevice()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.holdLeaseAs(w.id, w.carol.fingerprint());

    QCOMPARE(takeLeaseAs(server, w.alice, w.id).error, ServerError::LeaseHeld);
    QVERIFY(headOf(server, w.id).value->leaseHolder == w.carol.fingerprint());

    // A held lease never blocks a roster edit or a response record: the lease is
    // advisory at the server, which cannot tell a directory edit from either.
    QCOMPARE(publish(server, w, w.alice, {w.alice, w.bob, w.carol}), ServerError::None);
    QCOMPARE(w.version, 3);
}

void FakeServerTest::limitsAreConfigurable()
{
    FakeServer::Limits limits;
    limits.uploadsPerHour = 2;
    FakeServer server(limits);
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};

    // wardOfThree's own upload was Alice's first of the hour.
    QCOMPARE(publish(server, w, w.alice, everyone), ServerError::None);
    QCOMPARE(publish(server, w, w.alice, everyone), ServerError::RateLimited);
    QCOMPARE(w.version, 3);

    // Counted per signer, and per hour.
    QCOMPARE(publish(server, w, w.bob, everyone), ServerError::None);
    QCOMPARE(w.version, 4);
    QVERIFY(advanceClock(server, 61 * 60));
    QCOMPARE(publish(server, w, w.alice, everyone), ServerError::None);
    QCOMPARE(w.version, 5);
}

// --- Replies ---------------------------------------------------------------------

void FakeServerTest::aReplyNeverArrivesBeforeTheCallReturns()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());

    bool answered = false;
    server.head(ward->id, [&answered](const Result<Head>&) { answered = true; });
    QVERIFY(!answered);

    // A failure is answered later too.
    bool refused = false;
    server.head(newDocumentId(), [&refused](const Result<Head>&) { refused = true; });
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
    server.head(w.id, std::bind(&noteArrival, std::ref(arrived), QString("head")));
    server.download(w.id, std::bind(&noteArrival, std::ref(arrived), QString("download")));
    server.releaseLease(w.alice.signer(signedBytes(CallReleaseLease, w.id)),
                        w.id,
                        std::bind(&noteArrival, std::ref(arrived), QString("release")));

    // Held means held, however long the event loop runs.
    QTest::qWait(50);
    QVERIFY(arrived.isEmpty());

    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, QList<QString>{"head"});
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download"}));
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download", "release"}));
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
    server.upload(w.alice.signer(signedBytes(CallUpload, w.id, w.version, upload)),
                  w.id,
                  w.version,
                  upload,
                  std::bind(&keepUpload, std::ref(uploaded), _1));

    std::optional<Result<Head>> head;
    server.head(w.id, std::bind(&keepHead, std::ref(head), _1));

    // Both were decided when made: going offline before they are delivered
    // changes neither. The head asked after the upload sees it, though the
    // uploader has not heard back.
    server.setOffline(true);
    QVERIFY(server.deliverNext());
    QVERIFY(server.deliverNext());
    QVERIFY(uploaded.has_value() && uploaded->ok());
    QVERIFY(head.has_value() && head->ok());
    QCOMPARE(head->value->version, 3);
    QCOMPARE(uploaded->value.value(), 3);
}

void FakeServerTest::aLostReplysUploadIsStillTheCurrentVersion()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload upload = uploadFor(w.id, {w.alice, w.bob, w.carol}, 5);

    server.loseReplies(true);
    const Result<qint64> lost = uploadAs(server, w.alice, w.id, w.version, upload);
    QCOMPARE(lost.error, ServerError::Unreachable);
    server.loseReplies(false);

    const Result<Snapshot> snapshot = downloadOf(server, w.id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, 3);
    QCOMPARE(snapshot.value->header, upload.header.toJson());
    QCOMPARE(snapshot.value->blob, upload.blob);
}
