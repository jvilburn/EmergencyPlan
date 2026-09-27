#include "ServerConformanceTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ChainTestSupport;
using namespace ServerContract;
using namespace ServerTestSupport;

namespace
{

constexpr qint64 OneHour = 60 * 60;
constexpr qint64 OneDay = 24 * OneHour;

/// For a call the test expects to succeed. A failed call is recorded against the
/// running test and yields an empty value, so against a real server one failure
/// ends one test rather than throwing out of the whole runner.
/// A request with an empty body - taking or releasing the lease - signed by this
/// device for this endpoint and document, for tests that vary one of the three.
QByteArray emptyBodySignature(const TestDevice& device, Endpoint endpoint, const QString& documentId)
{
    return signRequest(endpoint, documentId, emptyBody(), device.signing.signingKey);
}

template<typename T>
T valueOf(const Result<T>& result, const char* file, int line)
{
    if (!result.ok())
    {
        QTest::qFail(qPrintable(QString("call failed with ServerError %1").arg(int(result.error))),
                     file,
                     line);
        return T();
    }
    return result.value.value();
}

} // namespace

ServerConformanceTest::ServerConformanceTest(
    std::function<std::unique_ptr<ServerContract::ServerApi>()> makeServer)
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

// --- Authentication ----------------------------------------------------------

void ServerConformanceTest::authenticatesASignedChallenge()
{
    const TestDevice device = newPerson();

    const std::optional<Session> session = signIn(server(), device);
    QVERIFY(session.has_value());
    QVERIFY(server().myDocuments(session.value()).ok());
}

void ServerConformanceTest::refusesAChallengeSignedUnderAnotherTagOrKey()
{
    const TestDevice device = newPerson();
    const TestDevice other = newPerson();

    // A challenge the server chose, signed as if it were a header: the tag is what
    // stops login bytes standing in for document evidence, and the reverse.
    const QByteArray first = valueOf(server().challenge(), __FILE__, __LINE__);
    const QByteArray asHeader =
        DocumentCrypto::sign(SignatureTag::Header, first, device.signing.signingKey);
    QCOMPARE(server().authenticate(device.signing.verifyKey, first, asHeader).error,
             ServerError::Unauthenticated);

    // Someone else's key, claiming this device.
    const QByteArray second = valueOf(server().challenge(), __FILE__, __LINE__);
    const QByteArray byOther = DocumentCrypto::sign(SignatureTag::Auth, second, other.signing.signingKey);
    QCOMPARE(server().authenticate(device.signing.verifyKey, second, byOther).error,
             ServerError::Unauthenticated);
}

void ServerConformanceTest::aChallengeIsGoodForOneLogin()
{
    const TestDevice device = newPerson();

    const QByteArray challenge = valueOf(server().challenge(), __FILE__, __LINE__);
    const QByteArray signature =
        DocumentCrypto::sign(SignatureTag::Auth, challenge, device.signing.signingKey);

    QVERIFY(server().authenticate(device.signing.verifyKey, challenge, signature).ok());
    QCOMPARE(server().authenticate(device.signing.verifyKey, challenge, signature).error,
             ServerError::Unauthenticated);
}

void ServerConformanceTest::anExpiredSessionIsUnauthenticated()
{
    const TestDevice device = newPerson();
    const std::optional<Session> session = signIn(server(), device);
    QVERIFY(session.has_value());

    QVERIFY(server().advanceClock(OneDay).ok());
    QCOMPARE(server().myDocuments(session.value()).error, ServerError::Unauthenticated);
}

void ServerConformanceTest::aMutatingRequestMustBeSignedByTheCallingDevice()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // A session token alone changes nothing.
    QCOMPARE(server().takeLease(w.aliceSession, w.id, QByteArray()).error, ServerError::Unauthenticated);

    // Signed, but by another member: a token taken from Alice is not enough even
    // with a key of Bob's.
    const QByteArray byBob = emptyBodySignature(w.bob, Endpoint::TakeLease, w.id);
    QCOMPARE(server().takeLease(w.aliceSession, w.id, byBob).error, ServerError::Unauthenticated);

    // Alice's own signature, captured from a request to another ward.
    const QByteArray otherWard = emptyBodySignature(w.alice, Endpoint::TakeLease, newDocumentId());
    QCOMPARE(server().takeLease(w.aliceSession, w.id, otherWard).error, ServerError::Unauthenticated);

    // And from another endpoint with the same empty body.
    const QByteArray otherEndpoint = emptyBodySignature(w.alice, Endpoint::ReleaseLease, w.id);
    QCOMPARE(server().takeLease(w.aliceSession, w.id, otherEndpoint).error, ServerError::Unauthenticated);

    // The control: signed by Alice, for this ward and this endpoint.
    const QByteArray byAlice = emptyBodySignature(w.alice, Endpoint::TakeLease, w.id);
    QVERIFY(server().takeLease(w.aliceSession, w.id, byAlice).ok());
}

void ServerConformanceTest::aRequestSignatureCoversItsBody()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload upload = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);
    const QByteArray signature = signRequest(Endpoint::Upload,
                                             w.id,
                                             uploadBody(w.version, upload),
                                             w.alice.signing.signingKey);

    // The signature was for this upload built on this version; the same bytes
    // offered against another base version, or with another header, are refused.
    QCOMPARE(server().upload(w.aliceSession, w.id, w.version - 1, upload, signature).error,
             ServerError::Unauthenticated);

    const Upload other = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);
    QCOMPARE(server().upload(w.aliceSession, w.id, w.version, other, signature).error,
             ServerError::Unauthenticated);

    QVERIFY(server().upload(w.aliceSession, w.id, w.version, upload, signature).ok());
}

