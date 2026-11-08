#pragma once

#include "AccessList.h"
#include "Journal.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

/// What belongs to this device alone, saved in its local file beside the ward and never in
/// a version on the server.
struct DeviceState
{
    Journal journal;                            ///< with its sequence counter
    std::optional<AccessList> dismissedAccess;  ///< the access list as of the last dismissed access notice
    bool singleHolderNoticeDismissed = false;
    bool singleAdminNoticeDismissed = false;
    bool noLongerShared = false;                ///< the ward is no longer this device's
    QList<QByteArray> inviteSeeds;              ///< the seed of each open invite this device made

    QJsonObject toJson() const;
    static DeviceState fromJson(JsonReader json);

    bool operator==(const DeviceState& other) const;
};
