# The .wardplanning Format

The specification of a Ward Planning ward file, update file, invitation file and device key
file, format version 1.
A reader built from this document alone reads every file the app writes. `tests/spec` is such
a reader, built without Qt or any of the app's code, and the test suite checks it against a
file holding a value in every field the app writes.

## Conventions

- **Binary values** are base64url without padding (RFC 4648 section 5, no `=`). Each has a fixed
  length: a document id or user id 16 bytes (22 characters), a nonce 24 (32), a fingerprint 16
  (22), a key, seed or former-key hash 32 (43), a sealed key 80 (107). A value of another length,
  with padding, with characters outside the base64url alphabet, or with non-zero unused trailing
  bits is malformed.
- **JSON** is UTF-8. An integer is a JSON number whose value is whole (`3`, `3.0` and `3e0` are
  all the integer 3) with a magnitude of at most 2^53; a number is any JSON number.
- **An id** in the ward part (of a person, family, team, emergency asset, ministering group or
  district, contact attempt or task) is a non-empty string. The app makes each one a UUID in its
  hyphenated form without braces; a reader takes any non-empty string.
- **A date** is a string `yyyy-MM-dd`. **A time** is a UTC string `yyyy-MM-ddTHH:mm:ss.zzzZ`,
  milliseconds included. A value that does not read back as the same string is malformed.
- **A word** is one of the strings its field lists, spelled exactly as listed.
- **Every field is required**, except one marked *omitted when ...* or *omitted until ...*: its
  writer leaves it out when it has no value (an empty string, array or object, `false`, no
  value, or, for a change, an unchanged field), and a reader reads its absence as no value. Such
  a field written out empty is malformed.
- **Null** is written only where a field says *or null*, for a value that has none. Such a field
  is required.
- **Nothing else is accepted.** A reader refuses a file with a field missing, malformed, of the
  wrong type, or not listed here, and reads nothing from it.
- **Collections** of things with an id are written as arrays in id order, ids compared as
  strings by UTF-16 code unit, and sets as arrays in the same sorted order. A list whose order is
  data keeps its order: an address's lines, a family's members, a family change's added and
  removed members, callings, task categories, an emergency's contact attempts and tasks (most
  recent first), the access list's users, each user's devices, the invites, the former keys
  (oldest first), description lines and the journal's entries. A reader depends on no sorted
  order.

## Format versions

The header's `format` integer covers the whole file: the header, the sealing and the payload.
It is read first, and it is the one thing no format version changes: a file opens with a
one-line JSON header whose `format` key is an integer. A reader treats a `format` above every
version it knows as a newer format, reads a version it knows with that version's reader, and
treats anything else as unreadable. A release that changes any field raises the version.

## Where the files live

The key file is `keys/device.key` in app data (`QStandardPaths::AppLocalDataLocation`). A ward
file is `wards/<document id>.wardplanning` in the same app data, named by its header's
`document`; a reader opening a ward by its id refuses a file whose header names a different id.

## The key file

The key file is one compact JSON object:

| Key | Type | Holds |
|---|---|---|
| `format` | integer | 1 |
| `sealingSecret` | 32 bytes | the device's X25519 secret key |
| `signingSeed` | 32 bytes | the device's Ed25519 seed |

The X25519 public key is `crypto_scalarmult_base(sealingSecret)`. The Ed25519 verify key is the
public key `crypto_sign_seed_keypair(signingSeed)` gives.

## Fingerprint

16-byte unkeyed BLAKE2b, with a zero salt and the personalization `wp-fingerprint` zero-padded
to 16 bytes, over the X25519 public key followed by the Ed25519 verify key (64 bytes in).

## Framing

A file is the header line's bytes, one `\n` (0x0A), then the payload's bytes. The header is
compact JSON, which escapes every newline inside it, so the first `\n` is the boundary.

## Header

