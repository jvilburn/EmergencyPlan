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
const QLatin1String KeyHighWaterMark("highWaterMark");
const QLatin1String KeyKeyGeneration("keyGeneration");
const QLatin1String KeySaveCounter("saveCounter");
const QLatin1String KeyPendingChanges("pendingChanges");

} // namespace

QJsonObject DocumentRecord::toJson() const
{
    QJsonObject json;
    if (baseVersion.has_value())
    {
        json[KeyBaseVersion] = baseVersion.value();
        json[KeyBaseHash] = HexCodec::encode(baseHash);
    }

    QJsonObject mark;
    mark[KeyKeyGeneration] = highWaterMark.keyGeneration;
    mark[KeySaveCounter] = highWaterMark.saveCounter;
    json[KeyHighWaterMark] = mark;

    QJsonArray changes;
    for (const RosterChange& change : pendingChanges)
    {
        changes.append(change.toJson());
    }
    json[KeyPendingChanges] = changes;

    return json;
}

std::optional<DocumentRecord> DocumentRecord::fromJson(const QJsonObject& json)
{
    DocumentRecord record;
    QJsonObject mark;
    QList<QJsonObject> changes;
    if (!StrictJson::hasOnlyKeys(json, {KeyBaseVersion, KeyBaseHash, KeyHighWaterMark, KeyPendingChanges})
        || !StrictJson::readObject(json, KeyHighWaterMark, mark)
        || !StrictJson::hasOnlyKeys(mark, {KeyKeyGeneration, KeySaveCounter})
        || !StrictJson::readInt(mark, KeyKeyGeneration, record.highWaterMark.keyGeneration)
        || !StrictJson::readInt64(mark, KeySaveCounter, record.highWaterMark.saveCounter)
        || !StrictJson::readObjectArray(json, KeyPendingChanges, changes))
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

    for (const QJsonObject& changeJson : changes)
    {
        const std::optional<RosterChange> change = RosterChange::fromJson(changeJson);
        if (!change.has_value())
        {
            return std::nullopt;
        }
        record.pendingChanges.append(change.value());
    }

    return record;
}

bool DocumentRecord::operator==(const DocumentRecord& other) const
{
    return baseVersion == other.baseVersion
        && baseHash == other.baseHash
        && highWaterMark.keyGeneration == other.highWaterMark.keyGeneration
        && highWaterMark.saveCounter == other.highWaterMark.saveCounter
        && pendingChanges == other.pendingChanges;
}
