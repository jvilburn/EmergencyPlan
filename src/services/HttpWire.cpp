#include "HttpWire.h"

#include "DocumentCrypto.h"
#include "DocumentHeader.h"
#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <utility>

using namespace ServerContract;

namespace
{

// Part of the wire format.
const QLatin1String KeyChallenge("challenge");
const QLatin1String KeyDevice("device");
const QLatin1String KeySignature("signature");
const QLatin1String KeyToken("token");
const QLatin1String KeyMember("member");
const QLatin1String KeyDisplayName("displayName");
const QLatin1String KeyDeviceName("deviceName");
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");
const QLatin1String KeyPerson("person");
const QLatin1String KeyDocuments("documents");
const QLatin1String KeyDevices("devices");
const QLatin1String KeyFingerprint("fingerprint");
const QLatin1String KeyName("name");
const QLatin1String KeyLinkedBy("linkedBy");
const QLatin1String KeyLinkedAt("linkedAt");
const QLatin1String KeyCode("code");
const QLatin1String KeyApprovedAs("approvedAs");
const QLatin1String KeyExpires("expires");
const QLatin1String KeyHeader("header");
const QLatin1String KeyBlob("blob");
const QLatin1String KeyVersion("version");
const QLatin1String KeyBaseVersion("baseVersion");
const QLatin1String KeyPublished("published");
const QLatin1String KeyWhen("when");
const QLatin1String KeyLeaseHolder("leaseHolder");
const QLatin1String KeyHolder("holder");
const QLatin1String KeyMembers("members");
const QLatin1String KeyRole("role");
const QLatin1String KeyHistory("history");
const QLatin1String KeyEntries("entries");
const QLatin1String KeyIndex("index");
const QLatin1String KeyKind("kind");
const QLatin1String KeyActor("actor");
const QLatin1String KeySubject("subject");
const QLatin1String KeyRequests("requests");
const QLatin1String KeyRequest("request");
const QLatin1String KeyStatus("status");
const QLatin1String KeySeconds("seconds");
const QLatin1String KeyError("error");
const QLatin1String KeyHow("how");
const QLatin1String KeyByWhom("byWhom");

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

bool readMemberId(const QJsonObject& json, QLatin1String key, MemberId& out)
{
    QString text;
    if (!StrictJson::readString(json, key, text))
    {
        return false;
    }
    out = MemberId::fromHex(text);
    return out.isValid() && out.toHex() == text;
}

bool readKeys(const QJsonObject& json, DeviceKeys& out)
{
    return readBase64(json, KeyPublicKey, out.publicKey.bytes)
        && readBase64(json, KeyVerifyKey, out.verifyKey.bytes)
        && out.isValid();
}

/// A device whose fingerprint is not its keys' would name one device and seal to
/// another.
std::optional<Device> readDevice(const QJsonObject& json)
{
    Device device;
    if (!StrictJson::hasOnlyKeys(json, {KeyFingerprint, KeyPublicKey, KeyVerifyKey, KeyName, KeyLinkedBy, KeyLinkedAt})
        || !readFingerprint(json, KeyFingerprint, device.fingerprint)
        || !readKeys(json, device.keys)
        || !StrictJson::readNonEmptyString(json, KeyName, device.name)
        || !StrictJson::readTime(json, KeyLinkedAt, device.linkedAt))
    {
        return std::nullopt;
    }
    if (json.contains(KeyLinkedBy))
    {
        Fingerprint linkedBy;
        if (!readFingerprint(json, KeyLinkedBy, linkedBy))
        {
            return std::nullopt;
        }
        device.linkedBy = linkedBy;
    }
    if (DocumentCrypto::fingerprintOf(device.keys.publicKey, device.keys.verifyKey) != device.fingerprint)
    {
        return std::nullopt;
    }
    return device;
}

std::optional<Person> readPerson(const QJsonObject& json)
{
    Person person;
    QList<QJsonObject> devices;
    if (!StrictJson::hasOnlyKeys(json, {KeyMember, KeyDisplayName, KeyDevices})
        || !readMemberId(json, KeyMember, person.member)
        || !StrictJson::readNonEmptyString(json, KeyDisplayName, person.displayName)
        || !StrictJson::readObjectArray(json, KeyDevices, devices) || devices.isEmpty())
    {
        return std::nullopt;
    }
    for (const QJsonObject& deviceJson : devices)
    {
        const std::optional<Device> device = readDevice(deviceJson);
        if (!device.has_value())
        {
            return std::nullopt;
        }
        person.devices.append(device.value());
    }
    return person;
}

std::optional<Person> readPersonField(const QJsonObject& json, QLatin1String key)
{
    QJsonObject object;
    if (!StrictJson::readObject(json, key, object))
    {
        return std::nullopt;
    }
    return readPerson(object);
}

bool readPublication(const QJsonObject& json, Publication& out)
{
    QJsonObject object;
    return StrictJson::readObject(json, KeyPublished, object)
        && StrictJson::hasOnlyKeys(object, {KeyName, KeyDevice, KeyWhen})
        && StrictJson::readNonEmptyString(object, KeyName, out.name)
        && readFingerprint(object, KeyDevice, out.device)
        && StrictJson::readTime(object, KeyWhen, out.when);
}

bool readIndex(const QJsonObject& json, QLatin1String key, qint64& out)
{
    return StrictJson::readInt64(json, key, out) && out >= 1;
}

QJsonObject uploadJson(const Upload& upload)
{
    QJsonObject json;
    json[KeyHeader] = upload.header.toJson();
    json[KeyBlob] = toBase64(upload.blob);
    return json;
}

/// Each name as it travels, for the enumerations the wire carries as text.
template<typename T>
std::optional<T> named(const QList<std::pair<QString, T>>& names, const QString& name)
{
    for (const auto& entry : names)
    {
        if (entry.first == name)
        {
            return entry.second;
        }
    }
    return std::nullopt;
}

const QList<std::pair<QString, ServerError>>& errorNames()
{
    static const QList<std::pair<QString, ServerError>> names = {
        {"unauthenticated", ServerError::Unauthenticated},
        {"accessLost", ServerError::AccessLost},
        {"notMember", ServerError::NotMember},
        {"notAdmin", ServerError::NotAdmin},
        {"lastAdmin", ServerError::LastAdmin},
        {"notOnAllowList", ServerError::NotOnAllowList},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"idTaken", ServerError::IdTaken},
        {"notFound", ServerError::NotFound},
        {"expired", ServerError::Expired},
        {"rateLimited", ServerError::RateLimited},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    return names;
}

const QList<std::pair<QString, HistoryEntry::Kind>>& kindNames()
{
    static const QList<std::pair<QString, HistoryEntry::Kind>> names = {
        {"added", HistoryEntry::Kind::Added},
        {"removed", HistoryEntry::Kind::Removed},
        {"left", HistoryEntry::Kind::Left},
        {"madeAdmin", HistoryEntry::Kind::MadeAdmin},
        {"madeMember", HistoryEntry::Kind::MadeMember},
    };
    return names;
}

const QList<std::pair<QString, Role>>& roleNames()
{
    static const QList<std::pair<QString, Role>> names = {
        {"admin", Role::Admin},
        {"member", Role::Member},
    };
    return names;
}

const QList<std::pair<QString, LostAccess::How>>& howNames()
{
    static const QList<std::pair<QString, LostAccess::How>> names = {
        {"removed", LostAccess::How::Removed},
        {"left", LostAccess::How::Left},
        {"unlinked", LostAccess::How::Unlinked},
    };
    return names;
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

std::optional<LostAccess> readLostAccess(const QJsonObject& json)
{
    QString how;
    LostAccess lost;
    if (!StrictJson::hasOnlyKeys(json, {KeyError, KeyHow, KeyByWhom, KeyWhen})
        || !StrictJson::readString(json, KeyHow, how)
        || !StrictJson::readOptionalNonEmptyString(json, KeyByWhom, lost.byWhom)
        || !StrictJson::readTime(json, KeyWhen, lost.when))
    {
        return std::nullopt;
    }
    const std::optional<LostAccess::How> parsed = named(howNames(), how);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    lost.how = parsed.value();
    return lost;
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

QJsonObject registerPersonRequest(const Registration& registration)
{
    QJsonObject json;
    json[KeyMember] = registration.member.toHex();
    json[KeyDisplayName] = registration.displayName;
    json[KeyDeviceName] = registration.deviceName;
    json[KeyPublicKey] = toBase64(registration.publicKey.bytes);
    return json;
}

QJsonObject startLinkRequest(const PublicKey& publicKey, const QString& deviceName)
{
    QJsonObject json;
    json[KeyPublicKey] = toBase64(publicKey.bytes);
    json[KeyDeviceName] = deviceName;
    return json;
}

QJsonObject registerRequest(const Upload& upload)
{
    return uploadJson(upload);
}

QJsonObject uploadRequest(qint64 baseVersion, const Upload& upload)
{
    QJsonObject json = uploadJson(upload);
    json[KeyBaseVersion] = baseVersion;
    return json;
}

QJsonObject grantRequest(qint64 index)
{
    QJsonObject json;
    json[KeyRequest] = index;
    return json;
}

QJsonObject setRoleRequest(Role role)
{
    QJsonObject json;
    json[KeyRole] = role == Role::Admin ? QStringLiteral("admin") : QStringLiteral("member");
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

std::optional<Session> readSession(const QJsonObject& json)
{
    Session session;
    if (!StrictJson::readNonEmptyString(json, KeyToken, session.token))
    {
        return std::nullopt;
    }
    return session;
}

std::optional<Me> readMe(const QJsonObject& json)
{
    Me me;
    const std::optional<Person> person = readPersonField(json, KeyPerson);
    if (!person.has_value() || !StrictJson::readStringArray(json, KeyDocuments, me.documents))
    {
        return std::nullopt;
    }
    // Each names a file on the device, so each must be a document id.
    for (const QString& documentId : me.documents)
    {
        if (!HexCodec::isCanonical(documentId, DocumentHeader::DocumentIdLength))
        {
            return std::nullopt;
        }
    }
    me.person = person.value();
    return me;
}

std::optional<QString> readLinkCode(const QJsonObject& json)
{
    QString code;
    if (!StrictJson::readNonEmptyString(json, KeyCode, code))
    {
        return std::nullopt;
    }
    return code;
}

std::optional<LinkState> readLinkState(const QJsonObject& json)
{
    LinkState state;
    if (!StrictJson::hasOnlyKeys(json, {KeyDeviceName, KeyPublicKey, KeyVerifyKey, KeyApprovedAs, KeyExpires})
        || !StrictJson::readNonEmptyString(json, KeyDeviceName, state.deviceName)
        || !readKeys(json, state.keys)
        || !StrictJson::readTime(json, KeyExpires, state.expires))
    {
        return std::nullopt;
    }
    if (json.contains(KeyApprovedAs))
    {
        state.approvedAs = readPersonField(json, KeyApprovedAs);
        if (!state.approvedAs.has_value())
        {
            return std::nullopt;
        }
    }
    return state;
}

std::optional<qint64> readVersion(const QJsonObject& json)
{
    qint64 version = 0;
    if (!readIndex(json, KeyVersion, version))
    {
        return std::nullopt;
    }
    return version;
}

std::optional<Head> readHead(const QJsonObject& json)
{
    Head head;
    QList<QJsonObject> members;
    if (!readIndex(json, KeyVersion, head.version)
        || !readPublication(json, head.published)
        || !StrictJson::readObject(json, KeyHeader, head.header)
        || !StrictJson::readObjectArray(json, KeyMembers, members)
        || !StrictJson::readInt64(json, KeyHistory, head.history) || head.history < 0)
    {
        return std::nullopt;
    }

    if (json.contains(KeyLeaseHolder))
    {
        Fingerprint holder;
        if (!readFingerprint(json, KeyLeaseHolder, holder))
        {
            return std::nullopt;
        }
        head.leaseHolder = holder;
    }

    for (const QJsonObject& memberJson : members)
    {
        QString role;
        const std::optional<Person> person = readPersonField(memberJson, KeyPerson);
        if (!person.has_value() || !StrictJson::hasOnlyKeys(memberJson, {KeyPerson, KeyRole})
            || !StrictJson::readString(memberJson, KeyRole, role) || !named(roleNames(), role).has_value())
        {
            return std::nullopt;
        }
        head.members.append(Member{person.value(), named(roleNames(), role).value()});
    }
    return head;
}

std::optional<Snapshot> readSnapshot(const QJsonObject& json)
{
    Snapshot snapshot;
    if (!readIndex(json, KeyVersion, snapshot.version)
        || !readPublication(json, snapshot.published)
        || !StrictJson::readObject(json, KeyHeader, snapshot.header)
        || !readBase64(json, KeyBlob, snapshot.blob))
    {
        return std::nullopt;
    }
    return snapshot;
}

std::optional<Lease> readLease(const QJsonObject& json)
{
    Lease lease;
    if (!readFingerprint(json, KeyHolder, lease.holder) || !StrictJson::readTime(json, KeyExpires, lease.expires))
    {
        return std::nullopt;
    }
    return lease;
}

std::optional<QList<HistoryEntry>> readHistory(const QJsonObject& json)
{
    QList<QJsonObject> objects;
    if (!StrictJson::readObjectArray(json, KeyEntries, objects))
    {
        return std::nullopt;
    }

    QList<HistoryEntry> entries;
    for (const QJsonObject& object : objects)
    {
        HistoryEntry entry;
        QString kind;
        if (!readIndex(object, KeyIndex, entry.index)
            || !StrictJson::readString(object, KeyKind, kind)
            || !StrictJson::readNonEmptyString(object, KeyActor, entry.actor)
            || !StrictJson::readNonEmptyString(object, KeySubject, entry.subject)
            || !StrictJson::readTime(object, KeyWhen, entry.when)
            || !named(kindNames(), kind).has_value())
        {
            return std::nullopt;
        }
        entry.kind = named(kindNames(), kind).value();
        entries.append(entry);
    }
    return entries;
}

std::optional<QList<AccessRequest>> readAccessRequests(const QJsonObject& json)
{
    QList<QJsonObject> objects;
    if (!StrictJson::readObjectArray(json, KeyRequests, objects))
    {
        return std::nullopt;
    }

    QList<AccessRequest> requests;
    for (const QJsonObject& object : objects)
    {
        AccessRequest request;
        const std::optional<Person> person = readPersonField(object, KeyPerson);
        if (!person.has_value() || person.value().devices.size() != 1
            || !readIndex(object, KeyIndex, request.index)
            || !StrictJson::readTime(object, KeyWhen, request.when))
        {
            return std::nullopt;
        }
        request.person = person.value();
        requests.append(request);
    }
    return requests;
}

std::optional<RequestStatus> readRequestStatus(const QJsonObject& json)
{
    static const QList<std::pair<QString, RequestStatus>> names = {
        {"pending", RequestStatus::Pending},
        {"granted", RequestStatus::Granted},
        {"declined", RequestStatus::Declined},
    };

    QString status;
    if (!StrictJson::readString(json, KeyStatus, status))
    {
        return std::nullopt;
    }
    return named(names, status);
}

Failure readFailure(int httpStatus, const QByteArray& body)
{
    // A redirect not followed, or a server failing, is a server not there.
    if (httpStatus < 400 || httpStatus >= 500)
    {
        return Failure{ServerError::Unreachable, std::nullopt};
    }

    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject() || !document.object().value(KeyError).isString())
    {
        return Failure{errorForStatus(httpStatus), std::nullopt};
    }

    const ServerError error =
        named(errorNames(), document.object().value(KeyError).toString()).value_or(ServerError::InvalidRequest);
    if (error != ServerError::AccessLost)
    {
        return Failure{error, std::nullopt};
    }

    // Acted on by deleting the ward, so read in full or not at all.
    const std::optional<LostAccess> lost = readLostAccess(document.object());
    return lost.has_value() ? Failure{error, lost} : Failure{ServerError::InvalidRequest, std::nullopt};
}

} // namespace HttpWire
