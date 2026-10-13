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

For the first two, the device makes its user document then: it asks the person's name,
and makes a fresh user keypair, sealed to this device alone, which makes it the user's
first device. The name is asked this once and kept in the user document; every join
uses it. The user document is registered with the server before anything that names
it reaches the server: sharing a ward document, or joining one.

A device that has a user joins ward documents as that user, and every device of the
user comes with it. It belongs to one user: a device link opened on a device that
already has a user is refused, with "remove this device first".

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

A document's kind is fixed when it is registered. Every call on a document is signed,
reads included, and the server allows it by the document's kind:

- **On a user document:** the signer is one of its devices, or one of its invites
  that has not expired.
- **On a ward document:** the signer is one of its invites, or a device of one of its
  users.

Writes also build on the current version.

A device invite expires 30 minutes after it is made: long enough to send the link to
yourself and open it on the new device, and short enough that a copy left in an email
or text soon admits nobody. The expiry lives in the header alone, since only the
server enforces it. A user invite travels to someone who may open it days later, so it
lasts until it is used or withdrawn.

Calls:

- `register`: stores version 1 of a document under its client-made id, with its kind.
- `head`: the current version number. A refusal says this device has lost access.
- `download`: the current version, header and blob, as one consistent state.
- `upload`: the next version, conditional on the base version.
- `documentsFor`: signed by a device, answers the ids of its user document and of
  every ward document that names its user.
- `delete`: allowed as a write is, except for a signer that is an invite. The server
  keeps a record of the deleted id and answers later calls on it with `Deleted`.

Every answer carries the minimum app version the server accepts. A device below it
stops syncing and asks the person to update; its edits keep saving locally and publish
once it is updated. The minimum rises when a release changes a format.

## The two payloads

```cpp
// In a ward document, beside the directory and the emergency response.
struct User
{
    UserId id;                    // their user document's id
    QString name;                 // from their user document, when they joined
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
    QString m_name;               // the person's name, used when joining a ward document
    Keypair m_key;                // the user's sealing keypair, held by every device
    QHash<QString, Device> m_devices;           // by fingerprint hex
    QHash<QString, DeviceInvite> m_invites;     // by fingerprint hex
};
```

Each header has exactly one entry per access list entry: `UserEntry` to `User` by user
id, `UserInviteEntry` to `UserInvite`, `DeviceEntry` to `Device` and
`DeviceInviteEntry` to `DeviceInvite` by fingerprint. A ward document always has a
user; a user document always has a device.

## Admin rules

Admin is the app's rule, kept by every honest copy of it, and lives only in the
access list; the server reads headers alone. Every user sees the whole access list,
admins marked, so a user who is not an admin knows whom to ask.

In a ward document, the actor being this user:

- **Invite a user**: an admin.
- **Remove a user**: an admin, removing someone else. Removing yourself is leaving.
  The invites they made stay, shown without a maker's name, for an admin to withdraw.
- **Change someone's admin flag**: an admin. The last admin steps down only by making
  someone else an admin first.
- **Re-invite a user**: an admin, for someone who lost every device. One write removes
  their entry, so their lost devices are refused at once, and adds a user invite with
  their admin flag, labeled for them. They join as a new user. Each ward document's
  admins re-invite for their own ward document, and the old user document stays on
  the server, admitting nobody new.
- **Withdraw a user invite**: its maker, or an admin.
- **Leave**: anyone but the last admin while others remain, who makes someone else an
  admin first.
- **Delete the ward document**: an admin, for everyone. For the last user, Leave reads
  Delete Ward Document, with a warning that it is gone for good, on the server and on
  every device.

In a user document, the actor being this device:

- **Add a device**, **withdraw a device invite**: any device.
- **Unlink a device**: any device, the last excepted. Unlinking itself is Remove This
  Device: it then drops its local data, unshared ward documents included, and starts
  over at first run.

Each action is one function on the model that takes the actor and answers the changed
list, or nothing when the actor may not:

```cpp
std::optional<AccessList> AccessList::removing(const UserId& actor, const UserId& user) const;
std::optional<AccessList> AccessList::settingAdmin(const UserId& actor, const UserId& user, bool admin) const;
std::optional<AccessList> AccessList::leaving(const UserId& actor) const;
```

and likewise `inviting`, `reinviting`, `withdrawing` and `joining`, and on
`UserDocument` `addingDevice`, `unlinking`, `withdrawing` and `joining`. A dialog
enables a button exactly when the call answers a value, and the action publishes that
value, so each rule is written once, beside what it changes. Delete Ward Document asks
`deleting(actor)`, and Add a User asks `isAdmin(actor)`, the check `inviting` makes,
since an invite's keys are made only once the person asks for one.

A ward document whose only admin loses every device keeps its users, who read and edit
as before; managing access needs a new ward document. The notice for an only admin
nudges them to make a second one first.