void ServerConformanceTest::writesOpenToNonMembersAreSignedToo()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    const TestDevice stranger = newPerson();
    const std::optional<Session> session = signIn(server(), stranger);
    QVERIFY(session.has_value());

    // Device authentication is still the device's own key on each write.
    QCOMPARE(server().requestAccess(session.value(), ward->id, requestFor(stranger, ""), QByteArray()).error,
             ServerError::Unauthenticated);
    QCOMPARE(server().startPairing(session.value(), "Phone", "a commitment", QByteArray()).error,
             ServerError::Unauthenticated);

    const Upload own = sealedUpload(
        buildHeader(newDocumentId(), 1, 1, {stranger}, stranger, DocumentCrypto::generateDataKey()),
        stranger);
    QCOMPARE(server().registerDocument(session.value(), own, QByteArray()).error,
             ServerError::Unauthenticated);

    // Each one signed goes through.
    QVERIFY(signedRequestAccess(server(), session.value(), ward->id, requestFor(stranger, "")).ok());
    QVERIFY(signedStartPairing(server(), session.value(), "Phone", "a commitment").ok());
    QVERIFY(signedRegisterDocument(server(), session.value(), own).ok());
}

// --- Registering a document --------------------------------------------------

void ServerConformanceTest::registersADocumentWithOnePerson()
{
    const std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(ward->version, qint64(1));

    const Result<Head> head = server().head(ward->aliceSession, ward->id);
    QVERIFY(head.ok());
    QVERIFY(head.value->header == ward->current);
}

void ServerConformanceTest::aFirstHeaderBringsOnlyDevicesPairedWithTheRegisteringDevice()
{
    std::optional<SharedDocument> first = registerWard(server());
    QVERIFY(first.has_value());
    const TestDevice alice = first->alice;

    // A device Alice never paired with, listed under her own member id: anyone
    // who has seen a header knows its keys.
    const TestDevice unpaired = newDeviceFor(alice.member);
    const Upload withUnpaired = sealedUpload(
        buildHeader(newDocumentId(), 1, 1, {alice, unpaired}, alice, DocumentCrypto::generateDataKey()),
        alice);
    QCOMPARE(signedRegisterDocument(server(), first->aliceSession, withUnpaired).error,
             ServerError::DeviceNotInvited);
    QCOMPARE(valueOf(server().myDocuments(first->aliceSession), __FILE__, __LINE__),
             QStringList{first->id});

    // Her phone, once paired with the registering device, comes along.
    const TestDevice alicePhone = newDeviceFor(alice.member);
    const std::optional<Session> phoneSession = signIn(server(), alicePhone);
    QVERIFY(phoneSession.has_value());
    QVERIFY(completePairing(server(), alicePhone, phoneSession.value(), alice, first->aliceSession));

    const std::optional<SharedDocument> second = registerWardWith(server(), alice, {alice, alicePhone});
    QVERIFY(second.has_value());
    const Result<QStringList> phoneDocuments = server().myDocuments(phoneSession.value());
    QVERIFY(phoneDocuments.ok());
    QCOMPARE(phoneDocuments.value.value(), QStringList{second->id});
}

void ServerConformanceTest::aFirstHeaderListsExactlyOnePerson()
{
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const std::optional<Session> session = signIn(server(), alice);
    QVERIFY(session.has_value());

    // A first header is exempt from invitations, so a second person in it would
    // arrive without anyone having asked.
    const Upload twoPeople = sealedUpload(
        buildHeader(newDocumentId(), 1, 1, {alice, bob}, alice, DocumentCrypto::generateDataKey()),
        alice);
    QCOMPARE(signedRegisterDocument(server(), session.value(), twoPeople).error, ServerError::ChainRefused);
}

void ServerConformanceTest::aFirstHeaderIsWrittenAndSignedByTheRegisteringDevice()
{
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const std::optional<Session> aliceSession = signIn(server(), alice);
    const std::optional<Session> bobSession = signIn(server(), bob);
    QVERIFY(aliceSession.has_value() && bobSession.has_value());

    const DataKey key = DocumentCrypto::generateDataKey();

    Upload badSignature =
        sealedUpload(buildHeader(newDocumentId(), 1, 1, {alice}, alice, key), alice);
    badSignature.header.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\0');
    QCOMPARE(signedRegisterDocument(server(), aliceSession.value(), badSignature).error,
             ServerError::ChainRefused);

    // Written by someone the header does not list.
    const Upload strangerWrote =
        sealedUpload(buildHeader(newDocumentId(), 1, 1, {alice}, bob, key), bob);
    QCOMPARE(signedRegisterDocument(server(), bobSession.value(), strangerWrote).error,
             ServerError::ChainRefused);

    // Alice's valid first header, registered by Bob.
    const Upload alicesHeader =
        sealedUpload(buildHeader(newDocumentId(), 1, 1, {alice}, alice, key), alice);
    QCOMPARE(signedRegisterDocument(server(), bobSession.value(), alicesHeader).error,
             ServerError::InvalidRequest);
}

