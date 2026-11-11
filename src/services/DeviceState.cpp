#include "DeviceState.h"
#include "Base64Url.h"
#include "Crypto.h"

#include <QJsonArray>
#include <QStringList>

QJsonObject DeviceState::toJson() const
{
    QJsonObject json;
    json["journal"] = journal.toJson();
    QStringList held;
    for (const QByteArray& fingerprint : heldRecipients)
    {
        held.append(Base64Url::encode(fingerprint));
    }
    held.sort();
    json["heldRecipients"] = QJsonArray::fromStringList(held);
    if (!inviteSeeds.isEmpty())
    {
        QJsonArray seeds;
        for (const QByteArray& seed : inviteSeeds)
        {
            seeds.append(Base64Url::encode(seed));
        }
        json["inviteSeeds"] = seeds;
    }
    if (joinSeed.has_value())
    {
        json["joinSeed"] = Base64Url::encode(*joinSeed);
    }
    if (noLongerShared)
    {
        json["noLongerShared"] = true;
    }
    return json;
}

DeviceState DeviceState::fromJson(JsonReader json)
{
    DeviceState device;
    device.journal = Journal::fromJson(json.object("journal"));
    const QList<QByteArray> held = json.bytesList("heldRecipients", Crypto::FingerprintBytes);
    device.heldRecipients = QSet<QByteArray>(held.begin(), held.end());
    device.inviteSeeds = json.optionalBytesList("inviteSeeds", Crypto::KeyBytes);
    device.joinSeed = json.optionalBytes("joinSeed", Crypto::KeyBytes);
    device.noLongerShared = json.optionalFlag("noLongerShared");
    json.finish();
    return device;
}

bool DeviceState::operator==(const DeviceState& other) const
{
    return journal == other.journal
        && heldRecipients == other.heldRecipients
        && inviteSeeds == other.inviteSeeds
        && joinSeed == other.joinSeed
        && noLongerShared == other.noLongerShared;
}
