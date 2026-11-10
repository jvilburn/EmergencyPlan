#include "DeviceState.h"

QJsonObject DeviceState::toJson() const
{
    QJsonObject json;
    json["journal"] = journal.toJson();
    return json;
}

DeviceState DeviceState::fromJson(JsonReader json)
{
    DeviceState device;
    device.journal = Journal::fromJson(json.object("journal"));
    json.finish();
    return device;
}

bool DeviceState::operator==(const DeviceState& other) const
{
    return journal == other.journal;
}
