#include "DocumentHeader.h"

#include "CanonicalBytes.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"

#include <QJsonArray>
#include <QJsonValue>

#include <cmath>
#include <limits>

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
const QLatin1String KeyKeyGeneration("keyGeneration");
const QLatin1String KeySaveCounter("saveCounter");
const QLatin1String KeyLastWriter("lastWriter");
const QLatin1String KeyWriterPublicKey("writerPublicKey");
const QLatin1String KeyWriterVerifyKey("writerVerifyKey");
const QLatin1String KeyLastWriteTime("lastWriteTime");
const QLatin1String KeyNonce("nonce");
const QLatin1String KeyRecipients("recipients");
const QLatin1String KeyAdmins("admins");
const QLatin1String KeyPayloadHash("payloadHash");
const QLatin1String KeyWriterSignature("writerSignature");

const QLatin1String KeyKind("kind");
const QLatin1String KeyMember("member");
const QLatin1String KeyFingerprint("fingerprint");
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");
const QLatin1String KeySealedKey("sealedKey");

// --- Serialisation ---

/// Milliseconds are kept deliberately. Qt::ISODate drops them, and a writer that
/// built its associated data from a value carrying milliseconds while the reader
/// built it from one that had lost them would make every file fail to
/// authenticate.
QString serialiseTime(const QDateTime& time)
{
    return time.toUTC().toString(Qt::ISODateWithMs);
}

QString toHex(const QByteArray& bytes)
{
    return HexCodec::encode(bytes);
}

QString toBase64(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64());
}

// --- Strict reading ---

/// Converting a double that is outside the integer type's range is undefined
/// behaviour, so the range is checked while the value is still a double. These
/// bytes come from the server, so "no sane writer would" is not a guard.
bool readInt(const QJsonObject& json, QLatin1String key, int& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number)
        || number < double(std::numeric_limits<int>::min())
        || number > double(std::numeric_limits<int>::max())
        || number != std::trunc(number))
    {
        return false;
    }

    out = int(number);
    return true;
}

bool readInt64(const QJsonObject& json, QLatin1String key, qint64& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    // Both bounds are exact as doubles: -2^63 is representable, and 2^63 is the
    // first value above the range, so the comparison is >= rather than >.
    const double lowest = -9223372036854775808.0;
    const double aboveHighest = 9223372036854775808.0;

    const double number = value.toDouble();
    if (!std::isfinite(number) || number < lowest || number >= aboveHighest
        || number != std::trunc(number))
    {
        return false;
    }

    out = value.toInteger();
    return true;
}

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

template<typename Value>
bool readHexValue(const QJsonObject& json, QLatin1String key, Value& out)
{
    return readHexBytes(json, key, Value::Size, out.bytes);
}

bool readDocumentId(const QJsonObject& json, QLatin1String key, QString& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    // documentId is the one hex field kept as a string rather than decoded to
    // bytes: it is compared as a string in verifyLink and bound as a string into
    // the associated data, so two spellings would be two different documents.
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

bool readTime(const QJsonObject& json, QLatin1String key, QDateTime& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!parsed.isValid())
    {
        return false;
    }

    out = parsed.toUTC();

    // The value must survive a round trip, or the associated data a reader
    // computes will differ from the one the writer computed.
    return serialiseTime(out) == value.toString();
}

} // namespace

// --- RecipientEntry ---

QJsonObject RecipientEntry::toJson() const
{
    QJsonObject json;
    json[KeyKind] = int(kind);
    json[KeyMember] = member.toHex();
    json[KeyFingerprint] = fingerprint.toHex();
    json[KeyPublicKey] = toHex(publicKey.bytes);
    json[KeyVerifyKey] = toHex(verifyKey.bytes);
    json[KeySealedKey] = toBase64(sealedKey);
    return json;
}

bool RecipientEntry::operator==(const RecipientEntry& other) const
{
    return kind == other.kind
        && member == other.member
        && fingerprint == other.fingerprint
        && publicKey == other.publicKey
        && verifyKey == other.verifyKey
        && sealedKey == other.sealedKey;
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

    json[KeyKeyGeneration] = keyGeneration;
    json[KeySaveCounter] = saveCounter;
    json[KeyLastWriter] = lastWriter.toHex();
    json[KeyWriterPublicKey] = toHex(writerPublicKey.bytes);
    json[KeyWriterVerifyKey] = toHex(writerVerifyKey.bytes);
    json[KeyLastWriteTime] = serialiseTime(lastWriteTime);
    json[KeyNonce] = toHex(nonce);

    QJsonArray recipientArray;
    for (const RecipientEntry& entry : recipients)
    {
        recipientArray.append(entry.toJson());
    }
    json[KeyRecipients] = recipientArray;

    QJsonArray adminArray;
    for (const MemberId& admin : admins)
    {
        adminArray.append(admin.toHex());
    }
    json[KeyAdmins] = adminArray;

    json[KeyPayloadHash] = toHex(payloadHash);
    json[KeyWriterSignature] = toHex(writerSignature);
    return json;
}

