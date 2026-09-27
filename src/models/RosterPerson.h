#pragma once

#include "CryptoTypes.h"
#include "RosterDevice.h"

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/// Someone with access to the document: who they are, their role, and the
/// devices they use.
///
/// Adding Bob gives Bob access; Bob's laptop and phone are two devices under one
/// person, added by Bob himself.
class RosterPerson
{
public:
    /// An admin grants, removes, unlinks another person's device, and changes
    /// roles, alone. A member reads and edits the document and manages their own
    /// devices.
    enum class Role
    {
        Admin,
        Member
    };

    RosterPerson() = default;

    /// A person always has at least one device, so there is no way to construct
    /// one without it.
    static RosterPerson create(const MemberId& member,
                               const QString& displayName,
                               const RosterDevice& firstDevice,
                               Role role);

    const MemberId& member() const { return m_member; }
    const QString& displayName() const { return m_displayName; }
    Role role() const { return m_role; }
    bool isAdmin() const { return m_role == Role::Admin; }

    void setDisplayName(const QString& displayName) { m_displayName = displayName; }
    void setRole(Role role) { m_role = role; }

    /// Sorted by device name, then by fingerprint so two devices sharing a name
    /// still order the same way every time.
    QList<RosterDevice> devices() const;

    int deviceCount() const { return int(m_devices.size()); }
    bool hasDevice(const Fingerprint& fingerprint) const;
    std::optional<RosterDevice> deviceFor(const Fingerprint& fingerprint) const;

    /// Idempotent: adding a device already here leaves one. Refused only when the
    /// device itself is not valid.
    bool addDevice(const RosterDevice& device);

    /// Refused for the last device, because that is removing the person, which
    /// takes a different route and an admin.
    bool removeDevice(const Fingerprint& fingerprint);

    /// A member id, a name, a role, at least one device, and every device valid -
    /// exactly what fromJson requires.
    bool isValid() const;

    QJsonObject toJson() const;
    static std::optional<RosterPerson> fromJson(const QJsonObject& json);

    bool operator==(const RosterPerson& other) const;
    bool operator!=(const RosterPerson& other) const { return !(*this == other); }

private:
    MemberId m_member;
    QString m_displayName;
    Role m_role = Role::Member;
    QList<RosterDevice> m_devices;
};
