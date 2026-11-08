#pragma once

#include "DeviceState.h"
#include "Document.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <optional>

/// The two kinds of payload. They differ in one part: a local file carries this device's own
/// state under device, and a version on the server carries none.
enum class PayloadKind
{
    LocalFile,
    Version
};

/// The decrypted, inflated payload of a ward file or a version.
struct WardPayload
{
    Document ward;                      ///< the ward part, with the access part as its access list
    QHash<QByteArray, qint64> landed;   ///< each device's fingerprint to its landed mark
    std::optional<DeviceState> device;  ///< a local file's own state; a version has none

    QJsonObject toJson() const;

    /// The payload json holds, read as kind: a local file must carry device, and a
    /// version must not.
    static WardPayload fromJson(JsonReader json, PayloadKind kind);

    bool operator==(const WardPayload& other) const;
};