void ServerConformanceTest::refusesATakenDocumentId()
{
    const std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());

    // The app re-registers under a fresh id when this happens; it never takes
    // over the existing document.
    const TestDevice other = newPerson();
    const std::optional<Session> session = signIn(server(), other);
    QVERIFY(session.has_value());

    const Upload sameId = sealedUpload(
        buildHeader(ward->id, 1, 1, {other}, other, DocumentCrypto::generateDataKey()), other);
    QCOMPARE(signedRegisterDocument(server(), session.value(), sameId).error, ServerError::IdTaken);
}

void ServerConformanceTest::registrationIsRateLimited()
{
    const TestDevice device = newPerson();
    const std::optional<Session> session = signIn(server(), device);
    QVERIFY(session.has_value());

    // The limit is the server's configuration, so this asks only that there is
    // one, well before a hundred.
    bool limited = false;
    for (int attempt = 0; attempt < 100 && !limited; ++attempt)
    {
        const Upload upload = sealedUpload(
            buildHeader(newDocumentId(), 1, 1, {device}, device, DocumentCrypto::generateDataKey()),
            device);
        const Result<qint64> result = signedRegisterDocument(server(), session.value(), upload);

        if (result.error == ServerError::RateLimited)
        {
            limited = true;
        }
        else
        {
            QVERIFY(result.ok());
        }
    }

    QVERIFY(limited);
}

// --- Versions ----------------------------------------------------------------

void ServerConformanceTest::writesAreConditionalOnTheStartingVersion()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const qint64 started = w.version;
    const DocumentHeader alicesEdit = nextHeader(w, {w.alice, w.bob, w.carol}, w.alice);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, alicesEdit), ServerError::None);

    // Bob started from the same version; his publish must not land on top of
    // Alice's unseen.
    DocumentHeader bobsEdit = buildHeader(w.id,
                                          w.current.keyGeneration,
                                          w.current.saveCounter + 1,
                                          {w.alice, w.bob, w.carol},
                                          w.bob,
                                          w.dataKey);
    const Upload upload = sealedUpload(bobsEdit, w.bob);
    QCOMPARE(signedUpload(server(), w.bobSession, w.id, started, upload).error,
             ServerError::VersionConflict);
}

void ServerConformanceTest::versionAndKeyGenerationNeverDecrease()
{
    const TestDevice alice = newPerson();
    const std::optional<Session> session = signIn(server(), alice);
    QVERIFY(session.has_value());

    SharedDocument ward;
    ward.id = newDocumentId();
    ward.dataKey = DocumentCrypto::generateDataKey();
    ward.alice = alice;

    const Upload first = sealedUpload(buildHeader(ward.id, 3, 1, {alice}, alice, ward.dataKey), alice);
    QVERIFY(signedRegisterDocument(server(), session.value(), first).ok());
    ward.current = first.header;
    ward.version = 1;

    const DocumentHeader lower = buildHeader(ward.id, 2, 2, {alice}, alice, ward.dataKey);
    QCOMPARE(publish(server(), ward, session.value(), alice, lower), ServerError::ChainRefused);

    QCOMPARE(publish(server(), ward, session.value(), alice, nextHeader(ward, {alice}, alice)),
             ServerError::None);
    QCOMPARE(ward.version, qint64(2));
    QCOMPARE(publish(server(), ward, session.value(), alice, nextHeader(ward, {alice}, alice)),
             ServerError::None);
    QCOMPARE(ward.version, qint64(3));
}

void ServerConformanceTest::oneUploadRaisesTheSaveCounterOnlySoFar()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};

    // One past the step: a member pushing toward the cap.
    const DocumentHeader tooFar = buildHeader(w.id,
                                              w.current.keyGeneration,
                                              w.current.saveCounter + DocumentHeader::MaxSaveCounterStep + 1,
                                              everyone,
                                              w.alice,
                                              w.dataKey);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, tooFar), ServerError::CounterJump);

    // Exactly the step: months of offline saves published at once.
    const DocumentHeader longOffline = buildHeader(w.id,
                                                   w.current.keyGeneration,
                                                   w.current.saveCounter + DocumentHeader::MaxSaveCounterStep,
                                                   everyone,
                                                   w.alice,
                                                   w.dataKey);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, longOffline), ServerError::None);
}

void ServerConformanceTest::uploadsAreRateLimited()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};

    // The limit is the server's configuration, so this asks only that there is
    // one before two thousand - generous enough for a responder publishing
    // every few seconds, and what makes pushing the counter to its cap take
    // centuries.
    bool limited = false;
    for (int attempt = 0; attempt < 2000 && !limited; ++attempt)
    {
        const ServerError error =
            publish(server(), w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice));
        if (error == ServerError::RateLimited)
        {
            limited = true;
        }
        else
        {
            QCOMPARE(error, ServerError::None);
        }
    }

    QVERIFY(limited);

    // Another device is not held back by Alice's uploads.
    QCOMPARE(publish(server(), w, w.bobSession, w.bob, nextHeader(w, everyone, w.bob)), ServerError::None);
}

void ServerConformanceTest::thePayloadHashMustMatchTheBlob()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    Upload upload = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);
    upload.blob.append("tampered");

    QCOMPARE(signedUpload(server(), w.aliceSession, w.id, w.version, upload).error,
             ServerError::PayloadMismatch);
}

void ServerConformanceTest::theUploaderIsTheWriter()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Upload alicesVersion =
        sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, w.alice), w.alice);
    QCOMPARE(signedUpload(server(), w.bobSession, w.id, w.version, alicesVersion).error,
             ServerError::InvalidRequest);
}

