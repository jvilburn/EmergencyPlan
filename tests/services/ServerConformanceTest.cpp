#include "ServerConformanceTest.h"

#include "DocumentCrypto.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ServerContract;
using namespace ServerTestSupport;
using namespace std::placeholders;

namespace
{

constexpr qint64 ThreeMinutes = 3 * 60;

} // namespace

ServerConformanceTest::ServerConformanceTest(std::function<std::unique_ptr<ServerApi>()> makeServer)
    : m_makeServer(std::move(makeServer))
{
}

void ServerConformanceTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void ServerConformanceTest::init()
{
    m_server = m_makeServer();
    QVERIFY(m_server != nullptr);
}

void ServerConformanceTest::cleanup()
{
    m_server.reset();
}

// --- Documents and versions ------------------------------------------------------

void ServerConformanceTest::registersVersionOneByItsMaker()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(ward->version, 1);

    const Result<Head> head = headOf(server(), ward->id);
    QVERIFY(head.ok());
    QCOMPARE(head.value->version, 1);
    QCOMPARE(head.value->published.device, ward->alice.fingerprint());
    QVERIFY(head.value->published.when.isValid());
    QVERIFY(!head.value->leaseHolder.has_value());
    QCOMPARE(recipientsOf(head.value.value()), QStringList{ward->alice.fingerprint().toHex()});
}

void ServerConformanceTest::aRegistrationMustNameItsSigner()
{
    const TestDevice alice = newDevice("Alice");
    const TestDevice bob = newDevice("Bob");
    const QString id = newDocumentId();
    const Upload sealedToBob = uploadFor(id, {bob}, 1);

    QCOMPARE(registerAs(server(), alice, sealedToBob).error, ServerError::NotAKeyHolder);
    QCOMPARE(headOf(server(), id).error, ServerError::UnknownDocument);

    // The same version, signed by the device it names, is stored.
    const Result<qint64> registered = registerAs(server(), bob, sealedToBob);
    QVERIFY(registered.ok());
    QCOMPARE(registered.value.value(), 1);
    QCOMPARE(headOf(server(), id).value->published.device, bob.fingerprint());
}

void ServerConformanceTest::aRegistrationWithABadSignatureIsAnInvalidRequest()
{
    const TestDevice alice = newDevice("Alice");
    const QString id = newDocumentId();
    const Upload upload = uploadFor(id, {alice}, 1);

    // Signed as an upload rather than a registration.
    const Signer asUpload = alice.signer(signedBytes(CallUpload, id, 0, upload));
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::registerDocument, &server(), asUpload, upload, _1)).error,
             ServerError::InvalidRequest);
    QCOMPARE(headOf(server(), id).error, ServerError::UnknownDocument);

    // Signed over another header, the same blob.
    Upload otherHeader = upload;
    otherHeader.header.saveCounter = 2;
    const Signer wrongHeader = alice.signer(signedBytes(CallRegister, id, 0, otherHeader));
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::registerDocument, &server(), wrongHeader, upload, _1)).error,
             ServerError::InvalidRequest);
    QCOMPARE(headOf(server(), id).error, ServerError::UnknownDocument);

    // The same upload, signed as it is sent, lands.
    QVERIFY(registerAs(server(), alice, upload).ok());
}

void ServerConformanceTest::refusesATakenDocumentId()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice other = newDevice("Other");

    QCOMPARE(registerAs(server(), other, uploadFor(ward->id, {other}, 1)).error, ServerError::IdTaken);

    const Result<Head> head = headOf(server(), ward->id);
    QCOMPARE(head.value->version, 1);
    QCOMPARE(head.value->published.device, ward->alice.fingerprint());
}

void ServerConformanceTest::aMalformedIdIsAnInvalidRequest()
{
    const TestDevice alice = newDevice("Alice");
    const QString wellFormed = newDocumentId();
    const QStringList malformed{"not-an-id", wellFormed.toUpper(), wellFormed.left(31), wellFormed + "0"};

    for (const QString& id : malformed)
    {
        QCOMPARE(headOf(server(), id).error, ServerError::InvalidRequest);
        QCOMPARE(downloadOf(server(), id).error, ServerError::InvalidRequest);
        QCOMPARE(registerAs(server(), alice, uploadFor(id, {alice}, 1)).error, ServerError::InvalidRequest);
        QCOMPARE(uploadAs(server(), alice, id, 1, uploadFor(id, {alice}, 2)).error, ServerError::InvalidRequest);
        QCOMPARE(takeLeaseAs(server(), alice, id).error, ServerError::InvalidRequest);
        QCOMPARE(releaseLeaseAs(server(), alice, id).error, ServerError::InvalidRequest);
    }

    // Anchor: the well-formed id, with the same calls, is unknown until registered.
    QCOMPARE(headOf(server(), wellFormed).error, ServerError::UnknownDocument);
    QVERIFY(registerAs(server(), alice, uploadFor(wellFormed, {alice}, 1)).ok());
}

