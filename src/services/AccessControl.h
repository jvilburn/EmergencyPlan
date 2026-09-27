#pragma once

#include "AccessRoster.h"
#include "CryptoTypes.h"
#include "DocumentHeader.h"

#include <optional>

/// The result of a membership change: the header, roster, and data key the next
/// write should use.
///
/// The header is the start of a write, not a finished one. Its recipients, admin
/// list, key generation, lastWriter, and writer keys are set. The nonce, write
/// time, payload hash, and signature are cleared - the save path must produce each
/// of them afresh, and a save path that forgets one fails on it rather than
/// quietly reusing the previous write's value. For the nonce that would be
/// catastrophic. The save counter is the previous write's; the save path advances
/// it.
struct AccessChange
{
    DocumentHeader header;
    AccessRoster roster;
    DataKey dataKey;    ///< new on remove and unlink; unchanged otherwise
};

/// Membership changes as pure functions over a header and its roster.
///
/// Every operation takes the writer - the device that will sign the resulting
/// version - and acts as that device's person. Tying a change to its signer is
/// what lets each operation refuse everything the membership chain would refuse:
/// an operation that returns a change has produced one that, finished and signed
/// by that writer, links.
///
/// None of them touch a file, a server, or a key store.
///
/// **One change per published version.** Remove and unlink each advance the key
/// generation by one, and the chain refuses a step of two, so a change is built on
/// the server's accepted version and published before the next is built on it.
class AccessControl
{
public:
    /// Adds a new person, as a member, with exactly one device. The writer's person
    /// is an admin. The data key and generation are unchanged: the new person may
    /// read what came before, which is the point of being granted access to an
    /// existing ward.
    ///
    /// Refused when the member id is already on the roster, when the person carries
    /// anything but exactly one device, when that device collides with one already
    /// here, or when anything about the person would not read back from the payload
    /// exactly as written. dataKey is taken on trust: nothing here can confirm it is
    /// the key the header carries without the writer's secret key.
    static std::optional<AccessChange> grant(const DocumentHeader& header,
                                             const AccessRoster& roster,
                                             const DataKey& dataKey,
                                             const Fingerprint& writer,
                                             const RosterPerson& newPerson);

    /// Adds a new device under the writer's own person, whatever their role. Data
    /// key and generation unchanged. Refused for a device already on the roster -
    /// renaming is not linking - for one sharing a key with a device already here,
    /// and for one that would not read back exactly as written.
    static std::optional<AccessChange> linkDevice(const DocumentHeader& header,
                                                  const AccessRoster& roster,
                                                  const DataKey& dataKey,
                                                  const Fingerprint& writer,
                                                  const RosterDevice& newDevice);

    /// Drops one device and rotates the key, so it reads nothing written
    /// afterwards: one of the writer's own, or - when the writer is an admin -
    /// anyone's, such as a phone reported stolen. Refused for the writing device
    /// itself, since a version's writer is among its recipients, and for a
    /// person's last device, which is removing the person.
    static std::optional<AccessChange> unlinkDevice(const DocumentHeader& header,
                                                    const AccessRoster& roster,
                                                    const Fingerprint& writer,
                                                    const Fingerprint& device);

    /// Removes a person and every one of their devices, and rotates the key. The
    /// writer's person is an admin and is not the target - so an admin always
    /// remains. Another admin may be removed this way.
    static std::optional<AccessChange> remove(const DocumentHeader& header,
                                              const AccessRoster& roster,
                                              const Fingerprint& writer,
                                              const MemberId& target);

    /// Makes a person an admin or a member, the writer's own person included. The
    /// writer's person is an admin, and an admin remains. Who may read is
    /// unchanged, so the key is too.
    static std::optional<AccessChange> setRole(const DocumentHeader& header,
                                               const AccessRoster& roster,
                                               const DataKey& dataKey,
                                               const Fingerprint& writer,
                                               const MemberId& person,
                                               RosterPerson::Role role);

    /// Drops the writer's own person. Refused for the last admin while anyone else
    /// remains; the last person's leave lists no one. Does not rotate: the leaver
    /// would generate the new key and so know it.
    static std::optional<AccessChange> leave(const DocumentHeader& header,
                                             const AccessRoster& roster,
                                             const DataKey& dataKey,
                                             const Fingerprint& writer);

    /// Rebuilds the recipients and the admin list from the roster, sealing dataKey
    /// to every device. Every write reseals, so a sealed key written wrong once does
    /// not lock its owner out of every later version. nullopt when dataKey is
    /// invalid or any device cannot be sealed to, rather than writing an entry
    /// nobody can open.
    static std::optional<DocumentHeader> resealAll(const DocumentHeader& header,
                                                   const AccessRoster& roster,
                                                   const DataKey& dataKey);

    /// True only when every recipient entry has a roster device with the same
    /// fingerprint, member id, and keys, every roster device has exactly one entry,
    /// and the header's admin list is exactly the roster's admins. The one agreement
    /// the server cannot check, since it never sees the roster - so every open and
    /// every download checks it, and a version that fails is refused.
    static bool headerMatchesRoster(const DocumentHeader& header, const AccessRoster& roster);
};