// --- The allow-list ----------------------------------------------------------

void ServerConformanceTest::onlyCurrentDevicesReadOrWrite()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice stranger = newPerson();
    const std::optional<Session> session = signIn(server(), stranger);
    QVERIFY(session.has_value());

    QCOMPARE(server().head(session.value(), w.id).error, ServerError::NotAllowListed);
    QCOMPARE(server().download(session.value(), w.id).error, ServerError::NotAllowListed);
    QCOMPARE(signedTakeLease(server(), session.value(), w.id).error, ServerError::NotAllowListed);
    QCOMPARE(server().accessRequests(session.value(), w.id).error, ServerError::NotAllowListed);

    const Upload upload = sealedUpload(nextHeader(w, {w.alice, w.bob, w.carol}, stranger), stranger);
    QCOMPARE(signedUpload(server(), session.value(), w.id, w.version, upload).error,
             ServerError::NotAllowListed);
}

void ServerConformanceTest::anAcceptedUploadChangesTheAllowListAtOnce()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, removalHeader(w, {w.alice, w.carol}, w.alice)),
             ServerError::None);

    // No second step and no window: the upload that removed Bob is the moment he
    // can no longer fetch.
    QCOMPARE(server().download(w.bobSession, w.id).error, ServerError::NotAllowListed);

    const TestDevice dave = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());
    QVERIFY(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(dave, "")).ok());
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.carol, dave}, w.alice)),
             ServerError::None);

    QVERIFY(server().download(daveSession.value(), w.id).ok());
}

void ServerConformanceTest::aDownloadIsOneConsistentSnapshot()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Result<Snapshot> snapshot = server().download(w.bobSession, w.id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, w.version);
    QVERIFY(snapshot.value->header == w.current);
    QCOMPARE(DocumentCrypto::payloadHash(snapshot.value->blob), snapshot.value->header.payloadHash);
}

// --- The chain ---------------------------------------------------------------

void ServerConformanceTest::everyUploadMustExtendTheChain()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Bob's new phone has asked, so the invitation rule is satisfied - but Alice
    // is adding it to Bob's person, which only Bob may do.
    const TestDevice bobPhone = newDeviceFor(w.bob.member);
    const std::optional<Session> phoneSession = signIn(server(), bobPhone);
    QVERIFY(phoneSession.has_value());
    QVERIFY(signedRequestAccess(server(), phoneSession.value(), w.id, requestFor(bobPhone, "")).ok());

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.bob, w.carol, bobPhone}, w.alice)),
             ServerError::ChainRefused);
}

void ServerConformanceTest::keepsOnlyHeadersThatChangedMembership()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const DocumentHeader grant = w.current;
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice)), ServerError::None);

    // Carol made an admin: nobody gains or loses access, but a later removal of
    // hers is only provable from the header that gave her the role.
    const QList<MemberId> admins = {w.alice.member, w.carol.member};
    const DocumentHeader promotion = buildHeader(w.id,
                                                 w.current.keyGeneration,
                                                 w.current.saveCounter + 1,
                                                 everyone,
                                                 w.alice,
                                                 w.dataKey,
                                                 admins);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, promotion), ServerError::None);
    const DocumentHeader promoted = w.current;

    const DocumentHeader unchanged = buildHeader(w.id,
                                                 w.current.keyGeneration,
                                                 w.current.saveCounter + 1,
                                                 everyone,
                                                 w.alice,
                                                 w.dataKey,
                                                 admins);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, unchanged), ServerError::None);
    QCOMPARE(w.version, qint64(5));

    // Versions 3 and 5 changed nothing about who may open the document or who
    // administers it, so only the grant, the promotion, and the version asked for
    // are returned.
    const Result<HeaderChain> chain = server().headers(w.aliceSession, w.id, 1, 5);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.size(), qsizetype(3));
    QVERIFY(chain.value->headers.at(0) == grant);
    QVERIFY(chain.value->headers.at(1) == promoted);
    QVERIFY(chain.value->headers.at(2) == w.current);
}

void ServerConformanceTest::theChainEndsWithTheRequestedVersion()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const DocumentHeader grant = w.current;
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice)), ServerError::None);
    const DocumentHeader third = w.current;
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, everyone, w.alice)), ServerError::None);

    // Asked through version 3 while the server is at 4: the chain ends at 3, the
    // version a download fetched, whatever has been published since.
    const Result<HeaderChain> chain = server().headers(w.aliceSession, w.id, 1, 3);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.size(), qsizetype(2));
    QVERIFY(chain.value->headers.at(0) == grant);
    QVERIFY(chain.value->headers.at(1) == third);
}

void ServerConformanceTest::aFormerDeviceReadsTheChainOnlyThroughItsRemoval()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const DocumentHeader grant = w.current;
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, removalHeader(w, {w.alice, w.carol}, w.alice)),
             ServerError::None);
    const DocumentHeader removal = w.current;
    const qint64 removedAt = w.version;

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.carol}, w.alice)),
             ServerError::None);

    // Bob can fetch the evidence of his own removal and nothing written after it.
    const Result<HeaderChain> chain = server().headers(w.bobSession, w.id, 1, w.version);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.size(), qsizetype(2));
    QVERIFY(chain.value->headers.at(0) == grant);
    QVERIFY(chain.value->headers.at(1) == removal);

    const Result<HeaderChain> fromGrant = server().headers(w.bobSession, w.id, removedAt - 1, w.version);
    QVERIFY(fromGrant.ok());
    QCOMPARE(fromGrant.value->headers.size(), qsizetype(1));
    QVERIFY(fromGrant.value->headers.at(0) == removal);

    // And what he fetched is enough to convince him.
    QCOMPARE(DocumentCrypto::verifyChain(grant, {removal}, w.bob.fingerprint()), ChainResult::Removed);
}

