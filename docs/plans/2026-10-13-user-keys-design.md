# Users hold access, devices hold users

Settled in discussion 2026-10-13. The server is not written yet, so its contract
changes freely.

"Ward" here is the church unit; the document holding a ward's plan is a ward document.

## The idea

A person is invited to a ward document. A device is added to a person. Two
invitations, two kinds of document:

- A **ward document** lists the users who have access. Its data key is sealed to each
  user's key.
- A **user document** lists that user's devices. Its data key is sealed to each
  device's key, and its payload carries the user's secret key.

A device opens a ward document in two steps: its device key opens its user document,
and the user key from there opens the ward document. A user with three devices has all
three in every ward document they are in, and joining another ward document brings all
three with them.

The server decides who may read and write, from the documents' plaintext headers.
Member data reaches the server only as ciphertext, as church policy requires: the
encryption is there to keep plaintext off the server, and access control is the
server's job.

Ward documents list users; nothing lists ward documents. Which ward documents a device
may open comes from their headers, through the server.

## Identity

A user is identified by their user document: `UserId` is the user document's id.

The identity file holds the device's own keypairs, its device name, and, once it has
one, its user's id. The user document is kept on the device the way a ward document
is, as a local file and a base copy, so the device opens its ward documents offline.

First run names the device. A device has no user until it first needs one, which is
the first of:

- **Accepting a user invite** to a ward document.
- **Creating a ward document.**
- **Opening a device link**, which joins it to an existing user instead (see "Joining").

For the first two, the device makes its user document then: a fresh user keypair,
sealed to this device alone, which makes it the user's first device. The user
document is registered with the server before anything that names it reaches the
server: sharing a ward document, or joining one.

A device that has a user joins ward documents as that user, and every device of the
user comes with it.

## What the server holds

Every document is a versioned blob and a plaintext header. Each header entry carries
the document's data key sealed to one recipient. Each kind of document has its own
header, with entries named for what they are:

```cpp
struct WardHeader
{
    QList<UserEntry> users;             // user id + sealed key
    QList<UserInviteEntry> invites;     // fingerprint + sealed key
};

struct UserHeader
{
    QList<DeviceEntry> devices;         // fingerprint + sealed key
    QList<DeviceInviteEntry> invites;   // fingerprint + sealed key + expiry
};
```

Every call on a document is signed, reads included, and the server allows it by the
document's kind:

- **On a user document:** the signer is one of its devices, or one of its invites
  that has not expired.
- **On a ward document:** the signer is one of its invites, or a device of one of its
  users.

Writes also build on the current version.

A device invite expires a few minutes after it is made: it is used while both devices
are in hand, and the expiry bounds how long a copy left in an email or text admits
anyone. A user invite travels to someone who may open it days later, so it lasts until
it is used or withdrawn.

Calls:

- `register`, `head`, `download`, `upload`: as in `ServerContract.h`, with `head` and
  `download` signed. `head` answers the version alone, since a refusal already says
  this device has lost access; `download` answers the version, header and blob.
- `documentsFor`: signed by a device, answers the ids of its user document and of
  every ward document that names its user. A device joining its user finds its ward
  documents this way.
- `delete`: allowed as a write is, and removes the document. Every device then finds
  it gone and drops it.

## The two payloads

```cpp
// In a ward document, beside the directory and the emergency response.
struct User
{
    UserId id;                    // their user document's id
    QString name;                 // chosen by the user when they joined
    bool admin;                   // adds and removes users
    PublicKey key;                // the user's sealing key
};

struct UserInvite
{
    UserId maker;
    bool admin;                   // what the joiner becomes
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
    QString m_name;               // offered as the name when joining a ward document
    Keypair m_key;                // the user's sealing keypair, held by every device
    QHash<QString, Device> m_devices;           // by fingerprint hex
    QHash<QString, DeviceInvite> m_invites;     // by fingerprint hex
};
```

Each header has exactly one entry per access list entry, as
`Reconcile::headerMatchesSealingSet` checks now: `UserEntry` to `User` by user id,
`UserInviteEntry` to `UserInvite`, `DeviceEntry` to `Device` and `DeviceInviteEntry`
to `DeviceInvite` by fingerprint. A ward document always has a user; a user document
always has a device.

## Admin rules

Admin is the app's rule, kept by every honest copy of it, and lives only in the
access list; the server reads headers alone. Every user sees the whole access list,
admins marked, so a user who is not an admin knows whom to ask.

In a ward document, the actor being this user:

- **Invite a user**: an admin.
- **Remove a user**: an admin, removing someone else. Removing yourself is leaving.
- **Change someone's admin flag**: an admin. The last admin steps down only by making
  someone else an admin first.
- **Withdraw a user invite**: its maker, or an admin.
- **Leave**: anyone but the last admin while others remain, who makes someone else an
  admin first. The last user leaving deletes the ward document, after a warning that
  it is gone for good, on the server and on every device.
- **Delete the ward document**: an admin, for everyone.

In a user document, the actor being this device:

- **Add a device**, **withdraw a device invite**: any device.
- **Unlink a device**: any device, unlinking another; never the last.

Each action is one function on the model that takes the actor and answers the changed
list, or nothing when the actor may not:

