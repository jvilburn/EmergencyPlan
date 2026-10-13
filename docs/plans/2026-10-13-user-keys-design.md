# Users hold access, devices hold users

Settled in discussion 2026-10-13. The server is not written yet, so its contract
changes freely. "Settled questions" at the end records the decisions a review takes as
given.

"Ward" here is the church unit; the document holding a ward's plan is a ward document.

## The idea

A person is invited to a ward document. A device is added to a person. Two kinds of
document:

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

Every document lives on the server. A new document is registered when it is made, or
on the next connection when made offline, so it follows its user to every device.

## Words

Users and devices each have their own words, in the app and in the code alike:

| | Users (ward document) | Devices (user document) |
|---|---|---|
| Button | Invite a User | Add a Device |
| The open entry | `UserInvite` | `PendingDevice` |
| Making it | `inviting` | `addingDevice` |
| Accepting it | `joining` | `claiming` |
| Taking out another | Remove a User, `removingUser` | Remove a Device, `removingDevice` |
| Taking out yourself | Leave, `leaving` | Remove This Device |
| Withdrawing it | `withdrawingInvite` | `cancellingDevice` |

"Remove" always comes with its object, user or device.

## Identity

A user is identified by their user document: `UserId` is the user document's id.

The identity file holds the device's own keypairs and its device name. The device's
user is the user document it holds, kept at a fixed path in local app data as a local
file and a base copy, so the device opens its ward documents offline. A device with no
user document has no user.

First run names the device. A device has no user until it first needs one, which is
the first of:

- **Accepting a user invite** to a ward document.
- **Creating a ward document.**
- **Opening an add-device URL**, which makes it a device of an existing user instead
  (see "Joining and claiming").

For the first two, the device first asks:

> Do you already use Ward Planning on another device?
> **No, I'm new** / **Yes, add this device**

"No" asks the person's name and makes the user document: a fresh user keypair, sealed
to this device alone, which makes it the user's first device. The name is asked this
once and kept in the user document; every join uses it.

"Yes" says to use Add a Device on the other device and open what it gives here. A
device that was opened by a user invite keeps that invite's URL in its settings
meanwhile; once it has a user it accepts the invite on its own, and clears the URL
whether that succeeds or the invite turns out spent. Creating a ward document keeps
nothing: once added, the device receives the user's ward documents, and making a new
one is a click.

A device that has a user joins ward documents as that user, and every device of the
user comes with it. It belongs to one user: an add-device URL opened on a device that
already has a user is refused, with "remove this device first".

**Remove This Device** forgets everything on this device: the identity file, the user
document and the ward documents. It makes no server call and works on any device, the
last of its user included. With its keys gone, the device's entry in the user document
admits nobody; it shows in My Devices until another device removes it. Uninstalling
does the same: the installer deletes the app's local data.

## What the server holds

Every document is a versioned blob and a plaintext header. Each header entry carries
the document's data key sealed to one recipient. Each kind of document has its own
header, with entries named for what they are:

```cpp
struct WardHeader
{
    QList<UserEntry> users;                         // user id + sealed key
    QList<UserInviteEntry> invites;                 // fingerprint + sealed key
};

struct UserHeader
{
    QList<DeviceEntry> devices;                     // fingerprint + sealed key
    std::optional<PendingDeviceEntry> pending;      // fingerprint + sealed key
};
```

A header is built from its document's access list and data key. Sealing uses fresh
randomness, so a header's bytes change on every write; nothing compares headers byte
for byte.

A document's kind is fixed when it is registered. Every call on a document is signed,
reads and the first version included, and the server allows it by the document's
kind:

- **On a user document:** the signer is one of its devices, or its pending device
  while that has not expired.
- **On a ward document:** the signer is one of its invites, or a device of one of its
  users.

Writes also build on the current version. A ward header whose user entries name
anything but registered user documents is refused.

A pending device expires 30 minutes after it first appears in its user document's
header, timed by the server: long enough to send the URL to yourself and open it on
the new device, and short enough that a copy left in an email or text soon admits
nobody. A user invite travels to someone who may open it days later, so it lasts until
it is used or withdrawn.