void ServerConformanceTest::aDeviceNeverListedCannotReadTheChain()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    const TestDevice stranger = newPerson();
    const std::optional<Session> session = signIn(server(), stranger);
    QVERIFY(session.has_value());

    QCOMPARE(server().headers(session.value(), ward->id, 1, ward->version).error,
             ServerError::NotAllowListed);
}

// --- Members and devices joining ---------------------------------------------

void ServerConformanceTest::aDeviceJoinsOnlyByRequest()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Anyone who has seen a header knows Dave's keys. Without this rule, a
    // look-alike ward could add him and read what he typed into it.
    const TestDevice dave = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());

    const QList<TestDevice> withDave = {w.alice, w.bob, w.carol, dave};
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, withDave, w.alice)),
             ServerError::DeviceNotInvited);

    QVERIFY(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(dave, "")).ok());
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, withDave, w.alice)),
             ServerError::None);

    const Result<RequestStatus> status = server().myRequestStatus(daveSession.value(), w.id);
    QVERIFY(status.ok());
    QCOMPARE(status.value.value(), RequestStatus::Granted);
}

void ServerConformanceTest::aDeviceRequestsOnlyForItself()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice dave = newPerson();
    const TestDevice eve = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());

    // Dave asking on Eve's behalf would let him enrol a device he does not hold.
    QCOMPARE(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(eve, "")).error,
             ServerError::InvalidRequest);

    // A current member has nothing to ask for.
    QCOMPARE(signedRequestAccess(server(), w.bobSession, w.id, requestFor(w.bob, "")).error,
             ServerError::InvalidRequest);
}

void ServerConformanceTest::aCompletedPairingLetsTheResponderLinkTheInitiator()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice alicePhone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), alicePhone);
    QVERIFY(phoneSession.has_value());
    QVERIFY(completePairing(server(), alicePhone, phoneSession.value(), w.alice, w.aliceSession));

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, alicePhone, w.bob, w.carol}, w.alice)),
             ServerError::None);
}

void ServerConformanceTest::aPairingPermitsOnlyThePersonOfTheResponder()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // The phone paired with Bob. Alice adding it to herself is within her own
    // person, so the chain allows it - the server does not, because no device of
    // Alice's vouched for it.
    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());
    QVERIFY(completePairing(server(), phone, phoneSession.value(), w.bob, w.bobSession));

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, phone, w.bob, w.carol}, w.alice)),
             ServerError::DeviceNotInvited);
}

void ServerConformanceTest::aPairingPermitHoldsInEveryWardOfThatPerson()
{
    std::optional<SharedDocument> first = wardOfThree(server());
    QVERIFY(first.has_value());

    std::optional<SharedDocument> second =
        registerWardWith(server(), first->alice, {first->alice});
    QVERIFY(second.has_value());

    // Pair once; the queue then carries the link to every ward, however long that
    // takes.
    const TestDevice alicePhone = newDeviceFor(first->alice.member);
    const std::optional<Session> phoneSession = signIn(server(), alicePhone);
    QVERIFY(phoneSession.has_value());
    QVERIFY(completePairing(server(), alicePhone, phoneSession.value(), first->alice, first->aliceSession));

    SharedDocument& a = first.value();
    SharedDocument& b = second.value();
    QCOMPARE(publish(server(), a, a.aliceSession, a.alice, nextHeader(a, {a.alice, alicePhone, a.bob, a.carol}, a.alice)),
             ServerError::None);
    QCOMPARE(publish(server(), b, b.aliceSession, b.alice, nextHeader(b, {b.alice, alicePhone}, b.alice)),
             ServerError::None);
}

void ServerConformanceTest::anUnfinishedPairingPermitsNothing()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice alicePhone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), alicePhone);
    QVERIFY(phoneSession.has_value());

    // Every step but the handover: the responder never vouched for it.
    const Result<QString> code = signedStartPairing(server(), phoneSession.value(), "Phone", "a commitment");
    QVERIFY(code.ok());
    QVERIFY(server().pairing(w.aliceSession, code.value.value()).ok());
    QVERIFY(signedRespondToPairing(server(), w.aliceSession, code.value.value(), keysOf(w.alice)).ok());
    QVERIFY(signedRevealPairing(server(), phoneSession.value(), code.value.value(), keysOf(alicePhone), "a nonce").ok());

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, alicePhone, w.bob, w.carol}, w.alice)),
             ServerError::DeviceNotInvited);
}

// --- Removals and leaving ----------------------------------------------------

void ServerConformanceTest::anAdminRemovesAPersonAtOnce()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Bob arrived today. An admin's word is enough, with no one else to agree
    // and nothing to wait for.
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, removalHeader(w, {w.alice, w.carol}, w.alice)),
             ServerError::None);
    QCOMPARE(server().download(w.bobSession, w.id).error, ServerError::NotAllowListed);
}

void ServerConformanceTest::aMemberCannotRemoveAnyone()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    QCOMPARE(publish(server(), w, w.carolSession, w.carol, removalHeader(w, {w.alice, w.carol}, w.carol)),
             ServerError::ChainRefused);
    QVERIFY(server().download(w.bobSession, w.id).ok());
}

