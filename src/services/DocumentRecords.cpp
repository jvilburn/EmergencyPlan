#include "DocumentRecords.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonArray>

namespace
{

// Part of the settings format.
const QLatin1String KeyBaseVersion("baseVersion");
const QLatin1String KeyBaseHash("baseHash");
const QLatin1String KeyBaseSaveCounter("baseSaveCounter");
const QLatin1String KeyGranted("granted");     ///< written by builds before atomic grants; nothing reads it
const QLatin1String KeyHistorySeen("historySeen");

} // namespace

QJsonObject DocumentRecord::toJson() const
{
    QJsonObject json;
    if (baseVersion.has_value())
    {
        json[KeyBaseVersion] = baseVersion.value();
        json[KeyBaseHash] = HexCodec::encode(baseHash);
    }

    json[KeyBaseSaveCounter] = baseSaveCounter;
    json[KeyHistorySeen] = historySeen;


    return json;
}

std::optional<DocumentRecord> DocumentRecord::fromJson(const QJsonObject& json)
{
    DocumentRecord record;
    if (!StrictJson::hasOnlyKeys(json, {KeyBaseVersion, KeyBaseHash, KeyBaseSaveCounter, KeyGranted, KeyHistorySeen})
        || !StrictJson::readInt64(json, KeyBaseSaveCounter, record.baseSaveCounter)
        || !StrictJson::readInt64(json, KeyHistorySeen, record.historySeen))
    {
        return std::nullopt;
    }

    // A base version and its hash come together, or neither does.
    if (json.contains(KeyBaseVersion) != json.contains(KeyBaseHash))
    {
        return std::nullopt;
    }
    if (json.contains(KeyBaseVersion))
    {
        qint64 version = 0;
        QString hash;
        if (!StrictJson::readInt64(json, KeyBaseVersion, version)
            || !StrictJson::readString(json, KeyBaseHash, hash))
        {
            return std::nullopt;
        }
        record.baseHash = HexCodec::decode(hash, DocumentCrypto::PayloadHashSize);
        if (record.baseHash.isEmpty())
        {
            return std::nullopt;
        }
        record.baseVersion = version;
    }

    return record;
}

bool DocumentRecord::operator==(const DocumentRecord& other) const
{
    return baseVersion == other.baseVersion
        && baseHash == other.baseHash
        && baseSaveCounter == other.baseSaveCounter
        && historySeen == other.historySeen;
}
