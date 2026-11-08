#include "DeviceState.h"
#include "Base64Url.h"
#include "Crypto.h"

#include <QJsonArray>

QJsonObject DeviceState::toJson() const
{
    QJsonObject json;
    json["journal"] = journal.toJson();
    if (baseVersion.has_value())
    {
        json["baseVersion"] = *baseVersion;
    }
    if (dismissedAccess.has_value())
    {
        json["dismissedAccess"] = dismissedAccess->toJson();
    }
    if (singleHolderNoticeDismissed)
    {
        json["singleHolderNoticeDismissed"] = true;
    }
    if (singleAdminNoticeDismissed)
    {
        json["singleAdminNoticeDismissed"] = true;
    }
    if (noLongerShared)
    {
        json["noLongerShared"] = true;
    }
    if (!inviteSeeds.isEmpty())
    {
        QJsonArray seeds;
        for (const QByteArray& seed : inviteSeeds)
        {
            seeds.append(Base64Url::encode(seed));
        }
        json["inviteSeeds"] = seeds;
    }
    return json;
}

DeviceState DeviceState::fromJson(JsonReader json)
{
    DeviceState device;
    device.journal = Journal::fromJson(json.object("journal"));
    device.baseVersion = json.optionalInteger("baseVersion");
    std::optional<JsonReader> dismissedAccess = json.optionalObject("dismissedAccess");
    if (dismissedAccess.has_value())
    {
        device.dismissedAccess = AccessList::fromJson(*dismissedAccess);
    }
    device.singleHolderNoticeDismissed = json.optionalFlag("singleHolderNoticeDismissed");
    device.singleAdminNoticeDismissed = json.optionalFlag("singleAdminNoticeDismissed");
    device.noLongerShared = json.optionalFlag("noLongerShared");
    device.inviteSeeds = json.optionalBytesList("inviteSeeds", Crypto::KeyBytes);
    json.finish();
    return device;
}

bool DeviceState::operator==(const DeviceState& other) const
{
    return journal == other.journal
        && baseVersion == other.baseVersion
        && dismissedAccess == other.dismissedAccess
        && singleHolderNoticeDismissed == other.singleHolderNoticeDismissed
        && singleAdminNoticeDismissed == other.singleAdminNoticeDismissed
        && noLongerShared == other.noLongerShared
        && inviteSeeds == other.inviteSeeds;
}