An unshared ward document is on this device alone; deleting it is local.

Uninstalling deletes this device's local data: the identity file, the user document
and the ward documents. With its keys gone, the device's entry in the user document
admits nobody; it shows in My Devices until another device unlinks it.

## Notices

Two notices show what changed since the person last dismissed them, each read as the
difference between the list now and the list last dismissed:

- **Who has access**, above a ward document: users joining, leaving or removed, admin
  changes, and invites made.
- **Your devices**: a device added to this user or unlinked from it. Someone linking a
  device to you is the change most worth seeing.

## The links

```
wardplanning:user-invite/<ward document id>/<seed>   a user invite to a ward document
wardplanning:device-invite/<user id>/<seed>          a device invite to a user
```

Both hold the same data, a document id and a 32-byte seed from which the invite's
keypairs are derived, and differ only in the flow they start, so they are one type:

```cpp
struct InviteLink
{
    enum class Kind
    {
        User,
        Device
    };

    Kind kind;
    QString documentId;
    QByteArray seed;
};
```

The prefix carries the kind, so a fresh device knows before any fetch whether it is
joining a ward document or a user. The app shows a device link as text to copy and as
a QR code, for a phone to read.

## Joining

Both joins run the same steps on their own document:

1. Fetch the current version, signed by the invite, and validate it.
2. Put the joiner in the invite's place with `joining`: a `User` with this user's id,
   name and key, for a ward document; a `Device` with this device's keys and name, for
   a user document. The data key stays; the invite's entry gives way to the joiner's.
3. Upload, signed by the invite.
4. Unless the upload met a version conflict (start again from step 1) or the document
   is gone, download as the joiner. An allowed download whose header names the joiner
   means the join landed; a refused one means the invite was spent.

A link opened again after a lost answer finishes the same way: the fetch as the invite
is refused, since the invite is spent, and step 4 settles it. An expired device link
is refused at step 1, and the person makes a new one. A user invite to a ward document
this user is already in reports "you already have access" and leaves the invite open.

## Staying in sync

Every device head-checks its user document on the same schedule as its ward
documents, and asks `documentsFor` at startup and on each head check. A ward document
it does not hold yet is downloaded and added to its library, so a ward document joined
or shared on one device reaches the user's others.

A head check of the user document that finds an expired device invite writes the user
document without it. Two devices doing so at once remove the same record, which merges
to the same result.

## Taking access away

Each is one write, and the server refuses the one taken out from then on:

- **Removing a user from a ward document**: an admin writes the ward document without
  them.
- **Leaving a ward document**: this user writes it without themselves, built in
  memory, so the local file stays readable until the server holds the version.
- **Unlinking a device**: any device of the user writes the user document without it.
  The unlinked device is refused on the user document and on every ward document of
  its user.

A device refused on a ward document, or answered `Deleted`, drops it with its copies
and says it is no longer available to this person. A device refused on its user
document drops the user document and every ward document, unshared ones included, and
starts over at first run. An answer that a document is unknown is a fault: the device
keeps its copies and reports it.

## Merging

Every device edits freely, since edits of any kind are rare, and every document merges
by last writer wins: on a version conflict, what changed on one side takes that side,
and what changed on both takes this device's, the later publisher's.

Collections merge record by record: three ways against the base copy, by id, through
one generic merge. Absence is a value, so a removal is a change like any other. That
covers:

- the directory: families, teams, tags, districts, groups, assets, wards and stakes;
- the emergency response, whose nested collections are keyed by id;
- the users and invites of an access list, and the devices and invites of a user
  document.

Single values, such as the two import dates, take the same rule.

The response follows the rule like everything else: a deletion or an ending that
publishes second wins over the other side's records. Its contact attempts recorded on
both sides are separate records, so both stay. Task categories become a keyed set, and
a nested list merged by id is re-sorted newest first.

A merge that would leave a ward document with no user or no admin, or a user document
with no device, takes this device's list whole. Each side's list always has them,
since no single write removes the last, and this device is the later writer. Two
admins stepping down at once is the case it settles.

A join never merges: one that crosses another write meets a version conflict and
starts again from the current version. Two admins inviting different people keep both
invites. An invite whose maker was removed at the same moment stays, shown without a
maker's name.

Every device geocodes families missing coordinates. A geocode and a person's edit to
the same family at the same moment keep the later publisher's family; a person moved
out of a family while that family is edited elsewhere ends up in both. Both are
accepted: each needs two edits to one family at once.

## One sync engine

The ward document and the user document share one sync engine: the base copy, head
checks, fetching, validating, publishing, and merging on a version conflict. Its
per-kind parts are three: reading the payload, merging it, and checking the header
against the access list. The plan decides whether they arrive as a template or an
interface.
