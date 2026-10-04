#include "ServerTestSupport.h"

#include "DocumentCrypto.h"
#include "Pairing.h"

#include <QHash>

using namespace ChainTestSupport;
using namespace ServerContract;

namespace
{

/// Session tokens are random and fresh per sign-in, so one table serves every
/// server a run creates.
QHash<QString, TestDevice>& signers()
{
    static QHash<QString, TestDevice> table;
    return table;
}

QByteArray signAs(const Session& session, Endpoint endpoint, const QString& documentId, const QByteArray& body)
{
    const SigningKey& key = ServerTestSupport::signerOf(session).signing.signingKey;
    return signRequest(endpoint, session, documentId, body, key);
}

} // namespace

namespace ServerTestSupport
{

std::optional<Session> signIn(ServerApi& server, const TestDevice& device)
{
    const Result<QByteArray> challenge =
        wait<QByteArray>([&](Reply<QByteArray> reply) { server.challenge(reply); });
    if (!challenge.ok())
    {
        return std::nullopt;
    }

    const QByteArray signature = DocumentCrypto::sign(SignatureTag::Auth,
                                                      challenge.value.value(),
                                                      device.signing.signingKey);
    const std::optional<Session> session =
        wait<Session>([&](Reply<Session> reply)
        {
            server.authenticate(device.signing.verifyKey, challenge.value.value(), signature, reply);
        }).value;
    if (session.has_value())
    {
        signers().insert(session.value().token, device);
    }
    return session;
}

const TestDevice& signerOf(const Session& session)
{
    const auto found = signers().constFind(session.token);
    if (found == signers().constEnd())
    {
        qFatal("signerOf: a session that did not come from signIn");
    }
    return found.value();
}

Result<qint64> signedRegisterDocument(ServerApi& server, const Session& session, const Upload& upload)
{
    const QByteArray signature =
        signAs(session, Endpoint::RegisterDocument, upload.header.documentId, registerBody(upload));
    return wait<qint64>([&](Reply<qint64> reply)
    {
        server.registerDocument(session, upload, signature, reply);
    });
}

Result<qint64> signedUpload(ServerApi& server,
                            const Session& session,
                            const QString& documentId,
                            qint64 baseVersion,
                            const Upload& upload)
{
    const QByteArray signature = signAs(session, Endpoint::Upload, documentId, uploadBody(baseVersion, upload));
    return wait<qint64>([&](Reply<qint64> reply)
    {
        server.upload(session, documentId, baseVersion, upload, signature, reply);
    });
}

Result<Lease> signedTakeLease(ServerApi& server, const Session& session, const QString& documentId)
{
    const QByteArray signature = signAs(session, Endpoint::TakeLease, documentId, emptyBody());
    return wait<Lease>([&](Reply<Lease> reply) { server.takeLease(session, documentId, signature, reply); });
}

Result<Done> signedReleaseLease(ServerApi& server, const Session& session, const QString& documentId)
{
    const QByteArray signature = signAs(session, Endpoint::ReleaseLease, documentId, emptyBody());
    return wait<Done>([&](Reply<Done> reply) { server.releaseLease(session, documentId, signature, reply); });
}

Result<Done> signedRequestAccess(ServerApi& server,
                                 const Session& session,
                                 const QString& documentId,
                                 const AccessRequest& request)
{
    const QByteArray signature =
        signAs(session, Endpoint::RequestAccess, documentId, requestAccessBody(request));
    return wait<Done>([&](Reply<Done> reply)
    {
        server.requestAccess(session, documentId, request, signature, reply);
    });
}

Result<Done> signedClearAccessRequest(ServerApi& server,
                                      const Session& session,
                                      const QString& documentId,
                                      const Fingerprint& device)
{
    const QByteArray signature =
        signAs(session, Endpoint::ClearAccessRequest, documentId, clearAccessRequestBody(device));
    return wait<Done>([&](Reply<Done> reply)
    {
        server.clearAccessRequest(session, documentId, device, signature, reply);
    });
}

Result<QString> signedStartPairing(ServerApi& server,
                                   const Session& session,
                                   const QString& deviceName,
                                   const QByteArray& commitment)
{
    const QByteArray signature =
        signAs(session, Endpoint::StartPairing, QString(), startPairingBody(deviceName, commitment));
    return wait<QString>([&](Reply<QString> reply)
    {
        server.startPairing(session, deviceName, commitment, signature, reply);
    });
}

Result<Done> signedRespondToPairing(ServerApi& server,
                                    const Session& session,
                                    const QString& code,
                                    const DeviceKeys& responder)
{
    const QByteArray signature =
        signAs(session, Endpoint::RespondToPairing, QString(), respondToPairingBody(code, responder));
    return wait<Done>([&](Reply<Done> reply)
    {
        server.respondToPairing(session, code, responder, signature, reply);
    });
}

Result<Done> signedRevealPairing(ServerApi& server,
                                 const Session& session,
                                 const QString& code,
                                 const DeviceKeys& initiator,
                                 const QByteArray& nonce)
{
    const QByteArray signature =
        signAs(session, Endpoint::RevealPairing, QString(), revealPairingBody(code, initiator, nonce));
    return wait<Done>([&](Reply<Done> reply)
    {
        server.revealPairing(session, code, initiator, nonce, signature, reply);
    });
}

Result<Done> signedHandOver(ServerApi& server,
                            const Session& session,
                            const QString& code,
                            const QByteArray& handover)
{
    const QByteArray signature = signAs(session, Endpoint::HandOver, QString(), handOverBody(code, handover));
    return wait<Done>([&](Reply<Done> reply) { server.handOver(session, code, handover, signature, reply); });
}

DeviceKeys keysOf(const TestDevice& device)
{
    DeviceKeys keys;
    keys.publicKey = device.sealing.publicKey;
    keys.verifyKey = device.signing.verifyKey;
    return keys;
}

Upload sealedUpload(DocumentHeader header, const TestDevice& writer)
{
    Upload upload;
    upload.blob = DocumentCrypto::generateNonce() + QByteArray("opaque to the server");
    header.payloadHash = DocumentCrypto::payloadHash(upload.blob);
    resign(header, writer);
    upload.header = header;
    return upload;
}

AccessRequest requestFor(const TestDevice& device, const QString& pairingCode)
{
    AccessRequest request;
    request.member = device.member;
    request.displayName = "Requesting Person";
    request.device = rosterDevice(device, "Laptop");
    request.pairingCode = pairingCode;
    return request;
}

bool signInAll(ServerApi& server, SharedDocument& ward)
{
    const std::optional<Session> alice = signIn(server, ward.alice);
    const std::optional<Session> bob = signIn(server, ward.bob);
    const std::optional<Session> carol = signIn(server, ward.carol);
    if (!alice.has_value() || !bob.has_value() || !carol.has_value())
    {
        return false;
    }

    ward.aliceSession = alice.value();
    ward.bobSession = bob.value();
    ward.carolSession = carol.value();
    return true;
}

bool advance(ServerApi& server, SharedDocument& ward, qint64 seconds)
{
    return wait<Done>([&](Reply<Done> reply) { server.advanceClock(seconds, reply); }).ok()
        && signInAll(server, ward);
}

std::optional<SharedDocument> registerWardWith(ServerApi& server,
                                               const TestDevice& alice,
                                               const QList<TestDevice>& aliceDevices)
{
    SharedDocument ward;
    ward.id = newDocumentId();
    ward.dataKey = DocumentCrypto::generateDataKey();
    ward.alice = alice;
    ward.bob = newPerson();
    ward.carol = newPerson();

    if (!signInAll(server, ward))
    {
        return std::nullopt;
    }

    const Upload upload =
        sealedUpload(buildHeader(ward.id, 1, 1, aliceDevices, ward.alice, ward.dataKey), ward.alice);

    const Result<qint64> result = signedRegisterDocument(server, ward.aliceSession, upload);
    if (!result.ok())
    {
        return std::nullopt;
    }

    ward.current = upload.header;
    ward.version = result.value.value();
    return ward;
}

std::optional<SharedDocument> registerWard(ServerApi& server)
{
    const TestDevice alice = newPerson();
    return registerWardWith(server, alice, {alice});
}

DocumentHeader nextHeader(const SharedDocument& ward,
                          const QList<TestDevice>& devices,
                          const TestDevice& writer)
{
    return buildHeader(ward.id,
                       ward.current.keyGeneration,
                       ward.current.saveCounter + 1,
                       devices,
                       writer,
                       ward.dataKey);
}

DocumentHeader removalHeader(const SharedDocument& ward,
                             const QList<TestDevice>& devices,
                             const TestDevice& writer)
{
    return buildHeader(ward.id,
                       ward.current.keyGeneration + 1,
                       ward.current.saveCounter + 1,
                       devices,
                       writer,
                       DocumentCrypto::generateDataKey());
}

ServerError publish(ServerApi& server,
                    SharedDocument& ward,
                    const Session& session,
                    const TestDevice& writer,
                    const DocumentHeader& header)
{
    const Upload upload = sealedUpload(header, writer);
    const Result<qint64> result = signedUpload(server, session, ward.id, ward.version, upload);
    if (result.ok())
    {
        ward.version = result.value.value();
        ward.current = upload.header;
    }
    return result.error;
}

std::optional<SharedDocument> wardOfThree(ServerApi& server)
{
    std::optional<SharedDocument> ward = registerWard(server);
    if (!ward.has_value())
    {
        return std::nullopt;
    }

    SharedDocument& w = ward.value();
    if (!signedRequestAccess(server, w.bobSession, w.id, requestFor(w.bob, "")).ok()
        || !signedRequestAccess(server, w.carolSession, w.id, requestFor(w.carol, "")).ok())
    {
        return std::nullopt;
    }

    const DocumentHeader header = nextHeader(w, {w.alice, w.bob, w.carol}, w.alice);
    if (publish(server, w, w.aliceSession, w.alice, header) != ServerError::None)
    {
        return std::nullopt;
    }

    return ward;
}

bool completePairing(ServerApi& server,
                     const TestDevice& initiator,
                     const Session& initiatorSession,
                     const TestDevice& responder,
                     const Session& responderSession)
{
    const Result<QString> code = signedStartPairing(server, initiatorSession, "Phone", "a commitment");
    if (!code.ok())
    {
        return false;
    }

    const Result<PairingState> read = wait<PairingState>([&](Reply<PairingState> reply)
    {
        server.pairing(responderSession, code.value.value(), reply);
    });

    return read.ok()
        && signedRespondToPairing(server, responderSession, code.value.value(), keysOf(responder)).ok()
        && signedRevealPairing(server, initiatorSession, code.value.value(), keysOf(initiator), Pairing::newNonce())
               .ok()
        && signedHandOver(server, responderSession, code.value.value(), "a signed handover").ok();
}

} // namespace ServerTestSupport