HeaderParse DocumentHeader::parse(const QJsonObject& json, DocumentHeader& out)
{
    DocumentHeader header;

    if (!readInt(json, KeyFormatVersion, header.formatVersion))
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

    if (!readDocumentId(json, KeyDocumentId, header.documentId))
    {
        return HeaderParse::Malformed;
    }

    if (json.contains(KeyOriginDocumentId)
        && !readDocumentId(json, KeyOriginDocumentId, header.originDocumentId))
    {
        return HeaderParse::Malformed;
    }

    if (!readInt(json, KeyKeyGeneration, header.keyGeneration) || header.keyGeneration < 1)
    {
        return HeaderParse::Malformed;
    }

    if (!readInt64(json, KeySaveCounter, header.saveCounter) || header.saveCounter < 0)
    {
        return HeaderParse::Malformed;
    }

    if (!readHexValue(json, KeyLastWriter, header.lastWriter)
        || !readHexValue(json, KeyWriterPublicKey, header.writerPublicKey)
        || !readHexValue(json, KeyWriterVerifyKey, header.writerVerifyKey))
    {
        return HeaderParse::Malformed;
    }

    // The keys a header carries for its writer are the writer's, or the header
    // could vouch for itself with anyone's.
    if (DocumentCrypto::fingerprintOf(header.writerPublicKey, header.writerVerifyKey)
        != header.lastWriter)
    {
        return HeaderParse::Malformed;
    }

    if (!readTime(json, KeyLastWriteTime, header.lastWriteTime))
    {
        return HeaderParse::Malformed;
    }

    if (!readHexBytes(json, KeyNonce, DocumentCrypto::NonceSize, header.nonce))
    {
        return HeaderParse::Malformed;
    }

    // An empty list is the last person's leave: valid, final, and unopenable.
    const QJsonValue recipientsValue = json.value(KeyRecipients);
    if (!recipientsValue.isArray())
    {
        return HeaderParse::Malformed;
    }

    for (const QJsonValue& element : recipientsValue.toArray())
    {
        if (!element.isObject())
        {
            return HeaderParse::Malformed;
        }

        const QJsonObject entryJson = element.toObject();

        int kind = 0;
        if (!readInt(entryJson, KeyKind, kind))
        {
            return HeaderParse::Malformed;
        }

        // An unknown kind is a later format, not a damaged file.
        if (kind != int(RecipientKind::X25519Sealed))
        {
            return HeaderParse::NeedsNewerVersion;
        }

        RecipientEntry entry;
        entry.kind = RecipientKind::X25519Sealed;

        if (!readHexValue(entryJson, KeyMember, entry.member)
            || !readHexValue(entryJson, KeyFingerprint, entry.fingerprint)
            || !readHexValue(entryJson, KeyPublicKey, entry.publicKey)
            || !readHexValue(entryJson, KeyVerifyKey, entry.verifyKey)
            || !readBase64Bytes(entryJson,
                                KeySealedKey,
                                DocumentCrypto::SealedDataKeySize,
                                entry.sealedKey))
        {
            return HeaderParse::Malformed;
        }

        header.recipients.append(entry);
    }

    // Canonical: ascending, no repeats, so one admin set has one associated data.
    const QJsonValue adminsValue = json.value(KeyAdmins);
    if (!adminsValue.isArray())
    {
        return HeaderParse::Malformed;
    }

    QString previousAdmin;
    for (const QJsonValue& element : adminsValue.toArray())
    {
        if (!element.isString())
        {
            return HeaderParse::Malformed;
        }

        const MemberId admin = MemberId::fromHex(element.toString());
        if (!admin.isValid() || admin.toHex() <= previousAdmin)
        {
            return HeaderParse::Malformed;
        }

        previousAdmin = admin.toHex();
        header.admins.append(admin);
    }

    if (!readHexBytes(json, KeyPayloadHash, DocumentCrypto::PayloadHashSize, header.payloadHash))
    {
        return HeaderParse::Malformed;
    }

    if (!readHexBytes(json,
                      KeyWriterSignature,
                      DocumentCrypto::SignatureSize,
                      header.writerSignature))
    {
        return HeaderParse::Malformed;
    }

    out = header;
    return HeaderParse::Ok;
}

QByteArray DocumentHeader::associatedData() const
{
    CanonicalBytes writer;

    writer.append(qint64(formatVersion));
    writer.append(documentId);
    writer.append(originDocumentId);
    writer.append(qint64(keyGeneration));
    writer.append(saveCounter);
    writer.append(lastWriter.toHex());
    writer.append(toHex(writerPublicKey.bytes));
    writer.append(toHex(writerVerifyKey.bytes));
    writer.append(serialiseTime(lastWriteTime));
    writer.append(toHex(nonce));

    // The counts are what stop one collection borrowing the other's elements:
    // without them a header with two recipients and no admins could produce the
    // same bytes as one with fewer recipients and an admin.
    writer.append(qint64(recipients.size()));
    for (const RecipientEntry& entry : recipients)
    {
        writer.append(qint64(int(entry.kind)));
        writer.append(entry.member.toHex());
        writer.append(entry.fingerprint.toHex());
        writer.append(toHex(entry.publicKey.bytes));
        writer.append(toHex(entry.verifyKey.bytes));
        writer.append(toBase64(entry.sealedKey));
    }

    writer.append(qint64(admins.size()));
    for (const MemberId& admin : admins)
    {
        writer.append(admin.toHex());
    }

    return writer.bytes();
}

QByteArray DocumentHeader::signedBytes() const
{
    CanonicalBytes writer;
    writer.append(associatedData());
    writer.append(toHex(payloadHash));
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
        && keyGeneration == other.keyGeneration
        && saveCounter == other.saveCounter
        && lastWriter == other.lastWriter
        && writerPublicKey == other.writerPublicKey
        && writerVerifyKey == other.writerVerifyKey
        && serialiseTime(lastWriteTime) == serialiseTime(other.lastWriteTime)
        && nonce == other.nonce
        && recipients == other.recipients
        && admins == other.admins
        && payloadHash == other.payloadHash
        && writerSignature == other.writerSignature;
}
