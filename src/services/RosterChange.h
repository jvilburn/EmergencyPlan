#pragma once

#include "CryptoTypes.h"
#include "RosterDevice.h"
#include "RosterPerson.h"

#include <QJsonObject>

#include <optional>

/// One change to who has access to a ward, waiting to be published.
///
/// Every roster change goes through RosterChangeQueue - grant, link and unlink,
/// removal, role changes, and leaving - and each is kept in its ward's
/// DocumentRecord until the server accepts it, so a change survives the app
/// closing before it reached the server. The record it is kept in says which
/// ward it is for.
struct RosterChange
{
    enum class Kind
    {
        Grant,      ///< person: the new person
        Link,       ///< device: a device joining this device's own person
        Unlink,     ///< device: the device to drop
        Remove,     ///< target: the person to remove
        SetRole,    ///< target and role
        Leave       ///< this device's person leaves
    };

    Kind kind = Kind::Leave;
    std::optional<RosterPerson> person;
    std::optional<RosterDevice> device;
    std::optional<MemberId> target;
    std::optional<RosterPerson::Role> role;

    QJsonObject toJson() const;

    /// Exactly what toJson writes: the fields its kind uses and no others, each
    /// valid. A change read in part would publish something nobody asked for.
    static std::optional<RosterChange> fromJson(const QJsonObject& json);

    bool operator==(const RosterChange& other) const;
    bool operator!=(const RosterChange& other) const { return !(*this == other); }
};