void ServerConformanceTest::readsNeedNoSigner()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    // Anyone with the id reads ciphertext: the server never protected content.
    const Result<Head> head = headOf(server(), ward->id);
    const Result<Snapshot> snapshot = downloadOf(server(), ward->id);
    QVERIFY(head.ok());
    QVERIFY(snapshot.ok());
    QCOMPARE(head.value->version, 2);
    QCOMPARE(snapshot.value->version, 2);
    QCOMPARE(snapshot.value->header, head.value->header);
    QCOMPARE(recipientsOf(head.value.value()).size(), 3);

    QCOMPARE(headOf(server(), newDocumentId()).error, ServerError::UnknownDocument);
    QCOMPARE(downloadOf(server(), newDocumentId()).error, ServerError::UnknownDocument);
}

void ServerConformanceTest::anUploadByAKeyTheHeaderNamesLands()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QCOMPARE(ward->version, 2);

    QCOMPARE(publish(server(), ward.value(), ward->bob, {ward->alice, ward->bob, ward->carol}), ServerError::None);
    QCOMPARE(ward->version, 3);

    const Result<Head> head = headOf(server(), ward->id);
    QCOMPARE(head.value->version, 3);
    QCOMPARE(head.value->published.device, ward->bob.fingerprint());
}

void ServerConformanceTest::anUploadByAKeyTheHeaderDoesNotNameIsRefused()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const TestDevice stranger = newDevice("Stranger");

    // Even a version that names the stranger: what counts is the header it replaces.
    QCOMPARE(publish(server(), ward.value(), stranger, {ward->alice, stranger}), ServerError::NotAKeyHolder);
    QCOMPARE(ward->version, 2);

    const Result<Head> head = headOf(server(), ward->id);
    QCOMPARE(head.value->version, 2);
    QCOMPARE(head.value->published.device, ward->alice.fingerprint());
}

void ServerConformanceTest::anUploadWhoseHeaderNamesAnotherDocumentIsRefused()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    // Well signed, by a key holder, on the right base: the header's own id is what is
    // wrong.
    const Upload elsewhere = uploadFor(newDocumentId(), {ward->alice, ward->bob, ward->carol}, 5);
    QCOMPARE(uploadAs(server(), ward->bob, ward->id, ward->version, elsewhere).error, ServerError::InvalidRequest);
    QCOMPARE(headOf(server(), ward->id).value->version, 2);
    QCOMPARE(headOf(server(), elsewhere.header.documentId).error, ServerError::UnknownDocument);
}

void ServerConformanceTest::aKeyTheCurrentHeaderDropsCanNoLongerWrite()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QCOMPARE(publish(server(), ward.value(), ward->carol, {ward->alice, ward->bob, ward->carol}), ServerError::None);

    // Alice writes Carol out; Carol's next write is refused, Bob's still lands.
    QCOMPARE(publish(server(), ward.value(), ward->alice, {ward->alice, ward->bob}), ServerError::None);
    QCOMPARE(ward->version, 4);
    QCOMPARE(publish(server(), ward.value(), ward->carol, {ward->alice, ward->bob, ward->carol}),
             ServerError::NotAKeyHolder);
    QCOMPARE(takeLeaseAs(server(), ward->carol, ward->id).error, ServerError::NotAKeyHolder);
    QCOMPARE(publish(server(), ward.value(), ward->bob, {ward->alice, ward->bob}), ServerError::None);
    QCOMPARE(ward->version, 5);
}

void ServerConformanceTest::aBadSignatureIsAnInvalidRequest()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const Upload upload = uploadFor(ward->id, {ward->alice, ward->bob, ward->carol}, 5);

    // Signed over another base version.
    const Signer staleBase = ward->bob.signer(signedBytes(CallUpload, ward->id, ward->version - 1, upload));
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::upload, &server(), staleBase, ward->id, ward->version, upload, _1)).error,
             ServerError::InvalidRequest);

    // Signed over another blob.
    Upload otherBlob = upload;
    otherBlob.blob[0] = char(otherBlob.blob.at(0) ^ 0x01);
    const Signer wrongBlob = ward->bob.signer(signedBytes(CallUpload, ward->id, ward->version, otherBlob));
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::upload, &server(), wrongBlob, ward->id, ward->version, upload, _1)).error,
             ServerError::InvalidRequest);

    // Signed as another call.
    const Signer asRegister = ward->bob.signer(signedBytes(CallRegister, ward->id, ward->version, upload));
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::upload, &server(), asRegister, ward->id, ward->version, upload, _1)).error,
             ServerError::InvalidRequest);

    // Signed by Alice's key, carrying Bob's keys.
    Signer borrowed = ward->alice.signer(signedBytes(CallUpload, ward->id, ward->version, upload));
    borrowed.keys = ward->bob.keys();
    QCOMPARE(wait<qint64>(std::bind(&ServerApi::upload, &server(), borrowed, ward->id, ward->version, upload, _1)).error,
             ServerError::InvalidRequest);

    QCOMPARE(headOf(server(), ward->id).value->version, 2);

    // The same upload, signed as it is sent, lands.
    const Result<qint64> landed = uploadAs(server(), ward->bob, ward->id, ward->version, upload);
    QVERIFY(landed.ok());
    QCOMPARE(landed.value.value(), 3);
}

