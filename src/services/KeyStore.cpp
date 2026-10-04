#include "KeyStore.h"

#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <qtkeychain/keychain.h>

namespace
{

const QString Service("Ward Planning");

// Part of the keychain record's format.
const QLatin1String KeySealingPublic("sealingPublic");
const QLatin1String KeySealingSecret("sealingSecret");
const QLatin1String KeyVerify("verifyKey");
const QLatin1String KeySigning("signingKey");
const QLatin1String KeyMember("member");

bool readKey(const QJsonObject& json, QLatin1String key, int size, QByteArray& out)
{
    QString text;
    if (!StrictJson::readString(json, key, text))
    {
        return false;
    }
    out = HexCodec::decode(text, size);
    return !out.isEmpty();
}

} // namespace

// --- IdentityRecord --------------------------------------------------------------

QByteArray IdentityRecord::toBytes() const
{
    QJsonObject json;
    json[KeySealingPublic] = HexCodec::encode(sealing.publicKey.bytes);
    json[KeySealingSecret] = HexCodec::encode(sealing.secretKey.bytes);
    json[KeyVerify] = HexCodec::encode(signing.verifyKey.bytes);
    json[KeySigning] = HexCodec::encode(signing.signingKey.bytes);
    if (member.has_value())
    {
        json[KeyMember] = member.value().toHex();
    }
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
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
    IdentityRecord record;
    if (!StrictJson::hasOnlyKeys(json, {KeySealingPublic, KeySealingSecret, KeyVerify, KeySigning, KeyMember})
        || !readKey(json, KeySealingPublic, PublicKey::Size, record.sealing.publicKey.bytes)
        || !readKey(json, KeySealingSecret, SecretKey::Size, record.sealing.secretKey.bytes)
        || !readKey(json, KeyVerify, VerifyKey::Size, record.signing.verifyKey.bytes)
        || !readKey(json, KeySigning, SigningKey::Size, record.signing.signingKey.bytes))
    {
        return std::nullopt;
    }

    if (json.contains(KeyMember))
    {
        QString text;
        if (!StrictJson::readString(json, KeyMember, text))
        {
            return std::nullopt;
        }
        const MemberId member = MemberId::fromHex(text);
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

// --- KeyStore --------------------------------------------------------------------

KeyStore::KeyStore(const QString& account, QObject* parent)
    : QObject(parent)
    , m_account(account)
{
}

void KeyStore::store(const QByteArray& secret)
{
    QKeychain::WritePasswordJob* job = new QKeychain::WritePasswordJob(Service, this);
    job->setAutoDelete(true);
    job->setKey(m_account);
    job->setTextData(QString::fromLatin1(secret.toBase64()));
    connect(job, &QKeychain::Job::finished, this, &KeyStore::onStored);
    job->start();
}

void KeyStore::load()
{
    QKeychain::ReadPasswordJob* job = new QKeychain::ReadPasswordJob(Service, this);
    job->setAutoDelete(true);
    job->setKey(m_account);
    connect(job, &QKeychain::Job::finished, this, &KeyStore::onLoaded);
    job->start();
}

void KeyStore::remove()
{
    QKeychain::DeletePasswordJob* job = new QKeychain::DeletePasswordJob(Service, this);
    job->setAutoDelete(true);
    job->setKey(m_account);
    connect(job, &QKeychain::Job::finished, this, &KeyStore::onRemoved);
    job->start();
}

void KeyStore::onStored(QKeychain::Job* job)
{
    if (job->error() != QKeychain::NoError)
    {
        emit failed(job->errorString());
        return;
    }
    emit stored();
}

void KeyStore::onLoaded(QKeychain::Job* job)
{
    switch (job->error())
    {
    case QKeychain::NoError:
        break;
    case QKeychain::EntryNotFound:
        emit notFound();
        return;
    default:
        emit failed(job->errorString());
        return;
    }

    const QKeychain::ReadPasswordJob* read = static_cast<const QKeychain::ReadPasswordJob*>(job);
    const QByteArray secret = QByteArray::fromBase64(read->textData().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (secret.isEmpty())
    {
        emit failed(tr("The keychain entry for Ward Planning is not readable."));
        return;
    }
    emit loaded(secret);
}

void KeyStore::onRemoved(QKeychain::Job* job)
{
    if (job->error() != QKeychain::NoError && job->error() != QKeychain::EntryNotFound)
    {
        emit failed(job->errorString());
        return;
    }
    emit removed();
}
