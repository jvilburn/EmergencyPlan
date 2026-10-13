# Users hold access, devices hold users

Settled in discussion 2026-10-13. The server is not written yet, so its contract
changes freely.

## The idea

A person is invited to a ward. A device is added to a person. Two invitations, two
kinds of document:

- A **ward document** lists the users who have access. Its data key is sealed to each
  user's key.
- A **user document** lists that user's devices. Its data key is sealed to each
  device's key, and its payload carries the user's secret key.

A device opens a ward in two steps: its device key opens its user document, and the
user key from there opens the ward. A user in a ward with three devices has all three
in the ward, and a user joining a second ward brings all three with them.

The server decides who may read and write, from the documents' plaintext headers.
Member data reaches the server only as ciphertext, as church policy requires: the
encryption is there to keep plaintext off the server, and access control is the
server's job.

Wards list users; nothing lists wards. Which wards a device may open comes from the
ward headers, through the server.

## Identity

A user is identified by their user document: `UserId` is the user document's id.

The identity file holds the device's own keypairs, its device name, and, once it has
one, its user's id. The user document is kept on the device the way a ward is, as a
local file and a base copy, so the device opens its wards offline.

First run names the device. A device has no user until it first needs one, which is
the first of:

- **Accepting a user invite** to a ward.
- **Creating a ward.**
- **Opening a device link**, which joins it to an existing user instead (see "Joining").

For the first two, the device makes its user document then: a fresh user keypair,
sealed to this device alone, which makes it the user's first device. The user
document is registered with the server before anything that names it reaches the
server: sharing a ward, or joining one.

A device that has a user joins wards as that user, and every device of the user comes
with it.

## What the server holds

Every document is a versioned blob and a plaintext header. The header's entries each
carry the document's data key, sealed to one recipient, and come in two kinds:

- a **key entry**, by fingerprint: a key that signs for itself. Devices and invites.
- a **user entry**, by user id: a user, whose devices sign for them.

A ward's header has a user entry per user and a key entry per open invite. A user
document's header has a key entry per device and per open device invite.

The one rule, for every call on a document, reads included: the call is signed, and
the signer is allowed when its fingerprint is a key entry in the document's header,
or a key entry in the header of a user the document's header names. Writes also build
on the current version.

Calls:

- `register`, `head`, `download`, `upload`: as in `ServerContract.h`, with `head` and
  `download` signed.
- `documentsFor`: signed by a device, answers the ids of its user document and of
  every ward that names its user. A device joining its user finds its wards this way.
- `takeLease` / `releaseLease`: signed by the device. The holder is that device.

A publication and a lease holder are reported as the device and the user it signed
for, so the client names the user from the ward's access list.

## The two payloads

```cpp
// In a ward document, beside the directory and the emergency response.
struct User
{
    UserId id;                    // their user document's id
    QString name;                 // chosen by the user when they joined
    Role role;
    PublicKey key;                // the user's sealing key
};

struct UserInvite
{
    UserId maker;
    Role role;
    QString label;                // "for Bob"
    DeviceKeys keys;              // from the link's seed
};

class AccessList
{
    QHash<QString, User> m_users;               // by user id
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
    QString m_name;               // offered as the name when joining a ward
    Keypair m_key;                // the user's sealing keypair, held by every device
    QHash<QString, Device> m_devices;           // by fingerprint hex
    QHash<QString, DeviceInvite> m_invites;     // by fingerprint hex
};
```

Each header has exactly one entry per access list entry, as
`Reconcile::headerMatchesSealingSet` checks now: user entries match users by id, key
entries match invites and devices by fingerprint. A ward always has a user; a user
document always has a device.

## The links

```
wardplanning:invite/<ward id>/<seed>            a user invite to a ward
wardplanning:device/<user id>/<seed>            a device invite to a user
```

The prefix says which flow the link starts, so a fresh device knows before any fetch
whether it is joining a ward or a user. Both keep `InviteLink`'s shape: a document id
and a 32-byte seed from which the invite's keypairs are derived.

## Joining

Both joins are the flow `DocumentSession::join` has now, on their own document:

1. Fetch the current version, signed by the invite, and validate it.
2. Put the joiner in the invite's place: a `User` with this user's id and key, for a
   ward; a `Device` with this device's keys and name, for a user document. The data
   key stays; the invite's entry gives way to the joiner's.
3. Upload, signed by the invite.
4. Whatever the answer, download as the joiner. An allowed download whose header names
   the joiner means the join landed; a refused one means the invite was spent.

A link opened again after a lost answer finishes the same way: the fetch as the invite
is refused, since the invite is spent, and step 4 settles it.

After a device joins its user, it asks `documentsFor` and downloads each ward.

## Taking access away

Each is one write, and the server refuses the one taken out from then on:

- **Removing a user from a ward**: an admin writes the ward without them.
- **Leaving a ward**: this user writes the ward without themselves, built in memory, as
  `DocumentSession::leave` does now.
- **Unlinking a device**: any device of the user writes the user document without it.
  The unlinked device is refused on the user document and on every ward of its user.

A device refused on a ward drops it, with its copies, and says the ward is no longer
available to this person. A device refused on its user document drops the user
document and every ward, and starts over at first run.

## Merging

Both access lists merge three ways, entry by entry, against the base copy:

- An entry changed on one side takes that side's value.
- An entry removed on one side and changed on the other is removed. Removal is the
  protective act.
- Every field holds a single value that only one side can change: names are chosen at
  join, the user key is fixed, and two roles changed from the same base agree. Entries
  are otherwise only added or removed.
- A merge that would leave nobody - no user in a ward, no device in a user document -
  takes the server's side whole.

## What goes

- Devices in ward access lists, device invites in wards, `removeDevice` on a ward, the
  last-device rule in wards, and device rows in Who Has Access. Devices get their own
  dialog, My Devices, with Add a Device and Unlink.
- Fresh data keys on removal: `SealingSet::rotatesFrom`, `ServerVersion::reseal`, and
  the former-key hashes with `descendsFrom` and `trustedKeys`.
- `LostAccess` and `whyLost`: a refused device reads nothing more of the ward, so it
  reports the ward gone without saying by whom.
- `Holder` becomes `User`, `MemberId` becomes `UserId`, and the `Invite::For` enum gives
  way to the two invite types.

## Open

- **Role rules.** Where the admin, last-admin and not-yourself checks live: today in
  `DocumentManager`, `AccessDialog` and `MainWindow`.
- **A notice for the user document.** Whether a device added to this user is shown, the
  way a change to who has access is shown above a ward.
- **Shared sync code.** `ServerVersion`, `Reconcile` and the base-copy rules are written
  against the ward's `Document`. The plan decides how the user document reuses them.