void ServerConformanceTest::aMemberLeavesByWritingTheirOwnLeave()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Bob, a member, uploads the version that drops him. He is the writer and
    // no longer in it.
    QCOMPARE(publish(server(), w, w.bobSession, w.bob, nextHeader(w, {w.alice, w.carol}, w.bob)),
             ServerError::None);
    QCOMPARE(server().download(w.bobSession, w.id).error, ServerError::NotAllowListed);
    QVERIFY(server().download(w.carolSession, w.id).ok());
}

void ServerConformanceTest::aRemovedPersonCanProveARemovalByAnAdminPromotedSince()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();
    const DocumentHeader anchor = w.current;
    const qint64 anchorVersion = w.version;

    // Alice makes Carol an admin, and Carol removes Bob. Bob's anchor predates
    // Carol's role, so the chain must carry the promotion for him to accept it.
    const QList<TestDevice> everyone = {w.alice, w.bob, w.carol};
    const DocumentHeader promotion = buildHeader(w.id,
                                                 w.current.keyGeneration,
                                                 w.current.saveCounter + 1,
                                                 everyone,
                                                 w.alice,
                                                 w.dataKey,
                                                 {w.alice.member, w.carol.member});
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, promotion), ServerError::None);

    const DocumentHeader removal = buildHeader(w.id,
                                               w.current.keyGeneration + 1,
                                               w.current.saveCounter + 1,
                                               {w.alice, w.carol},
                                               w.carol,
                                               DocumentCrypto::generateDataKey(),
                                               {w.alice.member, w.carol.member});
    QCOMPARE(publish(server(), w, w.carolSession, w.carol, removal), ServerError::None);

    const Result<HeaderChain> chain = server().headers(w.bobSession, w.id, anchorVersion, w.version);
    QVERIFY(chain.ok());
    QCOMPARE(chain.value->headers.size(), qsizetype(2));
    QCOMPARE(DocumentCrypto::verifyChain(anchor, chain.value->headers, w.bob.fingerprint()),
             ChainResult::Removed);
}

void ServerConformanceTest::aLastPersonsLeaveEndsTheDocument()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();
    const qint64 before = w.version;

    // Alice, alone, leaves: a version that lists no one and has no admin.
    const DocumentHeader leave = buildHeader(w.id,
                                             w.current.keyGeneration,
                                             w.current.saveCounter + 1,
                                             {},
                                             w.alice,
                                             w.dataKey,
                                             {});
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, leave), ServerError::None);
    QCOMPARE(w.version, before + 1);

    // Listed for nobody, readable by nobody, and nothing can follow it.
    const Result<QStringList> documents = server().myDocuments(w.aliceSession);
    QVERIFY(documents.ok());
    QVERIFY(!documents.value->contains(w.id));
    QCOMPARE(server().download(w.aliceSession, w.id).error, ServerError::NotAllowListed);

    const Upload afterwards = sealedUpload(nextHeader(w, {w.alice}, w.alice), w.alice);
    QCOMPARE(signedUpload(server(), w.aliceSession, w.id, w.version, afterwards).error,
             ServerError::NotAllowListed);
}

void ServerConformanceTest::aPersonWhoLeftOrWasRemovedIsGrantedAgainUnderTheSameMemberId()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Bob leaves; Alice removes Carol.
    QCOMPARE(publish(server(), w, w.bobSession, w.bob, nextHeader(w, {w.alice, w.carol}, w.bob)),
             ServerError::None);
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, removalHeader(w, {w.alice}, w.alice)),
             ServerError::None);

    // Each asks again from the same device, under the same member id, and is let back in.
    QVERIFY(signedRequestAccess(server(), w.bobSession, w.id, requestFor(w.bob, "")).ok());
    QVERIFY(signedRequestAccess(server(), w.carolSession, w.id, requestFor(w.carol, "")).ok());
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.bob, w.carol}, w.alice)),
             ServerError::None);

    QVERIFY(server().download(w.bobSession, w.id).ok());
    QVERIFY(server().download(w.carolSession, w.id).ok());
}

// --- Access requests ---------------------------------------------------------

void ServerConformanceTest::requestStatusIsReadableByItsDevice()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice dave = newPerson();
    const TestDevice eve = newPerson();
    const TestDevice frank = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    const std::optional<Session> eveSession = signIn(server(), eve);
    const std::optional<Session> frankSession = signIn(server(), frank);
    QVERIFY(daveSession.has_value() && eveSession.has_value() && frankSession.has_value());

    QVERIFY(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(dave, "")).ok());
    QVERIFY(signedRequestAccess(server(), eveSession.value(), w.id, requestFor(eve, "")).ok());
    QCOMPARE(valueOf(server().myRequestStatus(daveSession.value(), w.id), __FILE__, __LINE__),
             RequestStatus::Pending);

    QVERIFY(signedClearAccessRequest(server(), w.aliceSession, w.id, eve.fingerprint()).ok());
    QCOMPARE(valueOf(server().myRequestStatus(eveSession.value(), w.id), __FILE__, __LINE__),
             RequestStatus::Withdrawn);

    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, nextHeader(w, {w.alice, w.bob, w.carol, dave}, w.alice)),
             ServerError::None);
    QCOMPARE(valueOf(server().myRequestStatus(daveSession.value(), w.id), __FILE__, __LINE__),
             RequestStatus::Granted);

    QCOMPARE(server().myRequestStatus(frankSession.value(), w.id).error, ServerError::NotFound);
}