Calls:

- `register`: stores version 1 of a document under its client-made id, with its kind.
- `check`: signed by a device, which sends the ids of the documents it holds. Each is
  answered with its current version, `Refused`, `Deleted`, or `Unknown`. The answer
  also lists every document this device may open and did not send.
- `download`: the current version, header and blob, as one consistent state.
- `upload`: the next version, conditional on the base version.
- `delete`: removes a ward document, allowed as a write is, except for a signer that
  is an invite. The server keeps a record of the deleted id and answers later calls on
  it with `Deleted`.

Every answer carries the minimum app version the server accepts. A device below it
stops syncing and asks the person to update; its edits keep saving locally and publish
once it is updated. The minimum rises when a release changes a format.

## Registering

A document with no base copy is not registered yet. On connecting, a device registers
its user document first, then any ward documents not yet registered, and only then
calls `check`, sending the ids of registered documents. Accepting a user invite
registers the user document first when it is not yet.

A `register` whose answer is lost is retried. A retry answered "id taken" is followed
by a download: allowed means the first `register` landed, and the device takes that
version as its base.

## The two payloads

```cpp
// In a ward document, beside the directory and the emergency response.
struct User
{
    UserId id;                    // their user document's id
    QString name;                 // from their user document, when they joined
    bool admin;                   // invites and removes users
    PublicKey key;                // the user's sealing key
};

struct UserInvite
{
    UserId maker;                 // shown in the list
    bool admin;                   // what the joiner becomes
    QString label;                // "for Bob"
    DeviceKeys keys;              // from the URL's seed
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

struct PendingDevice
{
    DeviceKeys keys;              // from the URL's seed; the claiming device brings its name
};

class UserDocument
{
    QString m_name;               // the person's name, used when joining a ward document
    Keypair m_key;                // the user's sealing keypair, held by every device
    QHash<QString, Device> m_devices;           // by fingerprint hex
    std::optional<PendingDevice> m_pending;     // at most one
};
```

Each header has exactly one entry per access list entry: `UserEntry` to `User` by user
id, `UserInviteEntry` to `UserInvite`, `DeviceEntry` to `Device` and
`PendingDeviceEntry` to `PendingDevice` by fingerprint. A ward document always has a
user; a user document always has a device. Adding a device while one is pending
replaces the pending one.

## Admin rules

Admin is the app's rule, kept by every honest copy of it, and lives only in the
access list; the server reads headers alone. Every user sees the whole access list,
admins marked, so a user who is not an admin knows whom to ask.

In a ward document, the actor being this user:

- **Invite a user**: an admin.
- **Remove a user**: an admin, removing someone else. Removing yourself is leaving.
  The invites they made stay, for an admin to withdraw.
- **Change someone's admin flag**: an admin. The last admin steps down only by making
  someone else an admin first.
- **Re-invite a user**: an admin, for someone who lost every device. One write removes
  their entry, so their lost devices are refused at once, and adds a user invite with
  their admin flag, labeled for them. They join as a new user. Each ward document's
  admins re-invite for their own ward document, and the old user document stays on
  the server, admitting nobody new.
- **Withdraw a user invite**: an admin.
- **Leave**: anyone but the last admin while others remain, who makes someone else an
  admin first.
- **Delete the ward document**: an admin, for everyone. For the last user, Leave reads
  Delete Ward Document, with a warning that it is gone for good, on the server and on
  every device.

In a user document, the actor being this device:

- **Add a device**, **cancel the pending device**: any device.
- **Remove a device**: any device, removing another; never the last.

Each action is one function on the model that takes the actor and answers the changed
list, or nothing when the actor may not:

```cpp
std::optional<AccessList> AccessList::removingUser(const UserId& actor, const UserId& user) const;
std::optional<AccessList> AccessList::settingAdmin(const UserId& actor, const UserId& user, bool admin) const;
std::optional<AccessList> AccessList::leaving(const UserId& actor) const;
```