| Key | Type | Holds |
|---|---|---|
| `format` | integer | 1 |
| `document` | 16 bytes | the document id |
| `number` | integer | the version's number, at least 1; a ward's first version is 1, written when the ward is created, and each later one is one above the version its writer held |
| `writer` | 16 bytes | the fingerprint of the device or open invite that wrote the version |
| `nonce` | 24 bytes | the payload's nonce, drawn from `randombytes_buf` on every write |
| `recipients` | array of objects | one per device and one per open invite |

In a local file, `number` and `writer` name the version this device holds: the one it last took
or wrote.

Each recipient:

| Key | Type | Holds |
|---|---|---|
| `fingerprint` | 16 bytes | the recipient's fingerprint |
| `sealedKey` | 80 bytes | the 32-byte data key sealed with `crypto_box_seal` to the recipient's X25519 public key |

## Payload encryption

The payload's bytes are XChaCha20-Poly1305 (`crypto_aead_xchacha20poly1305_ietf`) under the
data key and the header's nonce, with the header line's exact bytes, without the `\n`, as the
associated data; the 16-byte tag follows the ciphertext. Decrypted, they are the compact payload
JSON compressed with Qt's `qCompress`: a 4-byte big-endian length of the uncompressed bytes,
then a zlib stream.

## Payload

One object. Its parts:

| Key | Type | Holds |
|---|---|---|
| `ward` | object | the ward's content (see *ward*) |
| `access` | object | who may read the ward (see *access*) |
| `landed` | object | each device's landed mark (see *landed*) |
| `device` | object | this device's own state (see *device*): required in a local file, and refused in a version on the server, an update file or an invitation file |

### access

| Key | Type | Holds |
|---|---|---|
| `users` | array of user objects | every user |
| `invites` | array of invite objects | the open invites; omitted when empty |
| `formerKeys` | array of 32-byte values | BLAKE2b-256, personalized `wp-former-key`, of each data key the ward used before its current one, oldest first; omitted when empty |

The users, each user's devices and the invites keep their order.

A user:

| Key | Type | Holds |
|---|---|---|
| `id` | 16 bytes | the user's id within this ward |
| `name` | string | the name they gave |
| `role` | word: `admin` or `member` | their role |
| `devices` | array of device objects | their devices |

A device:

| Key | Type | Holds |
|---|---|---|
| `name` | string | the device's name |
| `publicKey` | 32 bytes | its X25519 public key |
| `verifyKey` | 32 bytes | its Ed25519 verify key |

An invite:

| Key | Type | Holds |
|---|---|---|
| `for` | word: `device` or `user` | what it joins as |
| `maker` | 16 bytes | the id of the user who made it |
| `role` | word: `admin` or `member` | the role its joiner takes; required on a user invite, and refused on a device invite |
| `label` | string | who it is for; required on a user invite, and refused on a device invite |
| `publicKey` | 32 bytes | the invite's X25519 public key |
| `verifyKey` | 32 bytes | the invite's Ed25519 verify key |

### landed

An object whose keys are device fingerprints (16 bytes, base64url) and whose values are
integers: the highest journal sequence number of that device the version includes.

### device

| Key | Type | Holds |
|---|---|---|
| `journal` | journal object | this device's commands not yet seen to land |
| `dismissedAccess` | access object | the access list as of the last dismissed access notice; omitted until one is dismissed |
| `singleHolderNoticeDismissed` | `true` | omitted when false |
| `singleAdminNoticeDismissed` | `true` | omitted when false |
| `noLongerShared` | `true` | the ward is no longer this device's; omitted when false |
| `inviteSeeds` | array of 32-byte values | the seed of each open invite this device made; omitted when empty |

A journal:

| Key | Type | Holds |
|---|---|---|
| `lastSequence` | integer | the last sequence number given to an entry |
| `entries` | array of entry objects | in the order they were made; omitted when empty |

An entry:

| Key | Type | Holds |
|---|---|---|
| `sequence` | integer | its sequence number |
| `command` | command object | the command, as it was written when it entered the journal |

A command object:

