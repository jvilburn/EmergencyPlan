#include "DocumentHeader.h"

#include "CanonicalBytes.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

namespace
{

// --- JSON field names. Part of the file format. ---
//
// Named rather than spelled out at each use, because the writer and the reader
// have to agree on every one of them and a typo in one place would be a silent
// format bug.
const QLatin1String KeyFormatVersion("formatVersion");
const QLatin1String KeyDocumentId("documentId");
const QLatin1String KeyOriginDocumentId("originDocumentId");
const QLatin1String KeySaveCounter("saveCounter");
const QLatin1String KeyNonce("nonce");
const QLatin1String KeyRecipients("recipients");

const QLatin1String KeyFingerprint("fingerprint");
const QLatin1String KeySealedKey("sealedKey");

// --- Serialisation ---

QString toBase64(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64());
}

// --- Strict reading ---

bool readHexBytes(const QJsonObject& json, QLatin1String key, int expectedSize, QByteArray& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    out = HexCodec::decode(value.toString(), expectedSize);
    return out.size() == expectedSize;
}

bool readDocumentId(const QJsonObject& json, QLatin1String key, QString& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    // documentId is the one hex field kept as a string rather than decoded to
    // bytes: it is compared as a string and bound as a string into the associated
    // data, so two spellings would be two different documents.
    const QString text = value.toString();
    if (!HexCodec::isCanonical(text, DocumentHeader::DocumentIdLength))
    {
        return false;
    }

    out = text;
    return true;
}

/// QByteArray::fromBase64 is lenient in the same way fromHex is, so decode with
/// errors made fatal rather than checking the result's length alone.
bool readBase64Bytes(const QJsonObject& json, QLatin1String key, int expectedSize, QByteArray& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toString().toLatin1(),
        QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);

    if (!decoded)
    {
        return false;
    }

    out = decoded.decoded;
    return out.size() == expectedSize;
}

} // namespace

// --- RecipientEntry ---

QJsonObject RecipientEntry::toJson() const
{
    QJsonObject json;
    json[KeyFingerprint] = fingerprint.toHex();
    json[KeySealedKey] = toBase64(sealedKey);
    return json;
}

bool RecipientEntry::operator==(const RecipientEntry& other) const
{
    return fingerprint == other.fingerprint && sealedKey == other.sealedKey;
}

// --- DocumentHeader ---

QJsonObject DocumentHeader::toJson() const
{
    QJsonObject json;
    json[KeyFormatVersion] = formatVersion;
    json[KeyDocumentId] = documentId;

    if (!originDocumentId.isEmpty())
    {
        json[KeyOriginDocumentId] = originDocumentId;
    }

    json[KeySaveCounter] = saveCounter;
    json[KeyNonce] = HexCodec::encode(nonce);

    QJsonArray recipientArray;
    for (const RecipientEntry& entry : recipients)
    {
        recipientArray.append(entry.toJson());
    }
    json[KeyRecipients] = recipientArray;
    return json;
}

HeaderParse DocumentHeader::parse(const QJsonObject& json, DocumentHeader& out)
{
    DocumentHeader header;

    if (!StrictJson::readInt(json, KeyFormatVersion, header.formatVersion))
    {
        return HeaderParse::Malformed;
    }

    if (header.formatVersion > CurrentFormatVersion)
    {
        return HeaderParse::NeedsNewerVersion;
    }

    if (header.formatVersion < CurrentFormatVersion)
    {
        return HeaderParse::Malformed;
    }

    // In a version this build knows, every field is one it understands.
    if (!StrictJson::hasOnlyKeys(json,
                                 {KeyFormatVersion,
                                  KeyDocumentId,
                                  KeyOriginDocumentId,
                                  KeySaveCounter,
                                  KeyNonce,
                                  KeyRecipients}))
    {
        return HeaderParse::Malformed;
    }

    if (!readDocumentId(json, KeyDocumentId, header.documentId))
    {
        return HeaderParse::Malformed;
    }

    if (json.contains(KeyOriginDocumentId)
        && !readDocumentId(json, KeyOriginDocumentId, header.originDocumentId))
    {
        return HeaderParse::Malformed;
    }

    if (!StrictJson::readInt64(json, KeySaveCounter, header.saveCounter) || header.saveCounter < 0
        || header.saveCounter > MaxSaveCounter)
    {
        return HeaderParse::Malformed;
    }

    if (!readHexBytes(json, KeyNonce, DocumentCrypto::NonceSize, header.nonce))
    {
        return HeaderParse::Malformed;
    }

    const QJsonValue recipientsValue = json.value(KeyRecipients);
    if (!recipientsValue.isArray() || recipientsValue.toArray().isEmpty())
    {
        return HeaderParse::Malformed;
    }

    QSet<QString> fingerprints;
    for (const QJsonValue& element : recipientsValue.toArray())
    {
        if (!element.isObject())
        {
            return HeaderParse::Malformed;
        }

        const QJsonObject entryJson = element.toObject();
        if (!StrictJson::hasOnlyKeys(entryJson, {KeyFingerprint, KeySealedKey}))
        {
            return HeaderParse::Malformed;
        }

        RecipientEntry entry;
        if (!readHexBytes(entryJson, KeyFingerprint, Fingerprint::Size, entry.fingerprint.bytes)
            || !readBase64Bytes(entryJson, KeySealedKey, DocumentCrypto::SealedDataKeySize, entry.sealedKey))
        {
            return HeaderParse::Malformed;
        }

        // One entry per device, or which sealed key is a device's would be a guess.
        if (fingerprints.contains(entry.fingerprint.toHex()))
        {
            return HeaderParse::Malformed;
        }
        fingerprints.insert(entry.fingerprint.toHex());

        header.recipients.append(entry);
    }

    out = header;
    return HeaderParse::Ok;
}

QByteArray DocumentHeader::associatedData() const
{
    CanonicalBytes writer;

    writer.appendNumber(qint64(formatVersion));
    writer.appendText(documentId);
    writer.appendText(originDocumentId);
    writer.appendNumber(saveCounter);
    writer.appendText(HexCodec::encode(nonce));

    writer.appendNumber(qint64(recipients.size()));
    for (const RecipientEntry& entry : recipients)
    {
        writer.appendText(entry.fingerprint.toHex());
        writer.appendText(toBase64(entry.sealedKey));
    }

    return writer.bytes();
}

std::optional<RecipientEntry> DocumentHeader::entryFor(const Fingerprint& fingerprint) const
{
    if (!fingerprint.isValid())
    {
        return std::nullopt;
    }

    for (const RecipientEntry& entry : recipients)
    {
        if (entry.fingerprint == fingerprint)
        {
            return entry;
        }
    }

    return std::nullopt;
}

bool DocumentHeader::operator==(const DocumentHeader& other) const
{
    return formatVersion == other.formatVersion
        && documentId == other.documentId
        && originDocumentId == other.originDocumentId
        && saveCounter == other.saveCounter
        && nonce == other.nonce
        && recipients == other.recipients;
}