and likewise `inviting`, `withdrawingInvite` and `joining`, and on `UserDocument`
`addingDevice`, `cancellingDevice`, `removingDevice` and `claiming`. A dialog enables
a button exactly when the call answers a value, and the action publishes that value,
so each rule is written once, beside what it changes. Re-invite is `removingUser` then
`inviting` on the result, published once.

Two buttons ask a question instead: Delete Ward Document asks `mayDelete(actor)`, and
Invite a User asks `isAdmin(actor)`, the check `inviting` makes, since an invite's
keys are made only once the person asks for one.

Every access action runs on the server's current version, the way a join does: fetch
it, apply the action, upload, and on a version conflict start again from the fetch. If
the action is no longer allowed on the current version, the person is told why. An
access action needs a connection: without one it changes nothing and says so. The
local copy takes in the published version through ordinary sync, so a local copy never
changes its own access list.

A ward document whose only admin loses every device keeps its users, who read and edit
as before; managing access needs a new ward document. The notice for an only admin
nudges them to make a second admin first.

## Notices

Two notices show what changed since the person last dismissed them, each read as the
difference between the list now and the list last dismissed. The baseline starts as
the list this device first took in, so a document just joined, made or downloaded
shows nothing new.

- **Who has access**, above a ward document: users joining, leaving or removed, admin
  changes, and invites made.
- **Your devices**: a device added to this user or removed from it. Someone adding a
  device to you is the change most worth seeing.

## The URLs

```
wardplanning:invite/<ward document id>/<seed>        a user invite to a ward document
wardplanning:add-device/<user id>/<seed>             a pending device of a user
```

Both hold the same data, a document id and a 32-byte seed from which the entry's
keypairs are derived, and differ only in the flow they start, so they are one type:

```cpp
struct AccessUrl
{
    enum class Kind
    {
        UserInvite,
        PendingDevice
    };

    Kind kind;
    QString documentId;
    QByteArray seed;
};
```

The prefix carries the kind, so a fresh device knows before any fetch whether it is
joining a ward document or being added to a user. The app shows an add-device URL as
text to copy and as a QR code, for a phone to read.

## Joining and claiming

Joining a ward document and claiming a pending device run the same steps on their own
document:

1. Fetch the current version, signed by the invite or the pending device, and validate
   it. A refusal here means it is spent, expired, withdrawn or cancelled, and the
   flow ends.
2. Apply `joining` or `claiming`. Joining puts a `User` with this user's id, name and
   key in the invite's place; a user already in the ward document is left as it is, and
   only the invite goes, with "you already had access". Claiming puts a `Device` with
   this device's keys and name in the pending device's place. The data key stays.
3. Upload, signed by the invite or the pending device. A version conflict starts again
   from step 1.
4. Download as this device. Allowed means it landed; refused means it did not, and the
   flow starts again from step 1, which settles whether the entry is spent.

A device claiming a pending device writes the user document file only once step 4
confirms, which is the moment it gets its user. If the answer is lost, the next
`check` from a device with no user lists the user document that names it, and the
device takes it.

## Staying in sync

Every device calls `check` at startup and every 5 minutes, every 30 seconds while any
ward document on it has an active emergency. It acts on its user document's answer
first:

- `Refused` there means this device was removed. It drops the user document and every
  ward document, keeps its identity file, and goes back to having no user; nothing
  else in the answer is acted on.
- A newer version of any document is fetched and taken in.
- A document listed but not held is downloaded and added to the library, so a ward
  document joined or made on one device reaches the user's others.
- A ward document answered `Refused` or `Deleted` is dropped with its copies, and the
  person is told it is no longer available to them.
- `Unknown`, or a fetched version that fails validation, is a fault: the device keeps
  its copies, takes nothing in, and reports it.

## Taking access away

Each is one access action, and the server refuses the one taken out from then on:

- **Removing a user from a ward document**: an admin publishes it without them.
- **Leaving a ward document**: this user publishes it without themselves. The local
  copy stays readable until the server holds that version.
