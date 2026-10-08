#include "HttpWire.h"

#include "StrictJson.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <utility>

using ServerContract::ServerError;

namespace
{

// Part of the wire format.
const QLatin1String KeyChallenge("challenge");
const QLatin1String KeyDevice("device");
const QLatin1String KeySignature("signature");
const QLatin1String KeyToken("token");
const QLatin1String KeyHeader("header");
const QLatin1String KeyHeaders("headers");
const QLatin1String KeyBlob("blob");
const QLatin1String KeyVersion("version");
const QLatin1String KeyBaseVersion("baseVersion");
const QLatin1String KeyLeaseHolder("leaseHolder");
const QLatin1String KeyHolder("holder");
const QLatin1String KeyExpires("expires");
const QLatin1String KeyMember("member");
const QLatin1String KeyDisplayName("displayName");
const QLatin1String KeyDeviceName("deviceName");
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");
const QLatin1String KeyPairingCode("pairingCode");
const QLatin1String KeyRequests("requests");
const QLatin1String KeyStatus("status");
const QLatin1String KeyDocuments("documents");
const QLatin1String KeyCode("code");
const QLatin1String KeyInitiatorDeviceName("initiatorDeviceName");
const QLatin1String KeyCommitment("commitment");
const QLatin1String KeyResponder("responder");
const QLatin1String KeyInitiator("initiator");
const QLatin1String KeyNonce("nonce");
const QLatin1String KeyHandover("handover");
const QLatin1String KeySeconds("seconds");
const QLatin1String KeyError("error");

QString toBase64(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64());
}

/// Strict: text that is not exactly base64 is refused rather than skipped over.
bool readBase64(const QJsonObject& json, QLatin1String key, QByteArray& out)
{
    QString text;
    if (!StrictJson::readString(json, key, text))
    {
        return false;
    }

    const QByteArray::FromBase64Result decoded =
        QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || toBase64(decoded.decoded) != text)
    {
        return false;
    }
    out = decoded.decoded;
    return true;
}

/// Absent is empty; present must be base64.
bool readOptionalBase64(const QJsonObject& json, QLatin1String key, QByteArray& out)
{
    return !json.contains(key) || readBase64(json, key, out);
}

bool readFingerprint(const QJsonObject& json, QLatin1String key, Fingerprint& out)
{
    QString text;
    if (!StrictJson::readString(json, key, text))
    {
        return false;
    }
    out = Fingerprint::fromHex(text);
    return out.isValid() && out.toHex() == text;
}

QJsonObject keysJson(const DeviceKeys& keys)
{
    QJsonObject json;
    json[KeyPublicKey] = toBase64(keys.publicKey.bytes);
    json[KeyVerifyKey] = toBase64(keys.verifyKey.bytes);
    return json;
}

bool readKeys(const QJsonObject& json, DeviceKeys& out)
{
    return readBase64(json, KeyPublicKey, out.publicKey.bytes)
        && readBase64(json, KeyVerifyKey, out.verifyKey.bytes)
        && out.isValid();
}

/// An optional nested object of device keys.
bool readOptionalKeys(const QJsonObject& json, QLatin1String key, std::optional<DeviceKeys>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    QJsonObject keysObject;
    DeviceKeys keys;
    if (!StrictJson::readObject(json, key, keysObject) || !readKeys(keysObject, keys))
    {
        return false;
    }
    out = keys;
    return true;
}

QJsonObject accessRequestJson(const ServerContract::AccessRequest& request)
{
    QJsonObject json;
    json[KeyMember] = request.member.toHex();
    json[KeyDisplayName] = request.displayName;
    json[KeyDeviceName] = request.device.deviceName();
    json[KeyPublicKey] = toBase64(request.device.publicKey().bytes);
    json[KeyVerifyKey] = toBase64(request.device.verifyKey().bytes);
    if (!request.pairingCode.isEmpty())
    {
        json[KeyPairingCode] = request.pairingCode;
    }
    return json;
}

std::optional<ServerContract::AccessRequest> readAccessRequest(const QJsonObject& json)
{
    QString member;
    QString deviceName;
    DeviceKeys keys;
    ServerContract::AccessRequest request;
    if (!StrictJson::readString(json, KeyMember, member)
        || !StrictJson::readNonEmptyString(json, KeyDisplayName, request.displayName)
        || !StrictJson::readNonEmptyString(json, KeyDeviceName, deviceName)
        || !readKeys(json, keys)
        || !StrictJson::readOptionalNonEmptyString(json, KeyPairingCode, request.pairingCode))
    {
        return std::nullopt;
    }

    request.member = MemberId::fromHex(member);
    if (!request.member.isValid())
    {
        return std::nullopt;
    }
    request.device = RosterDevice::create(deviceName, keys.publicKey, keys.verifyKey);
    return request;
}

