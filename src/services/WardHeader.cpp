#include "WardHeader.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Identity.h"
#include "JsonReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool VersionRank::ranksAbove(const VersionRank& other) const
{
    if (number != other.number)
    {
        return number > other.number;
    }
    return writer > other.writer;
}

WardHeader::WardHeader(const DocumentId& document, const VersionRank& version, const QByteArray& nonce,
                       const QList<Recipient>& recipients, const QByteArray& line)
    : m_document(document)
    , m_version(version)
    , m_nonce(nonce)
    , m_recipients(recipients)
    , m_line(line)
{
}

WardHeader WardHeader::build(const DocumentId& document, const VersionRank& version, const QByteArray& dataKey,
                             const QList<RecipientKeys>& recipients)
{
    Q_ASSERT(version.number >= 1 && version.writer.size() == Crypto::FingerprintBytes);
    QByteArray nonce = Crypto::randomBytes(Crypto::NonceBytes);
    QList<Recipient> entries;
    QJsonArray recipientsJson;
    for (const RecipientKeys& keys : recipients)
    {
        Recipient entry{Identity::fingerprintOf(keys.publicKey, keys.verifyKey), Crypto::seal(dataKey, keys.publicKey)};
        QJsonObject entryJson;
        entryJson["fingerprint"] = Base64Url::encode(entry.fingerprint);
        entryJson["sealedKey"] = Base64Url::encode(entry.sealedKey);
        recipientsJson.append(entryJson);
        entries.append(entry);
    }

    QJsonObject json;
    json["format"] = Format;
    json["document"] = document.toString();
    json["number"] = version.number;
    json["writer"] = Base64Url::encode(version.writer);
    json["nonce"] = Base64Url::encode(nonce);
    json["recipients"] = recipientsJson;
    QByteArray line = QJsonDocument(json).toJson(QJsonDocument::Compact);
    Q_ASSERT(!line.contains('\n'));
    return WardHeader(document, version, nonce, entries, line);
}

HeaderRead WardHeader::parse(const QByteArray& line)
{
    std::optional<QJsonObject> object = JsonReader::parseObject(line);
    if (!object.has_value())
    {
        return HeaderRead{HeaderStatus::Unreadable, "the header is not a JSON object", std::nullopt};
    }
    QJsonValue format = object->value("format");
    if (!JsonReader::isInteger(format))
    {
        return HeaderRead{HeaderStatus::Unreadable, "format: is not an integer", std::nullopt};
    }
    if (format.toDouble() > Format)
    {
        return HeaderRead{HeaderStatus::Newer, QString(), std::nullopt};
    }
    if (format.toDouble() != Format)
    {
        return HeaderRead{HeaderStatus::Unreadable, "format: is not a version this app reads", std::nullopt};
    }

    JsonReader json(*object);
    json.integer("format");
    DocumentId document = DocumentId::fromBytes(json.bytes("document", Crypto::IdBytes));
    VersionRank version{json.integer("number"), json.bytes("writer", Crypto::FingerprintBytes)};
    if (version.number < 1)
    {
        json.fail("number", "is below 1");
    }
    QByteArray nonce = json.bytes("nonce", Crypto::NonceBytes);
    QList<Recipient> recipients;
    const QList<JsonReader> entryReaders = json.objects("recipients");
    for (JsonReader entry : entryReaders)
    {
        QByteArray fingerprint = entry.bytes("fingerprint", Crypto::FingerprintBytes);
        QByteArray sealedKey = entry.bytes("sealedKey", Crypto::SealedKeyBytes);
        entry.finish();
        recipients.append(Recipient{fingerprint, sealedKey});
    }
    json.finish();
    if (json.failed())
    {
        return HeaderRead{HeaderStatus::Unreadable, json.problem(), std::nullopt};
    }
    return HeaderRead{HeaderStatus::Read, QString(), WardHeader(document, version, nonce, recipients, line)};
}

std::optional<QByteArray> WardHeader::sealedKeyFor(const QByteArray& fingerprint) const
{
    for (const Recipient& recipient : m_recipients)
    {
        if (recipient.fingerprint == fingerprint)
        {
            return recipient.sealedKey;
        }
    }
    return std::nullopt;
}