void ServerConformanceTest::writesAreConditionalOnTheStartingVersion()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(publish(server(), ward.value(), ward->alice, {ward->alice}), ServerError::None);
    QCOMPARE(ward->version, 2);

    // An upload built on version 1 after version 2 landed.
    const Result<qint64> stale = uploadAs(server(), ward->alice, ward->id, 1, uploadFor(ward->id, {ward->alice}, 9));
    QCOMPARE(stale.error, ServerError::VersionConflict);
    QCOMPARE(headOf(server(), ward->id).value->version, 2);
}

void ServerConformanceTest::aDownloadIsOneConsistentSnapshot()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const Upload upload = uploadFor(ward->id, {ward->alice, ward->bob}, 7);
    const Result<qint64> version = uploadAs(server(), ward->bob, ward->id, ward->version, upload);
    QVERIFY(version.ok());
    QCOMPARE(version.value.value(), 3);

    const Result<Snapshot> snapshot = downloadOf(server(), ward->id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, 3);
    QCOMPARE(snapshot.value->header, upload.header.toJson());
    QCOMPARE(snapshot.value->blob, upload.blob);
    QCOMPARE(snapshot.value->published.device, ward->bob.fingerprint());
}

// --- The lease -------------------------------------------------------------------

void ServerConformanceTest::theLeaseIsHeldByOneDeviceAtATime()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    const Result<Lease> taken = takeLeaseAs(server(), ward->alice, ward->id);
    QVERIFY(taken.ok());
    QCOMPARE(taken.value->holder, ward->alice.fingerprint());
    QCOMPARE(takeLeaseAs(server(), ward->bob, ward->id).error, ServerError::LeaseHeld);
    QCOMPARE(releaseLeaseAs(server(), ward->bob, ward->id).error, ServerError::LeaseHeld);
    QCOMPARE(headOf(server(), ward->id).value->leaseHolder, std::optional<Fingerprint>(ward->alice.fingerprint()));

    // Refreshed by its holder; taken by another once released.
    QVERIFY(takeLeaseAs(server(), ward->alice, ward->id).ok());
    QVERIFY(releaseLeaseAs(server(), ward->alice, ward->id).ok());
    QVERIFY(!headOf(server(), ward->id).value->leaseHolder.has_value());
    const Result<Lease> bobs = takeLeaseAs(server(), ward->bob, ward->id);
    QVERIFY(bobs.ok());
    QCOMPARE(bobs.value->holder, ward->bob.fingerprint());
}

void ServerConformanceTest::onlyAKeyHolderTakesOrReleasesTheLease()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const TestDevice stranger = newDevice("Stranger");

    QCOMPARE(takeLeaseAs(server(), stranger, ward->id).error, ServerError::NotAKeyHolder);
    QCOMPARE(releaseLeaseAs(server(), stranger, ward->id).error, ServerError::NotAKeyHolder);
    QVERIFY(!headOf(server(), ward->id).value->leaseHolder.has_value());

    // A lease call signed over another document's id.
    Signer misdirected = ward->carol.signer(signedBytes(CallTakeLease, newDocumentId()));
    QCOMPARE(wait<Lease>(std::bind(&ServerApi::takeLease, &server(), misdirected, ward->id, _1)).error,
             ServerError::InvalidRequest);

    QVERIFY(takeLeaseAs(server(), ward->carol, ward->id).ok());
    QCOMPARE(headOf(server(), ward->id).value->leaseHolder, std::optional<Fingerprint>(ward->carol.fingerprint()));
}

void ServerConformanceTest::aLeaseExpires()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(takeLeaseAs(server(), ward->alice, ward->id).ok());
    QCOMPARE(takeLeaseAs(server(), ward->bob, ward->id).error, ServerError::LeaseHeld);

    QVERIFY(advanceClock(server(), ThreeMinutes));
    QVERIFY(!headOf(server(), ward->id).value->leaseHolder.has_value());
    QVERIFY(takeLeaseAs(server(), ward->bob, ward->id).ok());
}
