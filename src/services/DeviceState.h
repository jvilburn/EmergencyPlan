#pragma once

#include "Journal.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <optional>

/// What belongs to this device alone, saved in its local file beside the ward and never in
/// a version another device reads.
struct DeviceState
{
    Journal journal;                     ///< with its sequence counter
    QSet<QByteArray> heldRecipients;     ///< the fingerprints the version this device holds seals to
    QList<QByteArray> inviteSeeds;       ///< the seed of each open invite this device made, while the access list names it
    std::optional<QByteArray> joinSeed;  ///< the seed of the invite this device joined through, for as long as it holds the ward
    bool noLongerShared = false;         ///< a version this device took left it out, so its copy is read-only

    QJsonObject toJson() const;
    static DeviceState fromJson(JsonReader json);

    bool operator==(const DeviceState& other) const;
};