| Key | Type | Holds |
|---|---|---|
| `type` | string | the command type, one of those under *Commands*; any other refuses the file |
| `description` | description object | what the user did |
| `fields` | object | the type's own fields (see *Commands*) |

A description:

| Key | Type | Holds |
|---|---|---|
| `heading` | string | what was done to what |
| `lines` | array of strings | a line per value set; omitted when empty |

### ward

| Key | Type | Holds |
|---|---|---|
| `stakes` | array of stake objects | the stake records, in unit number order; omitted when empty |
| `wards` | array of ward record objects | the ward records, in unit number order; omitted when empty |
| `families` | array of family objects | in id order; omitted when empty |
| `teams` | array of team objects | in id order; omitted when empty |
| `addressCoordinates` | array of address coordinates objects | the table of address coordinates, in address order; omitted when empty |
| `emergencyResources` | array of emergency asset objects | in id order; omitted when empty |
| `emergencyResponse` | emergency object | the emergency in progress; omitted when there is none |
| `eqDistricts` | array of district objects | the elders quorum's districts, in id order; omitted when empty |
| `eqGroups` | array of group objects | the elders quorum's groups, in id order; omitted when empty |
| `rsDistricts` | array of district objects | the Relief Society's districts, in id order; omitted when empty |
| `rsGroups` | array of group objects | the Relief Society's groups, in id order; omitted when empty |
| `wardDirectoryPdfDate` | date | the date of the last ward directory imported; omitted when none |
| `ministeringPdfDate` | date | the date of the last ministering PDF imported; omitted when none |

A stake:

| Key | Type | Holds |
|---|---|---|
| `name` | string | the stake's name, without " Stake"; omitted when empty |
| `unitNumber` | string | its unit number, which keys it among the stakes; omitted when empty |
| `stakeCenterAddress` | string | the stake center's address; omitted when empty |
| `stakeCenterLat` | number | the stake center's latitude; omitted when none |
| `stakeCenterLng` | number | the stake center's longitude; omitted when none |
| `wardUnitNumbers` | array of strings | the unit numbers of its wards, sorted; omitted when empty |

A ward record:

| Key | Type | Holds |
|---|---|---|
| `name` | string | the ward's name, without " Ward"; omitted when empty |
| `unitNumber` | string | its unit number, which keys it among the ward records; omitted when empty |
| `stakeUnitNumber` | string | its stake's unit number; omitted when empty |
| `chapelAddress` | string | the chapel's address; omitted when empty |
| `chapelLat` | number | the chapel's latitude; omitted when none |
| `chapelLng` | number | the chapel's longitude; omitted when none |
| `chapelPhone` | string | the chapel's phone; omitted when empty |
| `meetingTime` | string | when the ward meets; omitted when empty |

A family:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the family's id |
| `address` | array of strings | the address's lines; omitted when empty |
| `members` | array of person objects | in the family's member order; omitted when empty |
| `note` | string | omitted when empty |

A person:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the person's id |
| `name` | string | `Surname, Given names` |
| `phone` | string | omitted when empty |
| `altPhone` | string | the other phone; omitted when empty |
| `email` | string | omitted when empty |
| `gender` | word: `male` or `female` | omitted when none |
| `birthday` | birthday object | omitted when no part of it is known |
| `callings` | array of strings | omitted when empty |
| `stakeUnitNumber` | string | the stake the person belongs to; omitted when empty |
| `wardUnitNumber` | string | the ward the person belongs to; omitted when empty |
| `isParent` | `true` | omitted when false |
| `specialNeedNote` | string | omitted when empty |
| `note` | string | omitted when empty |

A birthday:

| Key | Type | Holds |
|---|---|---|
| `year` | integer | the year; omitted when unknown |
| `month` | integer | 1 for January; omitted when unknown |
| `day` | integer | the day of the month; omitted when unknown |

A team:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the team's id |
| `name` | string | its name |
| `leaderId` | id | its leader, one of its members; omitted when none |
| `memberIds` | array of ids | its members, sorted; omitted when empty |

An address coordinates entry:

