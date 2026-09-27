#include "ServerTestSupport.h"

#include "DocumentCrypto.h"

using namespace ChainTestSupport;
using namespace ServerContract;

namespace ServerTestSupport
{

std::optional<Session> signIn(ServerApi& server, const TestDevice& device)
{
    const Result<QByteArray> challenge = server.challenge();
    if (!challenge.ok())
    {
        return std::nullopt;
    }

    const QByteArray signature = DocumentCrypto::sign(SignatureTag::Auth,
                                                      challenge.value.value(),
                                                      device.signing.signingKey);
    return server.authenticate(device.signing.verifyKey, challenge.value.value(), signature).value;
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
    return server.advanceClock(seconds).ok() && signInAll(server, ward);
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

    const Result<qint64> result = server.registerDocument(ward.aliceSession, upload);
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
    const Result<qint64> result = server.upload(session, ward.id, ward.version, upload);
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
    if (!server.requestAccess(w.bobSession, w.id, requestFor(w.bob, "")).ok()
        || !server.requestAccess(w.carolSession, w.id, requestFor(w.carol, "")).ok())
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
    const Result<QString> code = server.startPairing(initiatorSession, "Phone", "a commitment");
    if (!code.ok())
    {
        return false;
    }

    return server.pairing(responderSession, code.value.value()).ok()
        && server.respondToPairing(responderSession, code.value.value(), keysOf(responder)).ok()
        && server.revealPairing(initiatorSession, code.value.value(), keysOf(initiator), "a nonce")
               .ok()
        && server.handOver(responderSession, code.value.value(), "a signed handover").ok();
}

} // namespace ServerTestSupport