QJsonObject uploadJson(const ServerContract::Upload& upload)
{
    QJsonObject json;
    json[KeyHeader] = upload.header.toJson();
    json[KeyBlob] = toBase64(upload.blob);
    return json;
}

bool readHeaders(const QJsonObject& json, QList<QJsonObject>& out)
{
    return StrictJson::readObjectArray(json, KeyHeaders, out) && !out.isEmpty();
}

/// Each error by the name it travels under.
std::optional<ServerError> errorNamed(const QString& name)
{
    static const QList<std::pair<QString, ServerError>> names = {
        {"unauthenticated", ServerError::Unauthenticated},
        {"notAllowListed", ServerError::NotAllowListed},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"chainRefused", ServerError::ChainRefused},
        {"payloadMismatch", ServerError::PayloadMismatch},
        {"counterJump", ServerError::CounterJump},
        {"deviceNotInvited", ServerError::DeviceNotInvited},
        {"idTaken", ServerError::IdTaken},
        {"rateLimited", ServerError::RateLimited},
        {"notFound", ServerError::NotFound},
        {"alreadyWritten", ServerError::AlreadyWritten},
        {"outOfOrder", ServerError::OutOfOrder},
        {"expired", ServerError::Expired},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    for (const auto& entry : names)
    {
        if (entry.first == name)
        {
            return entry.second;
        }
    }
    return std::nullopt;
}

/// For an answer that names no error: only the statuses a front end may send on
/// the server's behalf mean anything without one.
ServerError errorForStatus(int httpStatus)
{
    switch (httpStatus)
    {
    case 401:
        return ServerError::Unauthenticated;
    case 429:
        return ServerError::RateLimited;
    default:
        return ServerError::Unreachable;
    }
}

} // namespace