| Key | Type | Holds |
|---|---|---|
| `address` | string | an address's lines joined by `, `, which keys the entry |
| `latitude` | number | |
| `longitude` | number | |

An emergency asset:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the asset's id |
| `name` | string | its name |
| `responseArea` | word: `med`, `rec` or `com` | medical, recovery or communications |
| `personIds` | array of ids | the people who have it, sorted; written when empty too |

A ministering group:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the group's id |
| `isRSFormat` | boolean | true for a Relief Society group, which ministers to people; false for an elders quorum group, which ministers to families |
| `ministerIds` | array of ids | its ministers, sorted; omitted when empty |
| `familyIds` | array of ids | the families it ministers to, sorted; omitted when empty |
| `ministeredPersonIds` | array of ids | the people it ministers to, sorted; omitted when empty |
| `presidencyMemberId` | id | the presidency member over it; omitted when none |

A ministering district:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the district's id |
| `name` | string | its name |
| `presidencyMemberId` | id | the presidency member over it; omitted when none |
| `groupIds` | array of ids | its groups, sorted; omitted when empty |

An emergency:

| Key | Type | Holds |
|---|---|---|
| `name` | string | its name |
| `startedAt` | time | when it was started |
| `taskCategories` | array of strings | the categories a task can take, in the order they were added; written when empty too |
| `contactAttempts` | array of contact attempt objects | most recent first: the greatest `timestamp` first, ties by the greater id; omitted when empty |
| `tasks` | array of task objects | most recent first by `createdAt`, ties by the greater id; omitted when empty |

A contact attempt:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the attempt's id |
| `familyId` | id | the family it tried to reach |
| `familyName` | string | the family's name when the attempt was made |
| `method` | word: `phone`, `text`, `email`, `visit` or `other` | how; omitted when none |
| `who` | id | the person reached; omitted when none |
| `whoName` | string | that person's name when the attempt was made; omitted when empty |
| `outcome` | word: `ok` or `unableToReach` | what it found; omitted when none |
| `timestamp` | time | when it was made |
| `notes` | string | omitted when empty |

A task:

| Key | Type | Holds |
|---|---|---|
| `id` | id | the task's id |
| `familyId` | id | the family it is for |
| `familyName` | string | the family's name when the task was created |
| `category` | string | its category |
| `description` | string | what needs doing |
| `createdAt` | time | when it was created |
| `assignment` | assignment object | who it is given to; omitted when unassigned |
| `resolution` | resolution object | how it was done; omitted when unresolved |

An assignment, given to a team or to a person:

| Key | Type | Holds |
|---|---|---|
| `teamId` | id | the team; omitted when given to a person |
| `personId` | id | the person; omitted when given to a team |
| `assigneeName` | string | the team's or person's name when the task was given |
| `notification` | notification object | how the assignee was told; omitted until they are |

A notification:

| Key | Type | Holds |
|---|---|---|
| `method` | word: `phone`, `text`, `email`, `visit` or `other` | how |
| `timestamp` | time | when |
| `notes` | string | what was said; omitted when empty |

A resolution:

| Key | Type | Holds |
|---|---|---|
| `notes` | string | what was done; omitted when empty |
| `resolvedAt` | time | when it was marked done |

### Commands

Each command type's `fields`, by its `type`. They use these shapes:

- **A change** is an object `{before, after}`: the value as the user saw it and the value they
  set, both required and of the field's value type. A value type marked *or null* writes null
  for no value. A change is omitted when its field is unchanged, unless the type says otherwise.
- **A people change** is an object `{added, removed}`: arrays of person ids, each sorted and
  written when empty too.