```cpp
std::optional<AccessList> AccessList::removing(const UserId& actor, const UserId& user) const;
std::optional<AccessList> AccessList::settingAdmin(const UserId& actor, const UserId& user, bool admin) const;
std::optional<AccessList> AccessList::leaving(const UserId& actor) const;
```

and likewise `inviting` and `withdrawing`, and on `UserDocument` `addingDevice`,
`unlinking` and `withdrawing`. A dialog enables a button exactly when the call answers
a value, and the action publishes that value, so each rule is written once, beside
what it changes.

An unshared ward document is on this device alone; deleting it is local.

## The links

```
wardplanning:invite/<ward document id>/<seed>   a user invite to a ward document
wardplanning:device/<user id>/<seed>            a device invite to a user
```

The prefix says which flow the link starts, so a fresh device knows before any fetch
whether it is joining a ward document or a user. Both keep `InviteLink`'s shape: a
document id and a 32-byte seed from which the invite's keypairs are derived. The app
shows a device link as text to copy and as a QR code, for a phone to read.

## Joining

Both joins are the flow `DocumentSession::join` has now, on their own document:

1. Fetch the current version, signed by the invite, and validate it.
2. Put the joiner in the invite's place: a `User` with this user's id and key, for a
   ward document; a `Device` with this device's keys and name, for a user document.
   The data key stays; the invite's entry gives way to the joiner's.
3. Upload, signed by the invite.
4. Whatever the answer, download as the joiner. An allowed download whose header names
   the joiner means the join landed; a refused one means the invite was spent.

A link opened again after a lost answer finishes the same way: the fetch as the invite
is refused, since the invite is spent, and step 4 settles it. An expired device link
is refused at step 1, and the person makes a new one.

After a device joins its user, it asks `documentsFor` and downloads each ward document.

## Taking access away

Each is one write, and the server refuses the one taken out from then on:

- **Removing a user from a ward document**: an admin writes the ward document without
  them.
- **Leaving a ward document**: this user writes it without themselves, built in
  memory, as `DocumentSession::leave` does now. The last user deletes it instead.
- **Unlinking a device**: any device of the user writes the user document without it.
  The unlinked device is refused on the user document and on every ward document of
  its user.

A device refused on a ward document, or finding it deleted, drops it, with its copies,
and says it is no longer available to this person. A device refused on its user document drops the user
document and every ward document, and starts over at first run.

## Merging

Every device edits freely, since edits of any kind are rare, and every document merges
by last writer wins: on a version conflict, what changed on one side takes that side,
and what changed on both takes this device's, the later publisher's.

Most collections merge record by record: three ways against the base copy, by id,
through one generic merge. Absence is a value, so a removal is a change like any
other. That covers the directory (families, teams, tags, districts, groups, assets),
the emergency response, and both kinds of invite. The response's nested collections
are keyed by id, so contact attempts recorded on both sides are separate records and
both stay, and task categories are a keyed set. Two admins inviting different people
keep both invites. Two devices making the same automatic update, such as geocoding a
family, write the same value, which merges silently.

The user list in a ward document and the device list in a user document each merge
whole: changed on both sides, this device's list is taken as it stands. Each side's
list always has a user and an admin, or a device, since no single write removes the
last, so the merged one does too. The races it settles - two removals at once, a
leave crossing an admin change - are rare. A join never merges: one that crosses
another write meets a version conflict and starts again from the current version.

An invite whose maker was removed at the same moment stays in the list. It still
admits whoever opens it, an admin can withdraw it, and the dialog shows it without the
maker's name.

## What goes

- Devices in ward document access lists, device invites in ward documents,
  `removeDevice` on a ward document, the last-device rule there, and device rows in
  Who Has Access. Devices get their own dialog, My Devices, with Add a Device and
  Unlink.
- Fresh data keys on removal: `SealingSet::rotatesFrom`, `ServerVersion::reseal`, and
  the former-key hashes with `descendsFrom` and `trustedKeys`.
- The edit lease: the lease calls, `Lease`, `LeaseHeld`, the lease holder in `Head`,
  `advanceClock`, the heartbeat timer, the session's lease state, `isDirectoryReadOnly`,
  `mayWriteDirectory`, `Refusal::DirectoryReadOnly`, `readOnlyChanged`, and the
  window's read-only state.
- The directory merged as one value, and the separate merge rules for the access list
  and the response.
- The conflict dialog: `raiseConflict`, `resolve`, `pendingConflict`, `Resolution`, the
  recovery copy, and the pause of head checks and publishing while a conflict waits.
- `LostAccess` and `whyLost`: a refused device reads nothing more of the ward
  document, so it reports it gone without saying by whom.
- `Publication` in `Head` and `Snapshot`: who published a version and when, read only
  by the conflict dialog and `whyLost`.
- `Holder` becomes `User`, `MemberId` becomes `UserId`, `Role` becomes `bool admin`,
  and the `Invite::For` enum gives way to the two invite types.

## Open

- **A notice for the user document.** Whether a device added to this user is shown, the
  way a change to who has access is shown above a ward document.
- **Shared sync code.** `ServerVersion`, `Reconcile` and the base-copy rules are written
  against the ward document's `Document`. The plan decides how the user document reuses them.
