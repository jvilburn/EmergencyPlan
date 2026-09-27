#include "FakeServerTest.h"

#include "DocumentCrypto.h"
#include "FakeServer.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ChainTestSupport;
using namespace ServerContract;
using namespace ServerTestSupport;

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
    QCOMPARE(server.challenge().error, ServerError::Unreachable);
    QCOMPARE(server.head(ward->aliceSession, ward->id).error, ServerError::Unreachable);
    QCOMPARE(server.download(ward->aliceSession, ward->id).error, ServerError::Unreachable);
    QCOMPARE(server.myDocuments(ward->aliceSession).error, ServerError::Unreachable);

    server.setOffline(false);
    QVERIFY(server.head(ward->aliceSession, ward->id).ok());
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
    QVERIFY(server.head(w.aliceSession, w.id).value->leaseHolder == w.carol.fingerprint());

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

    server.withholdHeader(w.id, 2);

    const Result<HeaderChain> chain = server.headers(w.aliceSession, w.id, 1, w.version);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.size(), qsizetype(1));
    QVERIFY(chain.value->headers.first() == w.current);
    QVERIFY(!(chain.value->headers.first() == grant));
}

void FakeServerTest::aSubstitutedHeaderIsServedInItsPlace()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    DocumentHeader forged = w.current;
    forged.saveCounter += 100;
    server.substituteHeader(w.id, w.version, forged);

    const Result<HeaderChain> chain = server.headers(w.aliceSession, w.id, 1, w.version);
    QVERIFY(chain.ok());
    QVERIFY(chain.value->headers.last() == forged);

    // Only the chain: a download is still the true state.
    QVERIFY(server.download(w.aliceSession, w.id).value->header == w.current);
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

    QCOMPARE(server.pairing(w.aliceSession, code).value->commitment, QByteArray("first commitment"));
    QCOMPARE(server.pairing(w.aliceSession, code).value->commitment, QByteArray("second commitment"));
    QCOMPARE(server.pairing(w.aliceSession, code).value->commitment, QByteArray("the real commitment"));
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
