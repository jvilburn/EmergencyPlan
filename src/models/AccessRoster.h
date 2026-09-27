#pragma once

#include "CryptoTypes.h"
#include "RosterPerson.h"

#include <QHash>
#include <QJsonObject>
#include <QList>

#include <optional>

/// Who may open this document, and on which devices.
///
/// The roster lives inside the payload, so only a member can read it. That is
/// what makes revocation possible: a header entry carries only a fingerprint,
/// and you cannot seal a key to a fingerprint - the roster is where the public
/// key to seal to is found.
class AccessRoster
{
public:
    AccessRoster() = default;

    /// Refused when the member id is already here - a re-granted person always
    /// gets a fresh member id, so ids are never reused - or when any of the
    /// person's devices is refused as addDevice would refuse it.
    bool addPerson(const RosterPerson& person);

    bool removePerson(const MemberId& member);
    bool contains(const MemberId& member) const;
    int personCount() const { return int(m_people.size()); }
    bool isEmpty() const { return m_people.isEmpty(); }

    /// Refused when the fingerprint already belongs to another person - one
    /// device answers to one person, or it could act with another person's
    /// rights, an admin's included - or when its public key or verify key already
    /// belongs to any other device.
    ///
    /// The key rule mirrors the header's: every write reseals from the roster, so
    /// a roster the header could not represent would make every later version
    /// fail the membership chain. One granted request carrying a copied public
    /// key would otherwise be enough to stop a ward publishing.
    bool addDevice(const MemberId& member, const RosterDevice& device);

    /// Refused for a person's last device: that is removing the person.
    bool removeDevice(const MemberId& member, const Fingerprint& fingerprint);

    bool setRole(const MemberId& member, RosterPerson::Role role);

    /// The admins' member ids in ascending order of hex - exactly as a header lists
    /// them, so the two can be compared directly.
    QList<MemberId> admins() const;

    std::optional<RosterPerson> person(const MemberId& member) const;
    std::optional<RosterPerson> personFor(const Fingerprint& fingerprint) const;
    std::optional<RosterDevice> deviceFor(const Fingerprint& fingerprint) const;

    /// Sorted by display name, then member id, so two people sharing a name are
    /// still ordered the same way on every device.
    QList<RosterPerson> people() const;

    QJsonObject toJson() const;

    /// Returns nullopt when any person or device is missing or ill-formed. A
    /// roster read in part is worse than none: it would name fewer people than
    /// the document actually grants.
    static std::optional<AccessRoster> fromJson(const QJsonObject& json);

    bool operator==(const AccessRoster& other) const;
    bool operator!=(const AccessRoster& other) const { return !(*this == other); }

private:
    QList<RosterDevice> allDevices() const;

    /// Keyed by member id hex, so uniqueness by person is the container's job.
    QHash<QString, RosterPerson> m_people;
};
