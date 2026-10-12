#pragma once

#include "CryptoTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

/// A device of a person: its keys and the name it was given.
struct NamedDevice
{
    QString name;
    DeviceKeys keys;

    Fingerprint fingerprint() const;

    bool operator==(const NamedDevice& other) const { return name == other.name && keys == other.keys; }
    bool operator!=(const NamedDevice& other) const { return !(*this == other); }
};

/// One person with access, a holder of the key: their devices hold it.
struct Holder
{
    MemberId id;
    QString name;
    Role role = Role::Member;
    QList<NamedDevice> devices;         ///< sorted by fingerprint hex

    bool isAdmin() const { return role == Role::Admin; }
    bool operator==(const Holder& other) const;
    bool operator!=(const Holder& other) const { return !(*this == other); }
};

/// A one-time invite: a keypair the data key is sealed to until someone joins with
/// it. Whoever holds the seed the keypair was made from can read the ward and write
/// the version that puts their own device in the invite's place.
struct Invite
{
    enum class For
    {
        Device,     ///< joins its maker as another device, with the maker's name and role
        User        ///< makes a new person, named by the joiner, with role
    };

    For purpose = For::User;
    MemberId maker;                     ///< a Device invite joins its maker
    Role role = Role::Member;           ///< what a User invite's joiner becomes; a Device joins as its maker
    QString label;                      ///< shown while the invite is open: "for Bob"
    DeviceKeys keys;

    Fingerprint fingerprint() const;

    bool operator==(const Invite& other) const;
    bool operator!=(const Invite& other) const { return !(*this == other); }
};

/// Who has access to this document, carried inside the payload: each person with a
/// name and a role, each of their devices with its keys, and each invite still open.
///
/// This is the whole access list. The header seals the data key to every device and
/// invite here and to nothing else, and the server sees only those fingerprints. A
/// person is added by an invite they join with, removed by a write without them, and
/// nothing else changes who can read.
///
/// It also carries a hash of every data key the ward used before its current one, so
/// a version can show it descends from one this device already holds: only someone
/// who held an earlier key can name it, and the server never sees these hashes.
class SealingSet
{
public:
    SealingSet() = default;

    /// A new ward's set: its maker, an admin, on one device.
    static SealingSet of(const MemberId& person, const QString& name, const NamedDevice& device);

    // --- Reading ---

    /// Sorted by person id hex, so two sets of the same state compare equal.
    QList<Holder> people() const;
    std::optional<Holder> person(const MemberId& id) const;

    /// The person a device belongs to; nullopt for an invite or an unknown fingerprint.
    std::optional<Holder> personOf(const Fingerprint& device) const;
    std::optional<NamedDevice> device(const Fingerprint& fingerprint) const;

    /// Sorted by fingerprint hex.
    QList<Invite> invites() const;
    std::optional<Invite> invite(const Fingerprint& fingerprint) const;

    int adminCount() const;

    /// Every device and every invite, sorted by fingerprint hex: what the header seals
    /// to, in the order it lists them.
    QList<DeviceKeys> recipients() const;

    // --- Writing: each refused, and nothing changed, when it would leave the set
    // inconsistent: a key shared between two entries, a person with no device, an
    // unknown person or invite.

    bool addInvite(const Invite& invite);
    bool removeInvite(const Fingerprint& invite);

    /// The joiner's device takes the invite's place: under the invite's maker for a
    /// Device invite, with the maker's role; for a User invite under a new person,
    /// joiner, named name, with the invite's role.
    bool join(const Fingerprint& invite, const NamedDevice& device, const MemberId& joiner, const QString& name);

    /// Never the last person: a ward with nobody to read it is not a state. The
    /// person's invites go with them.
    bool removePerson(const MemberId& id);

    /// Never a person's last device; removePerson for that.
    bool removeDevice(const Fingerprint& device);

    /// Refused for an unknown person, and for the role they have.
    bool setRole(const MemberId& id, Role role);

    /// Three ways: mine and theirs each built on base. An entry - a person with their
    /// devices and role, or an invite - changed on one side is taken from that side;
    /// one changed differently on both is theirs, which the server holds. A removed
    /// person's invites go with them, as in removePerson. The former keys are theirs
    /// with mine's appended, so the merged set descends from both. Where removals on
    /// both sides leave nobody, theirs stands whole. This is what lets an invite made
    /// here and a join landing there cross without troubling anyone.
    static SealingSet merge(const SealingSet& base, const SealingSet& mine, const SealingSet& theirs);

    // --- The rule ---

    /// Whether a write of this set on top of a version that sealed to previousRecipients
    /// needs a fresh data key: it does when that version sealed to anyone this set
    /// lacks, so a device or an invite taken out holds no key to anything written
    /// afterwards. previousRecipients holds fingerprint hex.
    bool rotatesFrom(const QSet<QString>& previousRecipients) const;

    /// Records the key this ward used until now, at a rotation; once for any key.
    void addFormerKey(const DataKey& key);

    /// Whether key is this ward's current key's ancestor.
    bool descendsFrom(const DataKey& key) const;

    QJsonObject toJson() const;

    /// nullopt for an empty set, a person without a device, a key appearing twice,
    /// or anything missing or ill-formed. A set read in part would seal to fewer
    /// devices than it names.
    static std::optional<SealingSet> fromJson(const QJsonObject& json);

    bool operator==(const SealingSet& other) const;
    bool operator!=(const SealingSet& other) const { return !(*this == other); }

private:
    /// Whether keys could be added without sharing a key with any entry here.
    bool keysAreFree(const DeviceKeys& keys) const;

    /// Where a device is: its person's key, and its index among their devices.
    struct DeviceAt
    {
        QString person;
        qsizetype index;
    };
    std::optional<DeviceAt> find(const Fingerprint& device) const;

    /// An invite is always some current person's: the one who made it.
    void dropInvitesWithoutAPerson();

    QHash<QString, Holder> m_people;        ///< by person id hex
    QHash<QString, Invite> m_invites;       ///< by fingerprint hex
    QStringList m_formerKeys;               ///< FormerKey hashes as hex, oldest first
};