void ServerConformanceTest::accessRequestsAreRateLimited()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    const TestDevice stranger = newPerson();
    const std::optional<Session> session = signIn(server(), stranger);
    QVERIFY(session.has_value());

    // The one write open to a non-member. The limit is the server's configuration,
    // so this asks only that there is one, well before a hundred.
    bool limited = false;
    for (int attempt = 0; attempt < 100 && !limited; ++attempt)
    {
        const Result<Done> result =
            signedRequestAccess(server(), session.value(), ward->id, requestFor(stranger, ""));
        if (result.error == ServerError::RateLimited)
        {
            limited = true;
        }
        else
        {
            QVERIFY(result.ok());
        }
    }

    QVERIFY(limited);
}

void ServerConformanceTest::accessRequestsCarryTheirPairingCodeToMembers()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice dave = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());
    QVERIFY(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(dave, "48219301")).ok());

    const Result<QList<AccessRequest>> requests = server().accessRequests(w.bobSession, w.id);
    QVERIFY(requests.ok());
    QCOMPARE(requests.value->size(), qsizetype(1));

    const AccessRequest& request = requests.value->first();
    QCOMPARE(request.pairingCode, QString("48219301"));
    QVERIFY(request.member == dave.member);
    QVERIFY(request.device.fingerprint() == dave.fingerprint());
    QCOMPARE(request.displayName, QString("Requesting Person"));
}

// --- Authentication levels ---------------------------------------------------

void ServerConformanceTest::deviceAuthenticationReachesOnlyItsEndpoints()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice stranger = newPerson();
    const std::optional<Session> session = signIn(server(), stranger);
    QVERIFY(session.has_value());

    // What a device does before it is a member.
    QVERIFY(server().myDocuments(session.value()).ok());
    QVERIFY(signedRequestAccess(server(), session.value(), w.id, requestFor(stranger, "")).ok());
    QVERIFY(server().myRequestStatus(session.value(), w.id).ok());
    QVERIFY(signedStartPairing(server(), session.value(), "Phone", "a commitment").ok());

    // Everything else needs membership.
    QCOMPARE(server().head(session.value(), w.id).error, ServerError::NotAllowListed);
    QCOMPARE(server().download(session.value(), w.id).error, ServerError::NotAllowListed);
    QCOMPARE(server().accessRequests(session.value(), w.id).error, ServerError::NotAllowListed);
}

void ServerConformanceTest::myDocumentsListsOnlyDocumentsTheDeviceIsIn()
{
    std::optional<SharedDocument> first = wardOfThree(server());
    QVERIFY(first.has_value());
    std::optional<SharedDocument> second = registerWardWith(server(), first->alice, {first->alice});
    QVERIFY(second.has_value());
    SharedDocument& a = first.value();

    QStringList both = {a.id, second->id};
    both.sort();
    QCOMPARE(valueOf(server().myDocuments(a.aliceSession), __FILE__, __LINE__), both);
    QCOMPARE(valueOf(server().myDocuments(a.bobSession), __FILE__, __LINE__), QStringList{a.id});

    // Asking is not joining.
    const TestDevice dave = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());
    QVERIFY(signedRequestAccess(server(), daveSession.value(), a.id, requestFor(dave, "")).ok());
    QVERIFY(valueOf(server().myDocuments(daveSession.value()), __FILE__, __LINE__).isEmpty());

    // And leaving takes a ward off the list.
    QCOMPARE(publish(server(), a, a.bobSession, a.bob, nextHeader(a, {a.alice, a.carol}, a.bob)),
             ServerError::None);
    QVERIFY(valueOf(server().myDocuments(a.bobSession), __FILE__, __LINE__).isEmpty());
}

// --- Pairings ----------------------------------------------------------------

void ServerConformanceTest::pairingStepsAreWrittenOnceAndInOrder()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());

    const QString code =
        valueOf(signedStartPairing(server(), phoneSession.value(), "Phone", "a commitment"), __FILE__, __LINE__);

    // The initiator reveals only once it holds the responder's keys, and the
    // responder hands over only once it has checked the reveal.
    QCOMPARE(signedRevealPairing(server(), phoneSession.value(), code, keysOf(phone), "a nonce").error,
             ServerError::OutOfOrder);

    QVERIFY(signedRespondToPairing(server(), w.aliceSession, code, keysOf(w.alice)).ok());
    QCOMPARE(signedRespondToPairing(server(), w.bobSession, code, keysOf(w.bob)).error,
             ServerError::AlreadyWritten);

    QCOMPARE(signedHandOver(server(), w.aliceSession, code, "a signed handover").error, ServerError::OutOfOrder);

    QVERIFY(signedRevealPairing(server(), phoneSession.value(), code, keysOf(phone), "a nonce").ok());
    QCOMPARE(signedRevealPairing(server(), phoneSession.value(), code, keysOf(phone), "another nonce").error,
             ServerError::AlreadyWritten);

    QVERIFY(signedHandOver(server(), w.aliceSession, code, "a signed handover").ok());
    QCOMPARE(signedHandOver(server(), w.aliceSession, code, "another handover").error,
             ServerError::AlreadyWritten);
}

