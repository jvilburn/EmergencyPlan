#include "FakeServerTest.h"

#include "DocumentCrypto.h"
#include "FakeServer.h"
#include "Reconcile.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ChainTestSupport;
using namespace ServerContract;
using namespace ServerTestSupport;

namespace
{

Result<Head> headOf(FakeServer& server, const Session& session, const QString& documentId)
{
    return wait<Head>([&](Reply<Head> reply) { server.head(session, documentId, reply); });
}

Result<Snapshot> downloadOf(FakeServer& server, const Session& session, const QString& documentId, qint64 after)
{
    return wait<Snapshot>([&](Reply<Snapshot> reply) { server.download(session, documentId, after, reply); });
}

Result<HeaderChain> chainOf(FakeServer& server, const Session& session, const QString& documentId, qint64 after)
{
    return wait<HeaderChain>([&](Reply<HeaderChain> reply) { server.headers(session, documentId, after, reply); });
}

Result<PairingState> pairingOf(FakeServer& server, const Session& session, const QString& code)
{
    return wait<PairingState>([&](Reply<PairingState> reply) { server.pairing(session, code, reply); });
}

/// Starts a signed upload and returns at once, leaving its reply to arrive later.
void startUpload(FakeServer& server,
                 const SharedDocument& ward,
                 const Session& session,
                 const Upload& upload,
                 Reply<qint64> reply)
{
    const QByteArray signature = signRequest(Endpoint::Upload,
                                             session,
                                             ward.id,
                                             uploadBody(ward.version, upload),
                                             signerOf(session).signing.signingKey);
    server.upload(session, ward.id, ward.version, upload, signature, reply);
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
    QCOMPARE(wait<QByteArray>([&](Reply<QByteArray> reply) { server.challenge(reply); }).error,
             ServerError::Unreachable);
    QCOMPARE(headOf(server, ward->aliceSession, ward->id).error, ServerError::Unreachable);
    QCOMPARE(downloadOf(server, ward->aliceSession, ward->id, 1).error, ServerError::Unreachable);
    QCOMPARE(wait<QStringList>([&](Reply<QStringList> reply) { server.myDocuments(ward->aliceSession, reply); })
                 .error,
             ServerError::Unreachable);

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
    QCOMPARE(publish(server, w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice)),
             ServerError::VersionConflict);
    QCOMPARE(publish(server, w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice)),
             ServerError::None);
}

void FakeServerTest::aLeaseCanBeHeldByAnotherMember()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    server.holdLeaseAs(w.id, w.carol.fingerprint());

    QCOMPARE(signedTakeLease(server, w.aliceSession, w.id).error, ServerError::LeaseHeld);
    QVERIFY(headOf(server, w.aliceSession, w.id).value->leaseHolder == w.carol.fingerprint());

    // A held lease never blocks a roster change or a response record: the lease
    // is advisory at the server, which cannot tell a directory edit from either.
    QCOMPARE(publish(server, w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.bob, w.carol}, w.alice)),
             ServerError::None);
}

void FakeServerTest::aWithheldHeaderIsLeftOutOfTheChain()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const DocumentHeader grant = w.current;
    QCOMPARE(publish(server, w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.bob, w.carol}, w.alice)),
             ServerError::None);

    // Version 2 is the grant, kept because its membership changed; version 3 is
    // the current header.
    const Result<HeaderChain> whole = chainOf(server, w.aliceSession, w.id, 1);
    QVERIFY(whole.ok());
    QCOMPARE(whole.value->headers, (QList<QJsonObject>{grant.toJson(), w.current.toJson()}));

    server.withholdHeader(w.id, 2);

    const Result<HeaderChain> chain = chainOf(server, w.aliceSession, w.id, 1);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers, QList<QJsonObject>{w.current.toJson()});

    // A download's chain is withheld the same way, and still ends with its version.
    const Result<Snapshot> snapshot = downloadOf(server, w.aliceSession, w.id, 1);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->headers, QList<QJsonObject>{w.current.toJson()});
}