- **Removing a device**: any other device of the user publishes the user document
  without it. The removed device is refused on the user document and on every ward
  document of its user.

## Merging

Every device edits the directory and the emergency response freely, since edits of any
kind are rare, and these merge by last writer wins: on a version conflict, what changed
on one side takes that side, and what changed on both takes this device's, the later
publisher's. A merge that comes out identical to the server's version publishes
nothing. Access lists are never merged: a local copy takes the server's, and access
actions run on the server's current version.

Collections merge record by record: three ways against the base copy, by id, through
one generic merge. Absence is a value, so a removal is a change like any other. That
covers:

- the directory: families, teams, tags, districts, groups, assets, wards and stakes;
- the emergency response, whose nested collections are keyed by id.

Single values, such as the two import dates, take the same rule.

The response follows the rule like everything else: a deletion or an ending that
publishes second wins over the other side's records. Its contact attempts recorded on
both sides are separate records, so both stay. Task categories become a keyed set, and
a nested list merged by id is re-sorted newest first.

Every device geocodes families missing coordinates. A geocode and a person's edit to
the same family at the same moment keep the later publisher's family; a person moved
out of a family while that family is edited elsewhere ends up in both. Both are
accepted: each needs two edits to one family at once.

## One sync engine

The ward document and the user document share one sync engine: the base copy,
checking, fetching, validating, publishing, and merging on a version conflict. It is
one `QObject` holding a small per-kind interface, with `WardKind` and `UserKind` as
its two implementations. Each kind does five things: read the payload, merge it, build
the header, parse the header, and check that header and access list agree.

## Settled questions

Decided in discussion, each with its reason. A review takes these as given.

- **The design is built fresh from `master`.** It describes what is built, not changes
  to earlier encryption work, so it lists nothing as replaced.
- **The server does access control; encryption keeps plaintext off the server.** A
  data key stays for the life of its document: a device or user taken out is refused
  by the server, so keys never rotate and versions carry no former-key hashes.
- **Every document lives on the server,** a new ward document included. There is no
  local-only ward document and no Share action.
- **Every device edits freely, with no edit lease.** Edits of any kind are rare, so
  conflicts are rarer still.
- **Last writer wins for the directory and the response,** with absence as a value.
  One rule keeps the code simple; the old edit-beats-delete protections for the
  response are given up knowingly. There is no conflict dialog.
- **Access lists are never merged.** Access actions run on the server's current
  version and need a connection; a local copy only takes the server's access list.
- **A geocode crossing an edit to the same family, and a person moved out of a family
  edited elsewhere, are accepted.** Each needs two edits to one family at once.
- **Removing a device, or being removed, drops the documents with no warning.** Every
  document lives on the server, so adding the device again downloads it all.
- **A device belongs to one user.** A fresh device asks whether the person already
  uses Ward Planning before making a user; two users for one person are undone by
  Remove This Device and a re-invite.
- **The last admin makes someone else an admin before leaving.** A ward document whose
  only admin loses every device is accepted: its users keep reading and editing, and
  managing access needs a new ward document.
- **A removed user's invites stay.**
- **Re-invite works per ward document,** and the old user document stays on the server.
- **Users and devices have their own words,** in the app and the code: invite, join,
  remove a user and leave; add a device, claim, remove a device and cancel.
- **Each document kind has its own header, and each open entry its own type,** named
  for what it is. The URL is one type with a kind, since both URLs hold the same data.
- **An add-device URL is a full URL whose pending device expires 30 minutes after it
  appears, timed by the server,** shown as text and as a QR code. A typed short code is
  not used. A user has one pending device at a time.
- **The server announces a minimum app version,** raised when a format changes; a
  device below it waits to sync. Updates are forced only by format changes.
- **A device that loses access is not told by whom,** and server answers carry no
  record of who published a version.
- **Admin is a flag on the user,** checked only by the app.
- **The server checks what it must and no more.** A rule against a mistake the app
  never makes, such as deleting a user document, is left out.
- **The sync engine is one `QObject` with a per-kind interface,** since moc does not
  take a class template and function bodies stay out of headers.
