#include "ServerContract.h"

#include "CanonicalBytes.h"
#include "DocumentCrypto.h"

namespace
{

constexpr int BodyHashSize = 32;

/// Each route as the design names it. Part of the protocol: the companion server
/// signs and checks the same strings.
QString routeOf(ServerContract::Endpoint endpoint)
{
    switch (endpoint)
    {
    case ServerContract::Endpoint::RegisterDocument:
        return "POST /documents";
    case ServerContract::Endpoint::Upload:
        return "PUT /documents/{id}";
    case ServerContract::Endpoint::TakeLease:
        return "POST /documents/{id}/lease";
    case ServerContract::Endpoint::ReleaseLease:
        return "DELETE /documents/{id}/lease";
    case ServerContract::Endpoint::RequestAccess:
        return "POST /documents/{id}/requests";
    case ServerContract::Endpoint::ClearAccessRequest:
        return "DELETE /documents/{id}/requests";
    case ServerContract::Endpoint::StartPairing:
        return "POST /pairings";
    case ServerContract::Endpoint::RespondToPairing:
        return "PUT /pairings/{code}/responder";
    case ServerContract::Endpoint::RevealPairing:
        return "PUT /pairings/{code}/reveal";
    case ServerContract::Endpoint::HandOver:
        return "PUT /pairings/{code}/handover";
    }

    // Unreachable for a value of the enumeration. An endpoint added without a
    // route here must not sign under another's.
    Q_ASSERT(false);
    return QString();
}

void appendHeader(CanonicalBytes& writer, const DocumentHeader& header)
{
    writer.appendBytes(header.signedBytes());
    writer.appendBytes(header.writerSignature);
}

void appendKeys(CanonicalBytes& writer, const DeviceKeys& keys)
{
    writer.appendBytes(keys.publicKey.bytes);
    writer.appendBytes(keys.verifyKey.bytes);
}

QByteArray requestMessage(ServerContract::Endpoint endpoint,
                          const QString& documentId,
                          const QByteArray& body)
{
    CanonicalBytes writer;
    writer.appendText(routeOf(endpoint));
    writer.appendText(documentId);
    writer.appendBytes(DocumentCrypto::hash(HashDomain::Request, body, BodyHashSize));
    return writer.bytes();
}

} // namespace

namespace ServerContract
{

QByteArray registerBody(const Upload& upload)
{
    CanonicalBytes writer;
    appendHeader(writer, upload.header);
    return writer.bytes();
}

QByteArray uploadBody(qint64 baseVersion, const Upload& upload)
{
    CanonicalBytes writer;
    writer.appendNumber(baseVersion);
    appendHeader(writer, upload.header);
    return writer.bytes();
}

QByteArray emptyBody()
{
    return QByteArray();
}

QByteArray requestAccessBody(const AccessRequest& request)
{
    CanonicalBytes writer;
    writer.appendText(request.member.toHex());
    writer.appendText(request.displayName);
    writer.appendText(request.device.deviceName());
    writer.appendBytes(request.device.publicKey().bytes);
    writer.appendBytes(request.device.verifyKey().bytes);
    writer.appendText(request.pairingCode);
    return writer.bytes();
}

QByteArray clearAccessRequestBody(const Fingerprint& device)
{
    CanonicalBytes writer;
    writer.appendText(device.toHex());
    return writer.bytes();
}

QByteArray startPairingBody(const QString& deviceName, const QByteArray& commitment)
{
    CanonicalBytes writer;
    writer.appendText(deviceName);
    writer.appendBytes(commitment);
    return writer.bytes();
}

QByteArray respondToPairingBody(const QString& code, const DeviceKeys& responder)
{
    CanonicalBytes writer;
    writer.appendText(code);
    appendKeys(writer, responder);
    return writer.bytes();
}

QByteArray revealPairingBody(const QString& code, const DeviceKeys& initiator, const QByteArray& nonce)
{
    CanonicalBytes writer;
    writer.appendText(code);
    appendKeys(writer, initiator);
    writer.appendBytes(nonce);
    return writer.bytes();
}

QByteArray handOverBody(const QString& code, const QByteArray& handover)
{
    CanonicalBytes writer;
    writer.appendText(code);
    writer.appendBytes(handover);
    return writer.bytes();
}

QByteArray signRequest(Endpoint endpoint,
                       const QString& documentId,
                       const QByteArray& body,
                       const SigningKey& key)
{
    return DocumentCrypto::sign(SignatureTag::Request, requestMessage(endpoint, documentId, body), key);
}

bool requestVerifies(Endpoint endpoint,
                     const QString& documentId,
                     const QByteArray& body,
                     const QByteArray& signature,
                     const VerifyKey& key)
{
    return DocumentCrypto::verify(SignatureTag::Request,
                                  requestMessage(endpoint, documentId, body),
                                  signature,
                                  key);
}

} // namespace ServerContract
