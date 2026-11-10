#pragma once

#include "Journal.h"
#include "JsonReader.h"

#include <QJsonObject>

/// What belongs to this device alone, saved in its local file beside the ward and never in
/// a version on the server.
struct DeviceState
{
    Journal journal;  ///< with its sequence counter

    QJsonObject toJson() const;
    static DeviceState fromJson(JsonReader json);

    bool operator==(const DeviceState& other) const;
};
