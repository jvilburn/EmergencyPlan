#include "HttpWire.h"

#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonDocument>

using namespace ServerContract;

namespace
{

// Part of the wire format.
const QLatin1String KeyHeader("header");
const QLatin1String KeyBlob("blob");
const QLatin1String KeyVersion("version");
const QLatin1String KeyBaseVersion("baseVersion");
const QLatin1String KeyPublished("published");
const QLatin1String KeyDevice("device");
const QLatin1String KeyWhen("when");
const QLatin1String KeyLeaseHolder("leaseHolder");
const QLatin1String KeyHolder("holder");
const QLatin1String KeyExpires("expires");
const QLatin1String KeySeconds("seconds");
const QLatin1String KeyError("error");

QString toBase64(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64());
}

bool readBase64(const QJsonObject& json, QLatin1String key, QByteArray& out)
{
    QString text;
    if (!StrictJson::readString(json, key, text))
    {
        return false;
    }
    const QByteArray::FromBase64Result decoded =
        QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
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
    return out.isValid() && text == out.toHex();
}

bool readPublication(const QJsonObject& json, Publication& out)
{
    QJsonObject published;
    return StrictJson::readObject(json, KeyPublished, published)
        && StrictJson::hasOnlyKeys(published, {KeyDevice, KeyWhen}) && readFingerprint(published, KeyDevice, out.device)
        && StrictJson::readTime(published, KeyWhen, out.when);
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

const QList<QPair<QString, ServerError>>& errorNames()
{
    static const QList<QPair<QString, ServerError>> names = {
        {"notAKeyHolder", ServerError::NotAKeyHolder},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"idTaken", ServerError::IdTaken},
        {"rateLimited", ServerError::RateLimited},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    return names;
}

} // namespace

namespace HttpWire
{

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

QJsonObject advanceClockRequest(qint64 seconds)
{
    QJsonObject json;
    json[KeySeconds] = seconds;
    return json;
}

QList<QPair<QByteArray, QByteArray>> signerHeaders(const Signer& signer)
{
    return {{"X-Signer-Public", signer.keys.publicKey.bytes.toBase64()},
            {"X-Signer-Verify", signer.keys.verifyKey.bytes.toBase64()},
            {"X-Signature", signer.signature.toBase64()}};
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
    if (!StrictJson::hasOnlyKeys(json, {KeyVersion, KeyPublished, KeyLeaseHolder, KeyHeader})
        || !readIndex(json, KeyVersion, head.version) || !readPublication(json, head.published)
        || !StrictJson::readObject(json, KeyHeader, head.header))
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
    return head;
}

std::optional<Snapshot> readSnapshot(const QJsonObject& json)
{
    Snapshot snapshot;
    if (!StrictJson::hasOnlyKeys(json, {KeyVersion, KeyPublished, KeyHeader, KeyBlob})
        || !readIndex(json, KeyVersion, snapshot.version) || !readPublication(json, snapshot.published)
        || !StrictJson::readObject(json, KeyHeader, snapshot.header) || !readBase64(json, KeyBlob, snapshot.blob))
    {
        return std::nullopt;
    }
    return snapshot;
}

std::optional<Lease> readLease(const QJsonObject& json)
{
    Lease lease;
    if (!StrictJson::hasOnlyKeys(json, {KeyHolder, KeyExpires}) || !readFingerprint(json, KeyHolder, lease.holder)
        || !StrictJson::readTime(json, KeyExpires, lease.expires))
    {
        return std::nullopt;
    }
    return lease;
}

ServerError readFailure(int httpStatus, const QByteArray& body)
{
    // A redirect not followed, or a server failing, is a server not there.
    if (httpStatus < 400 || httpStatus >= 500)
    {
        return ServerError::Unreachable;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject() || !document.object().value(KeyError).isString())
    {
        return httpStatus == 429 ? ServerError::RateLimited : ServerError::Unreachable;
    }

    const QString name = document.object().value(KeyError).toString();
    for (const QPair<QString, ServerError>& entry : errorNames())
    {
        if (entry.first == name)
        {
            return entry.second;
        }
    }
    return ServerError::InvalidRequest;
}

} // namespace HttpWire