- **A cleanup** is what a removal takes out of the places that refer to what it removes, with
  those places as they stood:

  | Key | Type | Holds |
  |---|---|---|
  | `people` | array of ids | the people removed, sorted; omitted when empty |
  | `families` | array of ids | the families removed, sorted; omitted when empty |
  | `teams` | array of ids | the teams removed, sorted; omitted when empty |
  | `heldTeams` | array of team objects | in id order; omitted when empty |
  | `heldAssets` | array of emergency asset objects | in id order; omitted when empty |
  | `heldGroups` | array of group objects | in id order; omitted when empty |
  | `heldDistricts` | array of district objects | in id order; omitted when empty |
  | `heldTasks` | array of task objects | in id order; omitted when empty |

  A field that holds a cleanup is omitted when the cleanup holds none of these, unless the
  type says it is always written.

`updateFamily`:

| Key | Type | Holds |
|---|---|---|
| `familyId` | id | the family edited |
| `change` | family change object | what the edit changes |
| `addedCleanup` | cleanup | the places that refer to the members it adds, which it restores; omitted when empty |
| `removedCleanup` | cleanup | the places that refer to the members it removes, which it takes them out of; omitted when empty |

A family change:

| Key | Type | Holds |
|---|---|---|
| `address` | change of an address (array of strings) | omitted when unchanged |
| `note` | change of a string | omitted when unchanged |
| `members` | array of member change objects | in id order; omitted when empty |
| `added` | array of placed member objects | the members it adds; omitted when empty |
| `removed` | array of placed member objects | the members it removes, as last seen; omitted when empty |

A member change holds `id`, the member's person id, and a change for each of the member's
fields the edit changes, each omitted when unchanged:

| Key | Type of the change's values |
|---|---|
| `name` | string |
| `gender` | word `male` or `female`, or null |
| `phone` | string |
| `altPhone` | string |
| `email` | string |
| `birthday` | birthday object, `{}` when no part is known |
| `callings` | array of strings |
| `isParent` | boolean |
| `wardUnitNumber` | string |
| `stakeUnitNumber` | string |
| `specialNeedNote` | string |
| `note` | string |

A placed member:

| Key | Type | Holds |
|---|---|---|
| `person` | person object | the member |
| `index` | integer | for an addition, where it is inserted; for a removal, where it was seen |

`deleteFamily` and `restoreFamily`:

| Key | Type | Holds |
|---|---|---|
| `family` | family object | the family as seen |
| `cleanup` | cleanup | the family under `families`, its members under `people`, and the places that refer to them; always written, since it holds the family |

`setAddressCoordinates` and `revertAddressCoordinates`:

| Key | Type | Holds |
|---|---|---|
| `address` | string | the address entry's key |
| `before` | coordinates object | the entry before the position was typed; omitted when there was none |
| `typed` | coordinates object | the position typed |

A coordinates object is `{latitude, longitude}`, two numbers.

`addTeam` and `deleteTeam`:

| Key | Type | Holds |
|---|---|---|
| `team` | team object | the team |
| `cleanup` | cleanup | the team under `teams`, and the open tasks assigned to it under `heldTasks`; always written, since it holds the team |

`updateTeam`:

| Key | Type | Holds |
|---|---|---|
| `teamId` | id | the team edited |
| `name` | change of a string | omitted when unchanged |
| `leader` | change of an id or null | omitted when unchanged |
| `members` | people change | written when empty too |

`addEmergencyAsset` and `deleteEmergencyAsset`:

| Key | Type | Holds |
|---|---|---|
| `asset` | emergency asset object | the asset |

`updateEmergencyAsset`:

| Key | Type | Holds |
|---|---|---|
| `assetId` | id | the asset edited |
| `name` | change of a string | omitted when unchanged |
| `people` | people change | written when empty too |

`startEmergency`, `endEmergency` and `restoreEmergency`:

| Key | Type | Holds |
|---|---|---|
| `response` | emergency object | the emergency started, ended as reported, or put back |

`addContactAttempt` and `deleteContactAttempt`:

| Key | Type | Holds |
|---|---|---|
| `attempt` | contact attempt object | the attempt |

`addTask` and `deleteTask`:

| Key | Type | Holds |
|---|---|---|
| `task` | task object | the task |

`updateTask`:

