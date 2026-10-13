# Users hold access, devices hold users

Settled in discussion 2026-10-13. The server is not written yet, so its contract
changes freely.

## The idea

A person is invited to a ward. A device is added to a person. Two invitations, two
documents:

- A **ward document** lists the users who have access. Its data key is sealed to each
  user's key.
- A **user document** lists that user's devices. Its data key is sealed to each
  device's key, and its payload carries the user's secret key.

A device opens a ward in two steps: its device key opens its user document, and the
user key from there opens the ward. A user in a ward with three devices has all three
in the ward, and a user joining a second ward brings all three with them.

Wards list users; nothing lists wards. Which wards a user is in comes from the ward
headers, through the server (see "Finding a user's wards").

## What the server holds

Every document, ward or user, is the same thing to the server: a versioned blob and a
plaintext header listing recipients by fingerprint, each with the data key sealed to
it. One rule for writes: the signer is among the current header's recipients, and the
write builds on the current version.

- A ward header's recipients are user keys and open user invites.
- A user document header's recipients are device keys and open device invites.

Calls:

- `register`, `head`, `download`, `upload`: as in `ServerContract.h`.
- `uploadBatch`: several uploads, each conditional on its own base version, applied
  all or none. Used by unlink (see "Unlinking a device").
- `documentsSealedTo`: signed by a key, answers the ids of documents whose current
  header lists that key's fingerprint.
- `takeLease` / `releaseLease`: signed by the user key, carrying a device tag inside
  the signed bytes. The holder is the pair (user, device tag), so two devices of one
  user see each other's lease. The lease is advisory; only the user's own devices
  could forge the tag.

## The two payloads

```cpp
// In a ward document, beside the directory and the emergency response.
struct User
{
    UserId id;
    QString name;                 // chosen by the user when they joined
    Role role;
    UserKeys keys;                // public halves: sealing and verify
};

struct UserInvite
{
    UserId maker;
    Role role;
    QString label;                // "for Bob"
    UserKeys keys;                // from the link's seed
};

class WardRoster
{
    QHash<QString, User> m_users;               // by user id hex
    QHash<QString, UserInvite> m_invites;       // by fingerprint hex
};
```

```cpp
// The whole payload of a user document.
struct Device
{
    QString name;
    DeviceKeys keys;
};

struct DeviceInvite
{
    DeviceKeys keys;              // from the link's seed; the joiner brings its name
};

class UserDocument
{
    UserId m_id;
    QString m_name;               // offered as the name when joining a ward
    UserSecretKeys m_keys;        // the secret halves every device of this user holds
    QHash<QString, Device> m_devices;           // by fingerprint hex
    QHash<QString, DeviceInvite> m_invites;     // by fingerprint hex
};
```

Each header must seal to exactly its payload's recipients, as
`Reconcile::headerMatchesSealingSet` checks now. A ward never has fewer than one user;
a user document never has fewer than one device.

## This device

The identity file holds the device's own keypairs, its device name, and, once it has
one, the id of its user document. The user document is kept on the device the way a
ward is, as a local file and a base copy, so the device opens its wards offline.

First run names the device. A device has no user until it first needs one, which is
the first of:

- **Accepting a user invite** to a ward.
- **Creating a ward.**
- **Opening a device link**, which joins it to an existing user instead (see below).

For the first two, the device makes its user document then: a fresh user keypair,
sealed to this device alone, which makes it the user's first device. The user
document stays local until its first device invite, the way a ward stays local until
it is shared.

A device that has a user joins wards as that user, and every device of the user comes
with it.

## The links

```
wardplanning:invite/<ward id>/<seed>      a user invite to a ward
wardplanning:device/<user document id>/<seed>   a device invite to a user
```

The prefix says which flow the link starts, so a fresh device knows before any fetch
whether it is joining a ward or a user. Both keep `InviteLink`'s shape: a document id
and a 32-byte seed from which the invite's keypairs are derived.

## Joining

Both joins are the flow `DocumentSession::join` has now, on their own document:

1. Fetch the current version as the invite and validate it.
2. Put the joiner in the invite's place: a `User` with this user's public keys, for a
   ward; a `Device` with this device's keys and name, for a user document.
3. Reseal (the invite's fingerprint is gone, so the data key is fresh) and upload,
   signed by the invite.
4. Whatever the answer, download as the joiner. A header that names the joiner means
   the join landed; one that does not means the invite was spent.

A link opened again after a lost answer finishes the same way: the fetch as the invite
finds it spent and step 4 settles it.

After a device joins its user, it asks `documentsSealedTo` with the user key and
downloads each ward.

## Leaving and removing

- **Removing a user from a ward**: an admin writes the ward without them. The reseal
  rule gives it a fresh data key.
- **Leaving a ward**: this user writes the ward without themselves, built in memory, as
  `DocumentSession::leave` does now.
- A device that finds a ward's header no longer lists its user drops that ward:
  Removed when another user published the version, Left when its own user did.

## Unlinking a device

Any device of the user may unlink another. A lost phone holds the user key, so the
user key changes:

1. Build the user document without the device, with a fresh user keypair. It reseals
   to the remaining devices under a fresh data key.
2. For each ward from `documentsSealedTo`, build the next version with this user's
   keys replaced by the new public halves. Each reseals under a fresh data key, since
   the old user fingerprint is gone.
3. Send all of it as one `uploadBatch`, signed by this device for the user document
   and by the old user key for each ward.

All or none: no ward is ever sealed to a key the user's other devices do not have yet,
and no ward is left readable by the unlinked device. A conflict on any document means
fetching what moved, merging, and building the batch again.

The unlinked device finds its user document no longer lists it. It drops the user
document and every ward, and the device starts over at first run.

The user's other devices fetch the user document before any ward, so they hold the new
user key by the time a ward needs it.

## Merging

Both rosters merge three ways, entry by entry, against the base copy:

- An entry changed on one side takes that side's value.
- An entry removed on one side and changed on the other is removed. Removal is the
  protective act.
- A ward user's name, role and keys merge as separate fields. Names are never edited,
  two roles from the same base agree, and keys change only by an unlink, which goes up
  in a batch, so no field clashes.
- Invites are only added or removed.
- A merge that would leave nobody - no user in a ward, no device in a user document -
  takes the server's side whole.

A rotation that crosses another user's edit to a ward merges as a change on the
server's side: the other user's next publish meets a version conflict, merges, and
keeps the new keys.

## What leaves the ward

Devices are the user document's concern alone. The ward roster carries no devices, so
these go with them: device invites in wards, `removeDevice` on a ward, the last-device
rule in wards, the per-device merge in wards, `LostAccess::Unlinked`, and device rows in
Who Has Access. Devices get their own dialog, My Devices, with Add a Device and Unlink.

`Holder` becomes `User`, `MemberId` becomes `UserId`, and the `Invite::For` enum gives
way to the two invite types.

## Open

- **Former keys.** The `formerKeys` hashes let a device refuse a version that does not
  descend from one it holds, which defends against the server making a ward of its own.
  To be decided whether that stays, for both document kinds.
- **Role rules.** Where the admin, last-admin and not-yourself checks live: today in
  `DocumentManager`, `AccessDialog` and `MainWindow`.
- **A notice for the user document.** Whether a device added to this user is shown, the
  way a change to who has access is shown above a ward.
- **Shared sync code.** `ServerVersion`, `Reconcile` and the base-copy rules are written
  against the ward's `Document`. The plan decides how the user document reuses them.
