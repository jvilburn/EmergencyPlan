#pragma once

#include "CryptoTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

/// The devices this document's data key is sealed to, carried inside the payload.
///
/// This is the one piece of access state the server never controls: a client seals
/// only to devices listed here, or to devices it approved itself, so a server that
/// lies about who has access can deny service but never gain it. A header entry
/// carries only a fingerprint, and a key cannot be sealed to a fingerprint, so the
/// public keys live here.
///
/// It also carries a hash of every data key the ward used before its current one, so
/// a version can show it descends from one this device already holds: only someone
/// who held an earlier key can name it, and the server never sees these hashes.
///
/// It carries no names, member ids, or roles; those belong to the server.
class SealingSet
{
public:
    /// What the next write seals to, and whether it needs a fresh data key.
    struct Next;

    SealingSet() = default;

    /// A new ward's set: the device that made it. Empty when its keys are invalid.
    static SealingSet of(const DeviceKeys& firstDevice);

    /// Refused for invalid keys, and for a device sharing either key with another
    /// one here. Adding a device already here changes nothing.
    bool add(const DeviceKeys& device);

    bool contains(const Fingerprint& fingerprint) const;
    int size() const { return int(m_devices.size()); }
    bool isEmpty() const { return m_devices.isEmpty(); }

    /// Sorted by fingerprint hex, so every device seals in the same order.
    QList<DeviceKeys> devices() const;

    /// Records the key this ward used until now, at a rotation.
    void addFormerKey(const DataKey& key);

    /// Whether key is this ward's current key's ancestor.
    bool descendsFrom(const DataKey& key) const;

    /// The sealing-set rule. Keeps the devices allowList still lists; adds each
    /// approved device allowList lists; rotates when anything was dropped. The
    /// former keys carry over.
    ///
    /// Only approved devices are ever added - never a device because the server
    /// lists it - which is what keeps a hostile server from planting one.
    /// allowList holds fingerprint hex.
    Next next(const QSet<QString>& allowList, const QList<DeviceKeys>& approved) const;

    QJsonObject toJson() const;

    /// nullopt for an empty set, or any device missing, ill-formed, repeated, or
    /// sharing a key. A set read in part would seal to fewer devices than it names.
    static std::optional<SealingSet> fromJson(const QJsonObject& json);

    bool operator==(const SealingSet& other) const
    {
        return m_devices == other.m_devices && m_formerKeys == other.m_formerKeys;
    }
    bool operator!=(const SealingSet& other) const { return !(*this == other); }

private:
    /// Keyed by fingerprint hex.
    QHash<QString, DeviceKeys> m_devices;

    /// FormerKey hashes as hex, oldest first.
    QStringList m_formerKeys;
};

struct SealingSet::Next
{
    SealingSet devices;
    bool rotate = false;
};