| Key | Type | Holds |
|---|---|---|
| `familyId` | id | the task's family |
| `taskId` | id | the task edited |
| `category` | change of a string | omitted when unchanged |
| `taskDescription` | change of a string | the task's `description`; omitted when unchanged |
| `assignment` | change of an assignment object or null | omitted when unchanged |
| `resolution` | change of a resolution object or null | omitted when unchanged |

`addTaskCategory`:

| Key | Type | Holds |
|---|---|---|
| `category` | string | the category added |

`importWardDirectory`:

| Key | Type | Holds |
|---|---|---|
| `families` | family collection change object | what the import changes in the families; written when empty too |
| `pdfDate` | change of a date or null | the ward directory PDF date; omitted when unchanged |
| `coordinates` | array of address coordinates entries | the positions the PDF prints for addresses the table lacked, in address order; omitted when empty |
| `ward` | ward record object | the ward record the PDF names |
| `ministeringPdfDate` | date or null | the ministering PDF date the import read |

A family collection change:

| Key | Type | Holds |
|---|---|---|
| `added` | array of family objects | the families it adds, in id order; omitted when empty |
| `changed` | array of family change objects | a change per family that differs, each with `id`, the family's id, beside its own fields, in id order; omitted when empty |
| `removed` | array of family objects | the families it removes, as last seen, in id order; omitted when empty |
| `addedCleanup` | cleanup | the places that refer to what it adds, which it restores; omitted when empty |
| `removedCleanup` | cleanup | the places that refer to what it removes, which it takes them out of; omitted when empty |

`importMinistering`:

| Key | Type | Holds |
|---|---|---|
| `org` | word: `eldersQuorum` or `reliefSociety` | the organization imported |
| `collection` | change of a ministering collection object | the organization's districts and groups; written always |
| `families` | family collection change object | what the import changes in the families; written when empty too |
| `pdfDate` | change of a date or null | the ministering PDF date; omitted when unchanged |
| `wardDirectoryPdfDate` | date or null | the ward directory PDF date the import read |

A ministering collection:

| Key | Type | Holds |
|---|---|---|
| `districts` | array of district objects | in id order; written when empty too |
| `groups` | array of group objects | in id order; written when empty too |

## Header and access list agree

Every recipient's fingerprint is computed from exactly one device's or invite's pair of public
keys in `access`, and every device and open invite there has exactly one recipient. A file that
fails this is refused.

## A deleted ward's final version

A header with an empty `recipients` array and a fresh nonce, followed by its `\n` and nothing
after it. A reader recognizes it by the empty `recipients`, before any payload rule applies. No
local file takes this form.

## Update files

A `.wardupdate` file is one line holding a 64-byte Ed25519 signature, one `\n`, then a version
in the framing above: its header, `\n`, and a payload holding `ward`, `access` and `landed`,
and no `device`. The signature is over the UTF-8 line

```
wardplanning-v1 update <document> <header hash> <payload hash>
```

with single spaces, where `<document>` is the header's `document`, and the hashes are 32-byte
unkeyed BLAKE2b of the header line's bytes, without its `\n`, and of the payload's bytes; all
three are base64url. It verifies with the verify key of the device or open invite whose
fingerprint is the header's `writer`, as the access list of the version the reader holds gives
it. A reader refuses a file whose signature does not verify, or whose `writer` that access list
does not name.

## Invitation files

A `.wardinvite` file is one line of compact JSON, one `\n`, then a version in the framing
above, whose payload holds `ward`, `access` and `landed`, and no `device`, and whose header
names the invite among its recipients.

| Key | Type | Holds |
|---|---|---|
| `format` | integer | 1 |
| `document` | 16 bytes | the document id |
| `seed` | 32 bytes | the invite's seed |

The invite's keys come from the seed through `crypto_kdf_derive_from_key` with the context
`wpinvite` and 32-byte subkeys: subkey 1 is the seed for `crypto_box_seed_keypair`, and subkey 2
the seed for `crypto_sign_seed_keypair`. The invite's fingerprint is computed from that pair of
public keys as for a device.