void ServerConformanceTest::pairingKeysMustBeTheCallersOwn()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());

    const QString code =
        valueOf(signedStartPairing(server(), phoneSession.value(), "Phone", "a commitment"), __FILE__, __LINE__);

    QCOMPARE(signedRespondToPairing(server(), w.aliceSession, code, keysOf(w.bob)).error,
             ServerError::InvalidRequest);
    QVERIFY(signedRespondToPairing(server(), w.aliceSession, code, keysOf(w.alice)).ok());

    const TestDevice other = newPerson();
    QCOMPARE(signedRevealPairing(server(), phoneSession.value(), code, keysOf(other), "a nonce").error,
             ServerError::InvalidRequest);

    // A member cannot pair with itself either.
    const QString own =
        valueOf(signedStartPairing(server(), w.carolSession, "Carol", "a commitment"), __FILE__, __LINE__);
    QCOMPARE(signedRespondToPairing(server(), w.carolSession, own, keysOf(w.carol)).error,
             ServerError::InvalidRequest);
}

void ServerConformanceTest::aPairingIsReadableOnlyByItsParties()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice phone = newDeviceFor(w.alice.member);
    const TestDevice stranger = newPerson();
    const std::optional<Session> phoneSession = signIn(server(), phone);
    const std::optional<Session> strangerSession = signIn(server(), stranger);
    QVERIFY(phoneSession.has_value() && strangerSession.has_value());

    const QString code =
        valueOf(signedStartPairing(server(), phoneSession.value(), "Phone", "a commitment"), __FILE__, __LINE__);

    // Until someone responds, the responder-to-be must read the commitment to keep
    // it, so any member may; a non-member may not.
    QCOMPARE(server().pairing(strangerSession.value(), code).error, ServerError::NotAllowListed);
    QVERIFY(server().pairing(w.carolSession, code).ok());

    QVERIFY(signedRespondToPairing(server(), w.aliceSession, code, keysOf(w.alice)).ok());

    QCOMPARE(server().pairing(w.carolSession, code).error, ServerError::NotAllowListed);
    QVERIFY(server().pairing(w.aliceSession, code).ok());
    QVERIFY(server().pairing(phoneSession.value(), code).ok());
}

void ServerConformanceTest::aPairingExpires()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());

    const QString code =
        valueOf(signedStartPairing(server(), phoneSession.value(), "Phone", "a commitment"), __FILE__, __LINE__);

    QVERIFY(advance(server(), w, 11 * 60));
    QCOMPARE(server().pairing(w.aliceSession, code).error, ServerError::Expired);
    QCOMPARE(signedRespondToPairing(server(), w.aliceSession, code, keysOf(w.alice)).error, ServerError::Expired);
}

// --- Commits -----------------------------------------------------------------

void ServerConformanceTest::aRefusedUploadChangesNothing()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const TestDevice dave = newPerson();
    const TestDevice eve = newPerson();
    const std::optional<Session> daveSession = signIn(server(), dave);
    QVERIFY(daveSession.has_value());
    QVERIFY(signedRequestAccess(server(), daveSession.value(), w.id, requestFor(dave, "")).ok());

    // One upload that removes Bob and grants Dave, whose request it would
    // consume, and Eve, who never asked. The chain accepts all of it; Eve is
    // refused, so none of it may half-happen: Bob stays, Dave's request stays
    // pending, and the version stays put.
    const qint64 before = w.version;
    QCOMPARE(publish(server(), w, w.aliceSession, w.alice, removalHeader(w, {w.alice, w.carol, dave, eve}, w.alice)),
             ServerError::DeviceNotInvited);

    QCOMPARE(valueOf(server().head(w.aliceSession, w.id), __FILE__, __LINE__).version, before);
    QCOMPARE(valueOf(server().myRequestStatus(daveSession.value(), w.id), __FILE__, __LINE__),
             RequestStatus::Pending);
    QCOMPARE(server().download(daveSession.value(), w.id).error, ServerError::NotAllowListed);
    QVERIFY(server().download(w.bobSession, w.id).ok());
}

// --- The lease ---------------------------------------------------------------

void ServerConformanceTest::theLeaseIsHeldByOneDeviceAtATime()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    const Result<Lease> alices = signedTakeLease(server(), w.aliceSession, w.id);
    QVERIFY(alices.ok());
    QVERIFY(alices.value->holder == w.alice.fingerprint());

    QCOMPARE(signedTakeLease(server(), w.bobSession, w.id).error, ServerError::LeaseHeld);
    QVERIFY(valueOf(server().head(w.bobSession, w.id), __FILE__, __LINE__).leaseHolder == w.alice.fingerprint());

    // The holder refreshes it; nobody else can release it.
    QVERIFY(signedTakeLease(server(), w.aliceSession, w.id).ok());
    QCOMPARE(signedReleaseLease(server(), w.bobSession, w.id).error, ServerError::LeaseHeld);

    QVERIFY(signedReleaseLease(server(), w.aliceSession, w.id).ok());
    QVERIFY(!valueOf(server().head(w.bobSession, w.id), __FILE__, __LINE__).leaseHolder.has_value());
    QVERIFY(signedTakeLease(server(), w.bobSession, w.id).ok());
}

void ServerConformanceTest::aLeaseExpires()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // A holder that vanishes does not lock the directory for good.
    QVERIFY(signedTakeLease(server(), w.aliceSession, w.id).ok());
    QVERIFY(advance(server(), w, OneHour));
    QVERIFY(signedTakeLease(server(), w.bobSession, w.id).ok());
}