namespace HttpWire
{

// --- Requests --------------------------------------------------------------------

QJsonObject authenticateRequest(const VerifyKey& device, const QByteArray& challenge, const QByteArray& signature)
{
    QJsonObject json;
    json[KeyDevice] = toBase64(device.bytes);
    json[KeyChallenge] = toBase64(challenge);
    json[KeySignature] = toBase64(signature);
    return json;
}

QJsonObject registerRequest(const ServerContract::Upload& upload)
{
    return uploadJson(upload);
}

QJsonObject uploadRequest(qint64 baseVersion, const ServerContract::Upload& upload)
{
    QJsonObject json = uploadJson(upload);
    json[KeyBaseVersion] = baseVersion;
    return json;
}

QJsonObject accessRequestRequest(const ServerContract::AccessRequest& request)
{
    return accessRequestJson(request);
}

QJsonObject startPairingRequest(const QString& deviceName, const QByteArray& commitment)
{
    QJsonObject json;
    json[KeyDeviceName] = deviceName;
    json[KeyCommitment] = toBase64(commitment);
    return json;
}

QJsonObject deviceKeysRequest(const DeviceKeys& keys)
{
    return keysJson(keys);
}

QJsonObject revealPairingRequest(const DeviceKeys& initiator, const QByteArray& nonce)
{
    QJsonObject json = keysJson(initiator);
    json[KeyNonce] = toBase64(nonce);
    return json;
}

QJsonObject handOverRequest(const QByteArray& handover)
{
    QJsonObject json;
    json[KeyHandover] = toBase64(handover);
    return json;
}

QJsonObject advanceClockRequest(qint64 seconds)
{
    QJsonObject json;
    json[KeySeconds] = seconds;
    return json;
}

// --- Answers ---------------------------------------------------------------------

std::optional<QByteArray> readChallenge(const QJsonObject& json)
{
    QByteArray challenge;
    if (!readBase64(json, KeyChallenge, challenge) || challenge.isEmpty())
    {
        return std::nullopt;
    }
    return challenge;
}

std::optional<ServerContract::Session> readSession(const QJsonObject& json)
{
    ServerContract::Session session;
    if (!StrictJson::readNonEmptyString(json, KeyToken, session.token))
    {
        return std::nullopt;
    }
    return session;
}

std::optional<qint64> readVersion(const QJsonObject& json)
{
    qint64 version = 0;
    if (!StrictJson::readInt64(json, KeyVersion, version) || version < 1)
    {
        return std::nullopt;
    }
    return version;
}

std::optional<ServerContract::Head> readHead(const QJsonObject& json)
{
    ServerContract::Head head;
    const std::optional<qint64> version = readVersion(json);
    if (!version.has_value())
    {
        return std::nullopt;
    }
    head.version = version.value();

    if (json.contains(KeyLeaseHolder))
    {
        Fingerprint holder;
        if (!readFingerprint(json, KeyLeaseHolder, holder))
        {
            return std::nullopt;
        }
        head.leaseHolder = holder;
    }
    return head;
}

std::optional<ServerContract::HeaderChain> readHeaderChain(const QJsonObject& json)
{
    ServerContract::HeaderChain chain;
    if (!readHeaders(json, chain.headers))
    {
        return std::nullopt;
    }
    return chain;
}

std::optional<ServerContract::Snapshot> readSnapshot(const QJsonObject& json)
{
    ServerContract::Snapshot snapshot;
    const std::optional<qint64> version = readVersion(json);
    if (!version.has_value() || !readHeaders(json, snapshot.headers) || !readBase64(json, KeyBlob, snapshot.blob))
    {
        return std::nullopt;
    }
    snapshot.version = version.value();
    return snapshot;
}

std::optional<ServerContract::Lease> readLease(const QJsonObject& json)
{
    ServerContract::Lease lease;
    if (!readFingerprint(json, KeyHolder, lease.holder) || !StrictJson::readTime(json, KeyExpires, lease.expires))
    {
        return std::nullopt;
    }
    return lease;
}

std::optional<QList<ServerContract::AccessRequest>> readAccessRequests(const QJsonObject& json)
{
    QList<QJsonObject> objects;
    if (!StrictJson::readObjectArray(json, KeyRequests, objects))
    {
        return std::nullopt;
    }

    QList<ServerContract::AccessRequest> requests;
    for (const QJsonObject& object : objects)
    {
        const std::optional<ServerContract::AccessRequest> request = readAccessRequest(object);
        if (!request.has_value())
        {
            return std::nullopt;
        }
        requests.append(request.value());
    }
    return requests;
}

std::optional<ServerContract::RequestStatus> readRequestStatus(const QJsonObject& json)
{
    QString status;
    if (!StrictJson::readString(json, KeyStatus, status))
    {
        return std::nullopt;
    }
    if (status == "pending")
    {
        return ServerContract::RequestStatus::Pending;
    }
    if (status == "granted")
    {
        return ServerContract::RequestStatus::Granted;
    }
    if (status == "withdrawn")
    {
        return ServerContract::RequestStatus::Withdrawn;
    }
    return std::nullopt;
}

std::optional<QStringList> readDocumentIds(const QJsonObject& json)
{
    QStringList documents;
    if (!StrictJson::readStringArray(json, KeyDocuments, documents))
    {
        return std::nullopt;
    }
    for (const QString& documentId : documents)
    {
        if (documentId.isEmpty())
        {
            return std::nullopt;
        }
    }
    return documents;
}

std::optional<QString> readPairingCode(const QJsonObject& json)
{
    QString code;
    if (!StrictJson::readNonEmptyString(json, KeyCode, code))
    {
        return std::nullopt;
    }
    return code;
}

std::optional<ServerContract::PairingState> readPairingState(const QJsonObject& json)
{
    ServerContract::PairingState state;
    if (!StrictJson::readNonEmptyString(json, KeyCode, state.code)
        || !StrictJson::readNonEmptyString(json, KeyInitiatorDeviceName, state.initiatorDeviceName)
        || !readBase64(json, KeyCommitment, state.commitment)
        || !readOptionalKeys(json, KeyResponder, state.responder)
        || !readOptionalKeys(json, KeyInitiator, state.initiator)
        || !readOptionalBase64(json, KeyNonce, state.nonce)
        || !readOptionalBase64(json, KeyHandover, state.handover)
        || !StrictJson::readTime(json, KeyExpires, state.expires))
    {
        return std::nullopt;
    }
    return state;
}

ServerError readError(int httpStatus, const QByteArray& body)
{
    // A redirect not followed, or a server failing, is a server not there.
    if (httpStatus < 400 || httpStatus >= 500)
    {
        return ServerError::Unreachable;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject() || !document.object().value(KeyError).isString())
    {
        return errorForStatus(httpStatus);
    }
    return errorNamed(document.object().value(KeyError).toString()).value_or(ServerError::InvalidRequest);
}

} // namespace HttpWire
