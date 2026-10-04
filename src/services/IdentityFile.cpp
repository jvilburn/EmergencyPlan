#include "IdentityFile.h"

#include "HexCodec.h"
#include "StrictJson.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>

namespace
{

// Part of the identity file's format.
const QLatin1String KeySealingSecret("sealingSecret");
const QLatin1String KeySigningSeed("signingSeed");
const QLatin1String KeyMember("member");

/// Owner read and write only. On Windows the folder's access list already keeps
/// it to this user, and this only clears the read-only flag.
const QFileDevice::Permissions OwnerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

} // namespace

// --- IdentityRecord --------------------------------------------------------------

QByteArray IdentityRecord::toBytes() const
{
    QJsonObject json;
    json[KeySealingSecret] = HexCodec::encode(sealing.secretKey.bytes);
    json[KeySigningSeed] = HexCodec::encode(DocumentCrypto::seedOf(signing.signingKey));
    if (member.has_value())
    {
        json[KeyMember] = member.value().toHex();
    }
    return QJsonDocument(json).toJson(QJsonDocument::Indented);
}

std::optional<IdentityRecord> IdentityRecord::fromBytes(const QByteArray& bytes)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject json = document.object();
    QString sealingText;
    QString seedText;
    if (!StrictJson::hasOnlyKeys(json, {KeySealingSecret, KeySigningSeed, KeyMember})
        || !StrictJson::readString(json, KeySealingSecret, sealingText)
        || !StrictJson::readString(json, KeySigningSeed, seedText))
    {
        return std::nullopt;
    }

    SecretKey secret;
    secret.bytes = HexCodec::decode(sealingText, SecretKey::Size);
    const QByteArray seed = HexCodec::decode(seedText, DocumentCrypto::SigningSeedSize);

    IdentityRecord record;
    record.sealing = DocumentCrypto::keypairFromSecret(secret);
    record.signing = DocumentCrypto::signingKeypairFromSeed(seed);
    if (!record.sealing.publicKey.isValid() || !record.signing.signingKey.isValid())
    {
        return std::nullopt;
    }

    if (json.contains(KeyMember))
    {
        QString memberText;
        if (!StrictJson::readString(json, KeyMember, memberText))
        {
            return std::nullopt;
        }
        const MemberId member = MemberId::fromHex(memberText);
        if (!member.isValid())
        {
            return std::nullopt;
        }
        record.member = member;
    }

    return record;
}

bool IdentityRecord::operator==(const IdentityRecord& other) const
{
    return sealing.publicKey == other.sealing.publicKey
        && sealing.secretKey == other.sealing.secretKey
        && signing.verifyKey == other.signing.verifyKey
        && signing.signingKey == other.signing.signingKey
        && member == other.member;
}

// --- The file --------------------------------------------------------------------

namespace IdentityFile
{

Load load(const QString& path, IdentityRecord& out, QString* errorMessage)
{
    QFile file(path);
    if (!file.exists())
    {
        return Load::NotFound;
    }

    std::optional<IdentityRecord> record;
    if (file.open(QIODevice::ReadOnly))
    {
        record = IdentityRecord::fromBytes(file.readAll());
    }

    if (!record.has_value())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("This device's identity file %1 could not be read. "
                                        "Ward Planning will not start a new identity while it is there.")
                                .arg(path);
        }
        return Load::Failed;
    }

    out = record.value();
    return Load::Loaded;
}

bool store(const QString& path, const IdentityRecord& record, QString* errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QByteArray bytes = record.toBytes();
    QSaveFile file(path);
    const bool written = file.open(QIODevice::WriteOnly)
        && file.setPermissions(OwnerOnly)
        && file.write(bytes) == bytes.size()
        && file.commit();
    if (!written)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("This device's identity could not be saved to %1: %2")
                                .arg(path, file.errorString());
        }
        return false;
    }

    // Set again on the committed file, in case the platform gave it the folder's.
    QFile::setPermissions(path, OwnerOnly);
    return true;
}

} // namespace IdentityFile
