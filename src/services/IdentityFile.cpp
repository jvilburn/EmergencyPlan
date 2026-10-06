#include "IdentityFile.h"

#include "HexCodec.h"
#include "StrictJson.h"

#include <QDateTime>
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
const QLatin1String KeyVersion("version");
const QLatin1String KeySealingSecret("sealingSecret");
const QLatin1String KeySigningSeed("signingSeed");
const QLatin1String KeyPerson("person");
const QLatin1String KeyMember("member");
const QLatin1String KeyDisplayName("displayName");
const QLatin1String KeyDeviceName("deviceName");

/// Owner read and write only. On Windows the file inherits local app data's access
/// list - this user, the administrators, and the system, as for a home folder on
/// Unix - and this only clears the read-only flag.
const QFileDevice::Permissions OwnerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

/// Whether the bytes are an identity file a later version of the app wrote: an
/// object whose version is above this build's, whatever else is in it.
bool isFromANewerVersion(const QByteArray& bytes)
{
    const QJsonObject json = QJsonDocument::fromJson(bytes).object();
    int version = 0;
    return StrictJson::readInt(json, KeyVersion, version) && version > IdentityRecord::CurrentVersion;
}

} // namespace

// --- IdentityRecord --------------------------------------------------------------

QByteArray IdentityRecord::toBytes() const
{
    QJsonObject json;
    json[KeyVersion] = CurrentVersion;
    json[KeySealingSecret] = HexCodec::encode(sealing.secretKey.bytes);
    json[KeySigningSeed] = HexCodec::encode(DocumentCrypto::seedOf(signing.signingKey));
    if (person.has_value())
    {
        QJsonObject personJson;
        personJson[KeyMember] = person.value().member.toHex();
        personJson[KeyDisplayName] = person.value().displayName;
        personJson[KeyDeviceName] = person.value().deviceName;
        json[KeyPerson] = personJson;
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
    int version = 0;
    QString sealingText;
    QString seedText;
    if (!StrictJson::hasOnlyKeys(json, {KeyVersion, KeySealingSecret, KeySigningSeed, KeyPerson})
        || !StrictJson::readInt(json, KeyVersion, version)
        || version != CurrentVersion
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

    if (json.contains(KeyPerson))
    {
        QJsonObject personJson;
        QString memberText;
        Person person;
        if (!StrictJson::readObject(json, KeyPerson, personJson)
            || !StrictJson::hasOnlyKeys(personJson, {KeyMember, KeyDisplayName, KeyDeviceName})
            || !StrictJson::readString(personJson, KeyMember, memberText)
            || !StrictJson::readNonEmptyString(personJson, KeyDisplayName, person.displayName)
            || !StrictJson::readNonEmptyString(personJson, KeyDeviceName, person.deviceName))
        {
            return std::nullopt;
        }
        person.member = MemberId::fromHex(memberText);
        if (!person.member.isValid())
        {
            return std::nullopt;
        }
        record.person = person;
    }

    return record;
}

bool IdentityRecord::operator==(const IdentityRecord& other) const
{
    return sealing.publicKey == other.sealing.publicKey
        && sealing.secretKey == other.sealing.secretKey
        && signing.verifyKey == other.signing.verifyKey
        && signing.signingKey == other.signing.signingKey
        && person == other.person;
}

bool IdentityRecord::Person::operator==(const Person& other) const
{
    return member == other.member
        && displayName == other.displayName
        && deviceName == other.deviceName;
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

    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("This device's identity file %1 could not be opened: %2.")
                                .arg(path, file.errorString());
        }
        return Load::Failed;
    }

    const QByteArray bytes = file.readAll();
    const std::optional<IdentityRecord> record = IdentityRecord::fromBytes(bytes);
    if (record.has_value())
    {
        out = record.value();
        return Load::Loaded;
    }

    if (isFromANewerVersion(bytes))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("This device's identity was saved by a newer version of Ward Planning. "
                                        "Install the latest version to go on using it.");
        }
        return Load::NeedsNewerVersion;
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = QObject::tr("This device's identity file %1 is damaged.").arg(path);
    }
    return Load::Failed;
}

bool store(const QString& path, const IdentityRecord& record, QString* errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Written only when it reads back as itself, so a record with an invalid key, or a
    // public half that is not its secret's, never becomes a file the next start refuses.
    const QByteArray bytes = record.toBytes();
    if (IdentityRecord::fromBytes(bytes) != record)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("This device's identity is not whole, so it was not saved.");
        }
        return false;
    }

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

    return true;
}

QString moveAside(const QString& path, QString* errorMessage)
{
    const QFileInfo info(path);
    const QString aside = info.dir().filePath(
        QString("%1-set-aside-%2.%3")
            .arg(info.completeBaseName(),
                 QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss"),
                 info.suffix()));

    if (!QFile::rename(path, aside))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("The identity file %1 could not be moved aside.").arg(path);
        }
        return QString();
    }
    return aside;
}

} // namespace IdentityFile