void FakeServerTest::aSubstitutedHeaderIsServedInItsPlace()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Result<Snapshot> before = downloadOf(server, w.aliceSession, w.id, 1);
    QVERIFY(before.ok());

    DocumentHeader forged = w.current;
    forged.saveCounter += 100;
    server.substituteHeader(w.id, w.version, forged);

    const Result<HeaderChain> chain = chainOf(server, w.aliceSession, w.id, 1);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.last(), forged.toJson());

    // A download carries it too, beside the unchanged blob.
    const Result<Snapshot> after = downloadOf(server, w.aliceSession, w.id, 1);
    QVERIFY(after.ok());
    QCOMPARE(after.value->headers.last(), forged.toJson());
    QCOMPARE(after.value->blob, before.value->blob);

    // Served as stored, so a later format reaches the client as that format.
    DocumentHeader later = w.current;
    later.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    server.substituteHeader(w.id, w.version, later);

    QList<DocumentHeader> parsed;
    QCOMPARE(Reconcile::readHeaders(downloadOf(server, w.aliceSession, w.id, 1).value->headers, parsed),
             Reconcile::Verdict::NeedsNewerVersion);
}

void FakeServerTest::scriptedPairingReadsAreServedInOrder()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server, phone);
    QVERIFY(phoneSession.has_value());

    const QString code = signedStartPairing(server, phoneSession.value(), "Phone", "the real commitment").value.value();

    // What a relay test needs: one commitment before the responder posts, another
    // after.
    PairingState first;
    first.code = code;
    first.commitment = "first commitment";
    PairingState second;
    second.code = code;
    second.commitment = "second commitment";
    server.scriptPairingReads(code, {first, second});

    QCOMPARE(pairingOf(server, w.aliceSession, code).value->commitment, QByteArray("first commitment"));
    QCOMPARE(pairingOf(server, w.aliceSession, code).value->commitment, QByteArray("second commitment"));
    QCOMPARE(pairingOf(server, w.aliceSession, code).value->commitment, QByteArray("the real commitment"));
}

void FakeServerTest::limitsAreConfigurable()
{
    FakeServer::Limits limits;
    limits.registrationsPerKey = 2;
    limits.registrationsPerHour = 100;
    FakeServer server(limits);

    const TestDevice device = newPerson();
    const std::optional<Session> session = signIn(server, device);
    QVERIFY(session.has_value());

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const Upload upload = sealedUpload(
            buildHeader(newDocumentId(), 1, 1, {device}, device, DocumentCrypto::generateDataKey()),
            device);
        QVERIFY(signedRegisterDocument(server, session.value(), upload).ok());
    }

    const Upload third = sealedUpload(
        buildHeader(newDocumentId(), 1, 1, {device}, device, DocumentCrypto::generateDataKey()),
        device);
    QCOMPARE(signedRegisterDocument(server, session.value(), third).error, ServerError::RateLimited);
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
    server.head(w.aliceSession, w.id, [&arrived](const Result<Head>&) { arrived.append("head"); });
    server.download(w.aliceSession, w.id, 1, [&arrived](const Result<Snapshot>&) { arrived.append("download"); });
    server.headers(w.aliceSession, w.id, 1, [&arrived](const Result<HeaderChain>&) { arrived.append("headers"); });

    // Held means held, however long the event loop runs.
    QTest::qWait(50);
    QVERIFY(arrived.isEmpty());

    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, QList<QString>{"head"});
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download"}));
    QVERIFY(server.deliverNext());
    QCOMPARE(arrived, (QList<QString>{"head", "download", "headers"}));
    QVERIFY(!server.deliverNext());
}

void FakeServerTest::anUploadTakesEffectWhileItsReplyIsHeld()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload upload = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);

    server.holdReplies(true);
    std::optional<Result<qint64>> uploaded;
    startUpload(server, w, w.aliceSession, upload, [&uploaded](const Result<qint64>& result) { uploaded = result; });

    std::optional<Result<Head>> head;
    server.head(w.bobSession, w.id, [&head](const Result<Head>& result) { head = result; });

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

    const Upload upload = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);

    server.loseReplies(true);
    const Result<qint64> lost =
        wait<qint64>([&](Reply<qint64> reply) { startUpload(server, w, w.aliceSession, upload, reply); });
    QCOMPARE(lost.error, ServerError::Unreachable);
    server.loseReplies(false);

    const Result<Snapshot> snapshot = downloadOf(server, w.aliceSession, w.id, w.version);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, w.version + 1);
    QCOMPARE(snapshot.value->headers.last(), upload.header.toJson());
    QCOMPARE(snapshot.value->blob, upload.blob);
}
