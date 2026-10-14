# The Shared Ward

A ward is an encrypted document that carries its own access list. The users holding one
open see each other, and see each other's work as it happens.

This is the whole design: what the file is, who can read it, how it is shared, and what
the server does.

## Purpose

Ward leaders need custom views of member data that the official church tools do not
provide, and bishoprics commonly build those views in shared Google Sheets. Church
policy asks that member data live in official systems. Ward Planning resolves that
tension by keeping the data in a document only the bishopric can read.

**Sharing has to be something a busy, non-technical counselor can set up in a couple of
minutes.** That is the primary goal, and it decides the shape of everything below.

The obvious approach, an encrypted file in a folder that Google Drive or OneDrive already
synchronises, fails that test on one specific point: it creates **two access lists that
have to be kept in agreement.** Adding a counselor means sharing the folder in the
provider's own interface *and* granting access in the app; removing means both, and
forgetting the folder step leaves that person with write access and version history. Each
is a step a casual user performs wrong. Before any of it, every member needs an account
with the same provider, the sync client installed, and the ability to find the
synchronised folder on disk.

**So the app talks to a Ward Planning server directly, and the ward itself is the access
list.** Sharing is: send someone a link, they install and open it. No provider account,
no sync client, no folder path, and no second list anywhere.

**Every member still keeps a complete local file.** It opens, edits and saves with no
network at all, which is what makes the application dependable during an emergency, the
moment it exists for. The server holds the authoritative copy, in the same encrypted
bytes the local file holds, and can read none of them.

Every document is a ward on the server from its first save that reaches the network,
starting as a ward of one. Sharing it is inviting someone, with no step before it.

## Scope

This covers the encrypted document, who has access to it, concurrent use, and the
contract between the app and the server. The server's own implementation, hosting and
operation are a companion piece of work; what follows fixes the interface that work has
to present.

Two related ideas surfaced during design and are deferred to their own work:

- **Spreadsheet sync**: column-mapped add and remove against an external `.csv` or `.xlsx`
- **Native talks tracker**: speaking history held in the document itself

## Decisions

| Question | Decision |
|---|---|
| Primary goal | Sharing that a non-technical user completes in a couple of minutes |
| Where the authoritative copy lives | A Ward Planning server, reached by the app directly |
| Server address | Built into the app, with an advanced override; documents carry only their id |
| Local copy | Complete, always present, fully editable offline |
| Running instances | One per user; files and links go to the running app (see *Links and launches*) |
| Where the local file lives | Application data, keyed by document id; the user never chooses a path |
| Third-party sync folders | Not built. Continuity rests on the project being open source. |
| The split of responsibility | **The ward says who may read it; the server stores versions and relays changes.** Besides the version condition, the server checks only that a write is signed by a device the current header names. |
| Who has access | The access list inside the encrypted payload: users, their devices, and open invites |
| Key model | Envelope encryption, one data key per document, sealed to each device |
| Key generation | Silent, on first run. No user-visible key material. |
| Server authentication | None. Each write and each channel carries an Ed25519 signature the current header admits. |
| Joining | A one-time invite link |
| Roles | Admin or member, held in the access list, a convention among honest clients |
| Who may add | An admin adds a user; anyone adds their own devices |
| Who may remove | An admin removes a user; anyone unlinks their own devices, or leaves |
| Removal | One write without their devices, which rotates the data key |
| Concurrent use | Presence while connected, every edit a version, and offline commands replayed onto the current version. Content and access changes take the same path. |
| A replay that changes nothing | Runs to completion and pushes, though the pushed version matches the server's. It is rare, so it follows the ordinary path. |
| Private key storage | A file in local app data, readable only by its user, holding the secret halves, as SSH keys are kept. Local app data does not roam. |
| Crypto library | libsodium 1.0.22 or later from vcpkg, pinned in the manifest |
| Document storage | One encrypted payload. Replay happens on the model, so storage stays simple. |
| Encryption coverage | Every document; emergency archives are removed |
| File extension | `.wardplanning`, for exported copies and file association |

## Threat model

Naming this explicitly, because several design choices only make sense against it.

**What is being protected, and how much.** The document holds a ward's member directory:
names, addresses, phone numbers, birth dates, family relationships, and the ward's own
planning notes. That deserves real protection, and should not sit readable on a server or
in a stray file, but it is not money, credentials, or secrets anyone would mount an
elaborate attack for. Easier routes to the same data already exist: a focused phishing
attack on a ward leader's church website login yields the whole directory with far less
effort than anything this design could be attacked with.

So the defences are proportionate. The design protects against the realistic failures (a
server operator who can read what it stores, a lost or stolen device, a mistaken share, a
file that leaks) and keeps each defence cheap in code and invisible in use. **Attacks
that need collusion, or a chain of unlikely steps, are accepted rather than defended**,
and are listed below in a line each. Where a defence would cost usability or much code,
the simpler design wins.

**The server is trusted to store and to serve, and with nothing else.** It is not trusted
with the content, and it is not trusted with who has access either: both live in the
encrypted payload, and a client seals the data key only to a device it approved itself or
that an earlier version already held. A client takes a new version only when it descends
from one it already holds, under the same key or naming that key among its former keys,
so the server cannot hand it a ward of the server's own making.

**Defended against.** The server operator reading member data; the operator is the
application's author, and the design keeps that from being a matter of trust. Anyone who
obtains a file without ever having held a key. A removed user writing anything, which
the server refuses because the current header no longer names them. A user removed by
an admin reading a later version obtained some other way, which is under a key they never
held.
Silent corruption or truncation.

**No server answer destroys anything.** A head that leaves this device out stops this
copy pushing until a head names this device again, and tells the user they no longer have
access; the device keeps reading the head while the ward is open, and resumes syncing if a
later one includes it. The copy itself stays until they remove it. Deleting a local copy is tidiness, not enforcement, so it is the user's
action rather than the server's. A hostile operator, or a bug, can therefore cut a member
off from sharing, which it could do anyway by refusing to serve, and can destroy nothing.

**Detected, not prevented.** Tampering with a local file's header, including its document
id: the header is bound as associated data, so a modified local file fails to decrypt
rather than quietly attaching itself to a different document. **A rolled-back head** from a
server fault: a head whose version is below the base this device holds is refused. A
hostile server can relabel an old version, which is the accepted stale-version case below.

**Inside the trust boundary.** Every key holder can read everything and write whatever
they like into the document. Admins also decide who has access. A ward's admins are its
bishopric and clerks, the people who would hold that authority anyway, and the design
trusts them with it.

**Accepted: roles are a convention.** The server cannot read the access list, so it cannot
tell an admin from a member. A member with a modified client could add a user, but that
member already holds the key and could hand the data over directly. The role exists to
keep well-meaning users from over-sharing, and the design and the user guide never call
it a security measure.

**Accepted: one compromised admin device can remove everyone.** An admin device in
hostile hands could remove every other user from the ward's future. This was weighed
against requiring a second admin's approval and judged the better trade: the realistic
threat is a lost device, not a hostile one, and what a lost device needs is someone able
to remove it quickly and alone. A removal made offline replays with the rest of its
journal, so an unrelated conflict hands it back as well, and the message says that user
still has access until it is redone. A hostile use is visible at once, because every access
change is announced, and everyone keeps the copy they hold. **Recovery is a new ward:**
someone creates a fresh document, imports the ward directory and ministering PDFs again,
and invites users again; the old ward is abandoned to whoever took it. Content vandalism,
already possible for any key holder, is recovered the same way.

**Accepted: a stolen, unlocked device acts as its owner.** It can read everything, write
content, unlink its owner's other devices, add devices of the thief's own, and on an
admin's device anything an admin can do. Its owner unlinks it from another device, or any
admin removes the owner, who is then invited again. Planted devices are shown for what
they are: each appears under its owner's name, and the owner unlinks any they do not
recognise.

**Accepted: an intercepted invite link gets in.** Whoever opens the link first gets in,
under the name they give. Interception of a device invite puts the interceptor in as one of
the inviter's devices, so the inviter sees a device they did not add and removes it, which
rotates the key. Interception of a user invite shows up in the access list as a user the
inviter did not expect, or under the expected name if the interceptor guessed it; either
way the real recipient finds their link already used and says so. In both cases the entry
is visible and removable, which is the accepted answer at this threat level.

**Accepted: a device's first download is taken as it stands.** A device holding no
version of a ward, newly joined or after its app data was cleared, has nothing for the
first version to descend from, so a hostile operator could hand it a ward of its own
making at that moment. What the device then writes into it would be readable by the
operator. Every later version must descend from the one it took.

**Accepted: a leaver knows the next key.** Leaving is the leaver's own push, and that push
rotates, so the leaver's device generates the new key; a modified client could keep it and
read later versions. Leaving is voluntary and the leaver already held everything, and the
next removal rotates again.

**Accepted: a channel-open signature replays.** It covers the call and the document id,
so a captured one could make a third party appear present. The gain to an attacker is a
lie about who is here, and nothing is exposed, so a challenge is not worth its machinery.

**Accepted: a hostile operator can split members onto separate copies, or serve a stale
version.** Members would see different or stale work while believing they were current.
Nothing is exposed and every member keeps their own edits; the gain to an attacker is
disruption only.

**Cleaned up when the user asks.** A removed member's local copy: their app offers to
delete it once it learns the ward is no longer theirs. Copies they saved are their own
files and keep what they had.

**Outside the app's reach.** Reports exported by `ReportGenerator`, which remain
plaintext PDFs.

**Known to the server, and accepted.** Per document: the ciphertext and its size, which
leaks approximate ward size; the fingerprints in each version's header, which count the
devices with access without naming anyone; which fingerprint published when, and from
what address; and which fingerprints hold a ward open at the same time. The server learns
no names, no roles, and no history of who was added or removed. The user guide says so
plainly.

**Availability.** The server going down denies *sharing and live editing*, not *use*.
Every member continues to open, edit and save their local file, and their commands replay
when the server returns. This is the property that decides the whole shape of the design.

## File format

A `.wardplanning` file is a plaintext JSON header followed by an encrypted payload. The
header is readable without a key so the app can identify the format, locate the reader's
entry, and check versions without decrypting anything.

**Framing.** The header is compact JSON on one line. A file is the header's bytes, one
`\n`, then the payload's bytes. Compact JSON escapes every newline inside it, so the first
`\n` is always the boundary, and the header stays readable in a text viewer. On the wire,
header and payload travel as separate fields.

**Header**

- format version, which starts at 1
- document id: 128 random bits generated by the app when the document is created, and
  never changed
- payload nonce
- recipient entries, one per device and one per open invite: `{fingerprint, data key
  sealed to that X25519 public key}`

The whole header is the associated data of the payload's AEAD, so anyone holding the data
key detects any change to it on decrypt. **The associated data is the header's exact
stored bytes, never a re-encoding.** A writer builds the header bytes once, nonce and
recipients included, and encrypts against them; a reader decrypts against the bytes it
read. No canonical JSON form is needed, because nothing is ever rebuilt from a parsed
header.

**Parsing is strict; a newer version is refused, never guessed at.** One format version,
in the header, covers the whole file: the header's fields, how the data key is sealed to
each device, and the payload inside. A reader accepts a file only when it knows that
version and finds every required field present and well formed. Tolerant parsing, which
skips what it does not recognise, mostly produces data read wrongly, so anything else is
refused whole. Any change to the header, the sealing, or the payload, such as a hybrid
post-quantum entry, is a new format version, and a reader refuses it before decrypting
anything.

**When a newer app wrote the version on the server**, this app keeps the local copy it
has, with the **directory read-only** and **nothing pushed**, and a persistent notice:
*This ward was saved by a newer version of Ward Planning. Update to see the latest changes
and share your work.* **Emergency response edits stay available** and are saved locally,
so responders keep working through an update. The update notice offers the download. The
state is checked again on every head, so it lasts only while the server keeps presenting a
version this app cannot read. The server supports a format before any app writes it.

**The server address lives in the app; the document carries only its id.** A file handed
over on a USB stick still knows which document it is. The app supplies the server, from a
built-in address with an advanced override for testing and self-hosting. Moving the
service to a new domain is therefore an app release, which is the natural shape of an
open-source maintainer adapting the project, and every existing document follows without
being rewritten.

**One id for the document's whole life.** The id names the local file, keys the local
record, and is bound into the header from the first save. The first push registers that
same id with the server, so nothing is renamed, re-keyed, or re-encrypted when the document
reaches it.

**Copies get a fresh id and a fresh key.** An exported copy is encrypted under a new data
key, so nobody who once held the ward's key can read it, and carries a new random id the
server has never seen, so a copy can never push itself into the ward's document and its
local records never collide with the original's. **A copy is sealed to the whole access
list as it stands**, so another member can open it. Once saved, a copy is the user's own
file, like any document they keep.

**Payload**

The document's JSON, compressed, encrypted with XChaCha20-Poly1305 under the data key, as
**one payload**: replay happens on the model, in a client that holds the data key, so one
payload serves it fully. One payload means one nonce, one version, and
one write.

**Nonce.** 192 bits from `randombytes_buf`, drawn fresh on **every write** and derived
from nothing in the file. XChaCha20-Poly1305's extended nonce is wide enough that random
nonces are safe without a counter or any stored state, which is why the header carries no
counter for one to be built from. This matters more than it looks: the data key is
long-lived, so a repeated nonce would XOR two versions of the ward directory against each
other and expose the Poly1305 one-time key.

**The header and the access list are two views of the same set, and must match exactly**:
every recipient entry's fingerprint is computed from exactly one key in the access list, and
every device and open invite in the access list has exactly one entry. A version that fails
this is refused like any other invalid version. Only a bug or a tampered app could write
one, and an unattributable key must never sit in the header.

This is also why the access list carries public keys at all. The header holds a fingerprint
per recipient, and a fingerprint is a one-way hash, so nothing can be sealed to someone from
it. Writing any version after the first needs the devices' actual X25519 keys, and the
access list inside the ciphertext is the only place they are - which is exactly the place
the server cannot read or change.

**Former keys.** The access list also carries a hash of every data key the ward used before
its current one (BLAKE2b, 32 bytes, personalised `wp-former-key`), appended at each rotation.
A client takes a version only when its key is one the client already holds for that ward,
or when the version names one of those among its former keys. Only someone who held an
earlier key can name it, and the hashes travel only inside the encrypted payload, so the
server can neither forge a descendant nor learn them. The list grows by one entry per
rotation, a handful a year.

**Former keys are not content.** A write takes them from the version it builds on and
appends the old key's hash when it rotates, so they are never something a command sets and
never something a user is asked about.

**How the access list changes.** Every write - a local save or a published version - seals
the data key to exactly the devices and open invites the access list holds, so a user an
admin removed loses their recipient entry at once. Nothing else adds a key. In particular a
client never seals to a key because the server mentioned it, which is what keeps a hostile
server from planting a device.

**Rotation belongs to publishing.** The version that reaches the server rotates the data key
when its access list has lost anyone the version it is built on sealed to: the new key goes
only to those remaining, and the old key's hash joins the former keys. A local save does
not rotate, because rotation exists so that a removed user cannot read versions written
after their removal, and a version is what reaches the server. Nobody else holds this
device's unpublished file.

That gives one invariant, and the descent check above rests on it: **the local file is under
the key of the last version this device took from, or gave to, the server.** So the versions
this device trusts are that one key, an incoming version under it is accepted directly, and
a later one is accepted because its former keys name it. Without this, an admin who removed
someone would hold a key of their own invention and could never again accept a version from
anyone else.

Several access changes made while disconnected therefore rotate once, at the push that
carries them, rather than once each.

**Every write re-seals.** A save rebuilds every recipient entry from the access list rather
than carrying the old ones forward, so a sealed key that was written wrong, by a bug or a
tampered app, stops locking its owner out of every later version. Sealing a 32-byte key to
a handful of devices costs nothing worth saving.

**Fingerprints are at least 128 bits:** 16-byte BLAKE2b over the device's X25519 public
key followed by its Ed25519 verify key, personalised `wp-fingerprint` (the string,
zero-padded to 16 bytes, in BLAKE2b's parameter block, with a zero salt). The server
reproduces it, and the known-answer vectors pin it. A fingerprint is an identity handle,
not a shortened secret. Because it covers both keys, a device cannot keep its identity
while either key is swapped: a changed key is a different device.

**Post-quantum.** X25519 is exposed to harvest-now-decrypt-later, and hybrid
X25519 + ML-KEM-768 is now in production elsewhere. The exposure needs an attacker holding
the file today and a quantum computer later, against data with a few-year sensitivity
horizon, so this design stays classical. A hybrid entry can be added later as a new format
version, which every member's app picks up by updating.

**Compression before encryption** is safe here: there is no adaptive chosen-plaintext
channel, so CRIME-style attacks do not apply. It does leak approximate ward size to the
server, which is accepted.

**Primitives.** `crypto_box_seal` wraps the data key to an X25519 public key. Anonymous
sealed boxes mean adding someone needs nothing from the inviter's own key. Sealed boxes
use XSalsa20-Poly1305 internally, not the XChaCha20-Poly1305 of the payload; for a 32-byte
data key this is immaterial, but it is worth not mis-stating. `crypto_sign` (Ed25519)
signs each write and each channel open.

**Library pin.** CVE-2025-69277 (`crypto_core_ed25519_is_valid_point` accepting points
outside the main subgroup; fixed by `ad3004e`, released in 1.0.20-stable and 1.0.21) does
not affect any call this design makes. The build requires 1.0.22, the current release,
regardless: `vcpkg.json` lists `libsodium` with `"version>=": "1.0.22"` and a
`builtin-baseline`.

## Identity

Each device holds an X25519 sealing keypair and an Ed25519 signing keypair, made at first
run and kept like SSH keys in local app data. A fingerprint is BLAKE2b over both public
keys. Within a ward, a user is named by a 128-bit user id their devices share.

## What the server is

A store of encrypted documents that tells the users holding one open when it changes, and
who else holds it open. Nothing about users.

It holds the current version of each ward and a version number that rises with it. The
number is there for the write condition, so two users cannot both write from the same
starting point; no call asks for an earlier version, and none is kept.

| Call | Who may | What |
|---|---|---|
| `POST /documents` | anyone | Stores version 1 of a new document under its client-made id. Rate-limited by address. |
| `GET /documents/{id}/head` | anyone with the id | Version, publisher fingerprint and time, and the header. |
| `GET /documents/{id}` | anyone with the id | The current version: header and blob. Ciphertext; unreadable without a key. |
| `PUT /documents/{id}` | a key holder | Conditional on `baseVersion`. Signed. |
| the live channel | a key holder | Open while the ward is open. Carries presence and version notices. |

**Signed requests.** A write carries the signer's two public keys and an Ed25519 signature
over one UTF-8 line:

```
wardplanning-v1 <call> <document id, hex> <base version> <header hash, hex> <payload hash, hex>
```

The hashes are BLAKE2b-256 of the exact bytes sent. Opening the live channel signs the same
line with `-` for the fields it lacks. No field contains a space, so the line is
unambiguous, simple to build on any server, and readable in a log. The server computes the signer's fingerprint from the keys exactly as clients
do, and accepts the write only if the *current* header names that fingerprint. There are
no sessions, no challenge, no registration, no token. The first version of a document is
the exception: anyone may create one, and its header names its creator. Opening the live
channel carries the same signature, so only key holders appear as present.

**Reads are open** to anyone who knows a document id, because the design never relied on
the server to protect content and the id is 128 random bits. A read limit by address stops
scraping.

Removing a user is one write without their devices: the new header lacks them, so the
server refuses their next upload, closes their live channel, and their next head shows a
header they are not in.

## Who is here

A client opens the live channel when it opens a ward. The server tells it which
fingerprints are already there, and tells everyone already there about the newcomer. A
closed channel, or a missed heartbeat, says they left.

Each client turns fingerprints into names through its own access list, so the display reads
"Alice Smith is also here" above the ward. A fingerprint the access list does not know shows as
a device that joined since this copy was last current, and resolves on the next version.

## Every edit is a version

A command applies to the open document, and the document is pushed as the next version
right away. The server broadcasts the new version number to the other channels, and each
client fetches it and validates it completely before anything local changes.

**An incoming version is applied at once**, the way a shared document in a browser shows
someone else's typing as they type. A dialog standing open is not waited for: the user
may then be looking at a family somebody else just changed, and the command they confirm
conflicts and is handed back as a single line to redo. Holding the version back until the
dialog closes would widen the window instead, for as long as the dialog stays open.

**So a connected ward's base is always current**, and that is what makes replay the
exception rather than the path. For a conflict, two users have to change the same thing
while one of them has it open in a dialog, or commit inside the gap between one version
landing on the server and the notice of it arriving at the other.

**It also keeps the cost of a conflict small while users work together.** The journal
empties as each push succeeds, so a connected user's journal holds at most the one
command they just made: losing that race costs one redo, of something done a moment ago.
A journal long enough for abandoning it to hurt belongs to a device that has been
disconnected, which is also when nobody else is publishing.

A ward is small enough that sending the whole encrypted blob per edit costs nothing worth
designing around, and edits are rare enough that per-operation encrypted deltas would be a
protocol built for traffic that does not exist. This holds during an emergency too, when
several responders are logging attempts at once, and the design carries no bandwidth
estimate because none would change it.

**A download replaces nothing until it validates.** The key unseals, the payload decrypts, the header matches the access list, the version is not
below the base this device holds, and the version descends from one this device holds. Any
failure keeps the local file untouched and is reported as a server fault: the local file is
the one copy that has to survive a server serving something wrong. A head that leaves this
device out is not a fault: it is the ward no longer being this device's, handled under
*Everything else, from the document*.

## The journal

Every command a user runs is appended to a journal, and leaves the journal when the
version carrying it is on the server. Content and access changes go into the same journal in the order they were
made, because they are the same kind of thing: an edit to one encrypted document. The
journal is therefore the work this device has done that nobody else has seen yet, and
**"there are unpushed changes" is "the journal is not empty."**

**The journal lives inside the local file.** The local file's encrypted payload holds the
ward and its access list, plus the state that belongs to this device alone: the journal,
the in-flight push record, and the to-redo list. A pushed version's payload holds the ward
and its access list only. In memory these are separate, so a push serialises the ward and
the local save writes the ward with the device's own state beside it; nothing is trimmed. One `QSaveFile` write
carries the document and its journal together, so a crash cannot leave one without the
other, and the journal is encrypted exactly as the ward is.

A journal entry is the command in its serialised form, the output of its `toJson`: what it
changes, the values it expects to find, the values it sets, and its description. So the
journal **reads as a list of what this user did**, and those descriptions become the
to-redo list when replay cannot finish. The journal holds what it saves: a command is
serialised once, when it enters the journal, and every later save writes the stored entry
as it is. Replay rebuilds each command with `fromJson`.

**A description is a heading and a line per value set**, so it says what to do again:

> Smith family
> - Bob, phone: 555-2222
> - address: 12 Elm St

Names are resolved at construction and stored with the entry, so a description stays right
after the family is renamed or deleted. Each kind of field renders its own line, and each
line passes through `tr()` as a unit. The undo menu shows the heading; the to-redo list
shows the heading and its lines, with long values such as a special-needs note in full.

**A command is immutable once constructed.** Its start condition, its result and its
description are all fixed by the constructor; `execute` takes the document and leaves the
command alone, and `inverse` builds a new command rather than changing this one. This is the property that makes replay safe, and it
is a constraint on commands rather than machinery around them: a command that read the
document inside `execute` to remember what it was replacing would have that memory
overwritten by the very version it is being replayed onto, and would then write a stale
directory over the other user's work without noticing. The two imports -
`ImportWardDirectoryCommand` and `ImportMinisteringCommand` - take their previous state at
construction for this reason, rather than reading it inside `execute` as they do now.
`SetFamiliesCommand` has no callers and is deleted.

**The start condition is what the user saw.** An edit that opens a panel or dialog
captures the values it shows when it opens, and the command is built from those, rather
than from the document re-read when the user confirms. An incoming version may have
changed the same thing while the dialog stood open, and only a start condition taken at
open can notice.

**A start condition covers exactly what the command changes.** An update command is built
from the difference between what the edit opened with and what it saved, and holds the
before and after of the changed fields only. The fields are typed per command:
`UpdateFamilyCommand` holds an optional address change, an optional coordinates change, and
a change per member keyed by `PersonId`. The command applies when every field it holds
still has its before value, its result holds when every one has its after value, and
anything else is a conflict. A geocode therefore collides only with an edit that changed
the coordinates, and two users editing different members of one family both apply.

Online the journal is almost always empty: a command enters it and leaves it as the push
succeeds. Offline it grows, and it survives closing the app, which is what makes a week of
disconnected work no different from a second of it.

**One push is in flight at a time, and the journal records it.** Before pushing, the
device saves with the journal the base version it is pushing from and how many entries
the push carries. A success removes exactly those entries, so a command made while the push
was in flight stays. When a push's answer never arrives, the device reads the head on its
next sync: a version one above the recorded base, published by this device, is its own
push, so the entries it carried leave the journal and the base moves up without a replay.
Any other head means the push did not land, and the ordinary path runs.

**Making every write a command is the core of this work.** What exists today is family
add, update and delete, team edits, emergency-asset edits and the two imports, which run
through `CommandHistory`; ministering changes only by import. The tag commands exist and
convert with the rest, though nothing in the interface calls them yet. The rest has to join them, because a write that is not a command cannot
replay:

- **Emergency responses become a keyed collection.** `Document` holds one
  `std::optional<EmergencyResponse>` today, and `EmergencyManager` assumes at most one:
  `startEmergency` returns early when one is active, and `isActive()` gates the response
  views throughout the interface. Each response gains an id, the document holds them by it,
  and the interface lists the emergencies in progress and switches between them.
- **Contact status becomes the outcome of an attempt.** `FamilyResponseRecord` stores one
  `m_contactStatus` today, overwritten by the OK and Unable to Reach buttons. Instead,
  `ContactAttempt` gains a `std::optional<ContactOutcome>` (`OK` or `UnableToReach`), the two
  buttons each record an attempt carrying their outcome, and a family's status is the outcome
  of its latest attempt that has one, or Not Contacted when none does. `ContactStatus` and
  `m_contactStatus` are deleted, and `effectiveStatus()` reads the attempts.
- **`EmergencyManager` keeps no copy of its own.** It holds `m_response` and writes it back
  through `persistResponseData`; instead it reads the response from the document, by the id
  of the emergency being shown, and every change goes through a command. `syncFamilies`,
  which adds a `FamilyResponseRecord` for every family on each document change, is deleted:
  a family with no record reads as Not Contacted, and a command that writes to a family's
  record creates it when missing, taking the name and address snapshot then. Two responders
  touching a new family both land, since each change is an addition. The archive code -
  `m_savedResponse`, `loadArchive`, `closeArchive`, `reopenArchive` and `saveArchive` - is
  deleted with the archives themselves.
- **The emergency response** is written by `EmergencyManager` mutating it in place, outside
  the command path. Each mutation becomes a command. Where the design already treats
  several fields as one value, one command covers them: `SetTaskAssignment` for the
  assignee, notes and notification together, and `SetTaskResolution` for the notes and
  time. A `SetTaskAssignment` that changes the assignee sets the notification to none,
  since the new assignee has not been told; `assignToTeam` and `assignToPerson` keep the
  previous assignee's notification today.
- **The access list** is new in its entirety. Invite, remove a user, remove a device,
  withdraw an invite, set a role and leave are each a command, which is what lets an access
  change replay like any other.
- **`Command` itself** gains `toJson` and `fromJson` with a type registry, so a journal
  entry reaches disk and comes back as the command it was. `undo` becomes `inverse` on
  every command, and `CommandHistory`'s `markSaved` and `isDirty`, with
  `DocumentManager::isDirty` and its `dirtyChanged` signal, are deleted: every command is
  saved as it is made, so nothing is ever unsaved.
- **Links and launches** are new: the single-instance handoff on Windows and Linux and the
  `QFileOpenEvent` path on macOS, and registration of `wardplanning:` and `.wardplanning`
  in `installer.iss` (which registers `.emergencyplan` today) and in the macOS and Linux
  packaging.
- **Edits that re-read on confirm** take their start condition from what they opened with:
  the family edit panel (`MainWindow.cpp` builds `UpdateFamilyCommand` from the live
  document, while the panel already holds the copy it opened with), the team dialog in
  `TeamsView.cpp`, and the person dialogs in `NeedsSubView.cpp`.

**Automatic updates are the one exception, and stay out of the journal.** Geocoding and
ward and stake lookup produce derived data: an address in, coordinates out. A result is
recomputable by definition and is never a user's work to redo, so putting "set
coordinates for the Smith family" in the list of what this user did would describe
something they did not do and cannot retype - and under one-conflict-abandons-the-replay, a
stale result could throw away work they did do. So an automatic result applies to the
document and pushes like anything else, and if that push loses it is simply not in the
replayed document, and the next batch computes it again.

## Replay

**A command replays onto the current version, or it is handed back to the user.** This
is the whole answer to two users working at once, and there is no second mechanism.

**It applies to every change without exception.** Editing a family, recording a contact
attempt, importing a directory, inviting someone, removing a user, changing a role: all
of them are commands, all of them enter the journal, and all of them replay by the one
rule. Who may read the ward lives in the same encrypted payload as the ward itself, so a
change to it is a change to the document and needs no path of its own. There is nothing
in this design that is published any other way.

The model is `git rebase`: the journal is a run of commits, the server's version is the new
base, and each command is replayed onto it rather than two finished states being compared.
A command that cannot replay is a conflict handed to the user, as a rebase hands one to
whoever started it.

**An empty journal fast-forwards.** With nothing to replay, the device takes the server's
version as it stands, saves it locally, and pushes nothing, as a rebase with no local
commits moves the branch and writes no commit.

**A command runs the same way the first time and on replay.** Whether the user has just
confirmed it or the journal is replaying it onto a newer version, `execute` answers one
question about the version in front of it:

```
enum class Outcome { Applied, Conflicted };
Outcome execute(Document& document) const;
```

A conflict the first time it runs is a dialog that stood open while someone else changed
the same thing. It is handed back as the one line to redo, and nothing enters the journal.

- **Its start condition holds** - the values it saw are still there. It applies, and
  answers `Applied`.
- **Its result already holds** - the user it would remove is gone, the role it would set
  is already set, the note already reads what it was going to say. There is nothing to do,
  and it still answers `Applied`.
- **Anything else.** The ground moved, and it answers `Conflicted` without touching the
  document.

Two outcomes rather than three, because nothing downstream distinguishes a command that
applied from one that found its work done: both carry on, and only a conflict stops things.
What "already done" means belongs inside the command, which is the only thing that knows.

**The second case is the one merge rule this design keeps.** It is "both sides made the
same change, so take it", which discards nothing: both users wanted the same end state and
one got there first, so no intent is lost.

**Replay also runs the app's own refusals.** A command is not offered when the app would
not allow it - the only admin cannot step down, the last user cannot leave, a member
cannot add users, remove users, set roles or withdraw someone else's invite - and replay
asks those same questions of the document in front of it, because a command made when a
rule allowed it can arrive where the rule does not. The last admin's own role is untouched
in that case, so the start condition matches; what changed is somebody else's entry, which
a start condition should not be reaching into.

A refusal is its own question, asked before `execute`:

```
std::optional<QString> refusal(const Document& document) const;
```

It returns the rule's wording, or nothing, and it is the same function the interface asks
when deciding whether to offer the action, so each rule lives in one place. Content
commands return nothing; only the access-list commands carry rules. An admin-only command
records its author's user id at construction, so a removal made offline by an admin who
is demoted before it replays is refused with the member's wording. The commit step, the
first time a command runs and on replay alike, asks `refusal` and stops with its message if
there is one, and otherwise runs `execute`. The first run needs it too, because a dialog
standing open can become refused, as it can become a conflict.

A refusal stops the replay as a conflict does, and **the user is told what stopped it**:
either that the ward changed first, or the same wording the app would have given had they
tried the operation just now. The difference decides what they do next. "Someone changed
this, do it again" sends them to redo work that will be refused again, while "you are the
only admin" sends them to promote somebody first, which is the thing that actually unblocks
them.

**One conflict abandons the whole replay.** The client takes the server's version as it
stands, writes the journal's descriptions to a to-redo list, empties the journal, and tells
the user their changes need doing again. It does not apply the commands that would have
fitted. The next command starts a fresh journal and pushes as usual.

This is the one place the design chooses the simpler behaviour over the more capable one,
and it is worth saying why. Most of the time one user is editing and nobody else is
there; the rest of the time everyone connected sees each other and sees each change as it
lands, so a conflict needs two users touching the same thing at the same time. The case
where a journal grows long enough for this to cost real work is an emergency with the
network down, and there the work is additive: contact attempts, tasks and new responses are
new elements with their own ids, so they replay onto anything. What is left is a short list
a user retypes.

Applying part of a journal would buy that user a few fewer lines to retype, in exchange
for a paused-replay state the app has to hold and survive being closed in, and for versions
on the server that are half of what somebody meant. **Every version is either a clean
replay of one user's whole intent, or untouched by it.**

Then the result is pushed. If that push also loses, the journal replays again on the newer
version, which is the ordinary case of two users working in the same minute.

**The to-redo list is what the user works from.** It holds the abandoned journal's
descriptions in their own sequence, marks the one that clashed, and survives closing the
app until the user dismisses it. It holds descriptions, not commands, because nothing
replays it. Nothing is settled by choosing a side, and nothing is silently dropped - the work
is still described for as long as they keep it.

**Why replay rather than reconciling two versions.** A command says what someone meant to
do, so replaying it produces the current ward plus work a user recognises. Comparing two
finished versions field by field is the merge rather than the rebase: it produces a third
state nobody authored and nobody can check, and it needs a rule for every pair of shapes in
the document. Replay needs the one rule above, and what makes it safe is a command knowing
what it saw.

**An emergency needs no special case.** A ward holds its emergency responses as a keyed
collection, each with its own id, so starting one is an addition like any other and collides
with nothing. Several responders working offline each record `RecordContactAttempt` entries,
which are additions too, and the whole journal replays. Marking a family OK or Unable to
Reach records an attempt as well, so two responders who mark the same family differently
both land, and the family shows the later outcome with both in its history. Two responders who each started a
response while disconnected end up with two events, each carrying the records its own
responder made, because the app has no way to know they were the same emergency and does not
pretend otherwise. The app lists the emergencies in progress, and the user switches to any
of them. Whoever is coordinating ends the one they do not want.

**While the network is down, responders cannot see each other's work.** The server relays
nothing it cannot reach, and each device holds its own partial picture until connectivity
returns. Cell data and tethering often survive when home internet does not, and ward
emergency plans already assume radios and runners for coordination. Peer-to-peer sync over
a local network would genuinely close the gap and is a substantial subsystem of its own; it
is named here as possible future work rather than folded in.

Any responder can work any family. Contact attempts, outcomes, tasks and responses are all
additions, so two responders reaching the same family both land. The two task edits,
assignment and resolution, are the exception: two users deciding the same task differently
is a conflict, which is the right answer for a real disagreement.

## Imports

An import is one command, and **it follows the same rule as every other: its start
condition covers exactly what it changes.** At construction it works out every change it
will make, each with its before and after:

- **A ward directory import**: the families it adds, updates and removes; for each removed
  family, the cleanup of its people from teams, tags, emergency assets, and ministering
  groups and districts; the directory PDF date; and the ward record it creates or updates.
- **A ministering import**: its organisation's districts and groups, the families it adds
  or updates, and the ministering PDF date.

`ImportWardDirectoryCommand` and `ImportMinisteringCommand` read all of this inside
`execute` today, and the directory import's `undo` restores none of the cleanup; both move
to construction. An import then conflicts only with an edit to something it also changes. A
family someone else added in the meantime is not among the ones it removes, so it survives.
The inverse is the same changes swapped, so undoing an import restores the memberships its
cleanup removed.

Its journal entry is the largest the journal holds, which is why **what the user reads is
one line, not the entry.** The command carries the name of the file it read, so that line
names what to do again:

> Import the ward directory from `Tanglewood.pdf`.

## What the document carries

The access list inside the payload names everyone who may read the ward, and carries each
device's public keys, because those are what the data key is sealed to:

```
accessList:
  users:
    - id: <user id>
      name: "Alice Smith"
      role: admin | member
      devices:
        - { name: "Laptop", publicKey, verifyKey }
        - { name: "Pixel 8", publicKey, verifyKey }
  invites:
    - { for: device | user, maker: <user id>, role, label: "for Bob", publicKey, verifyKey }
  formerKeys: [...]
```

The header names one recipient per key: every device, and every open invite. Header and
access list must agree exactly. The server sees fingerprints only.

**User ids** are 128 random bits, made when someone creates a ward or joins it through
*Add a user*, so they are per ward and distinct from the directory's `PersonId`, which
names a ward member. A device joining through an *Add a device* invite takes the inviter's
user id, name and role; one joining
through an *Add a user* invite makes a new user with the name it gives. An invite's
`label` is the inviter's note, so open invites can be told apart in the list; it is not
carried onto the user.

**Roles are a convention among honest clients.** The server cannot read the access list, so it
cannot tell an admin from a member. The rule (admins add users and remove users; everyone
adds and removes their own devices) is what the app offers, and it exists to keep
well-meaning users from over-sharing.

## Who has access, changed by two users

An access change is a command like any other and replays like one. Two admins removing
different users both apply. Two admins removing the same user: the second finds its
intent already satisfied and there is nothing to do. Two admins setting one user's role
differently is a conflict, so the second admin's replay is abandoned and they do it again
knowing what the first one did.

Mutual removal needs no rule of its own: if two admins remove each other while offline,
whoever pushes first stands, and the other's write is refused because the current header no
longer names them. They learn the ward is no longer theirs, and their own pending removal
stops mattering. A ward cannot be emptied by two users leaving at once.

The last user still cannot leave, because a ward with nobody to read it is not a state,
and leaving needs the server, since the version without you must be held there before your
copy goes.

An invite made on one device and a join landing on another are different commands and cross
without troubling anyone.

## Invites

An invite is a one-time bearer key. Its 32-byte seed yields both of its keypairs through
`crypto_kdf_derive_from_key` with context `wpinvite`: subkey 1 is the seed for
`crypto_box_seed_keypair`, and subkey 2 the seed for `crypto_sign_seed_keypair`.

1. **Add a device** or **Add a user** (admins only for the latter) generates an invite
   keypair from a random 32-byte seed, writes a new version whose header seals the data key
   to it, with an `invites` entry saying what it is for, and shows the link:
   `wardplanning:join/<document id>/<seed, base64url>`. What the user copies is a short
   message, not the bare link, because the recipient may not have the app yet:

   > Alice Smith has invited you to Tanglewood Ward in Ward Planning. Install it from
   > https://github.com/jvilburn/EmergencyPlan/releases/latest, then open this link:
   > wardplanning:join/...

   The download address is the project's release page, which the app also checks for its
   own updates. The user sends the message by email, text, or any channel they trust
   with it.
2. **Opening the link** on a device: derive the invite keypair from the seed, fetch the
   current version, unseal the data key with the invite's private key, validate as any
   download is validated, then write the next version: the invite entry becomes a device
   entry, under the inviter's user for a device invite, under a new user for a user
   invite, named by the joiner, signed by the invite's signing key, whose verify key the
   current header names. The device then downloads the current version as itself and writes
   it. Before using an invite, the device reads the head: if it already names this device's
   own fingerprint, an earlier join landed and only its answer was lost, so the device
   opens the ward as itself and leaves the invite alone.
   From then on the device signs as itself. The ward opens.
3. **Withdrawing** an unused invite is a write without its entry, by whoever made it or by
   an admin. Removing a user withdraws the invites they made.

The link is the gate. Whoever opens it first gets in, visibly, and can be removed.

An invite link is about 90 characters: fine to paste, long to type. A QR code can show it
for a camera later if a phone client ever wants one. If a web page is ever hosted for the
app, the link can become `https://.../join#<id>/<seed>`, one address that offers the
download when the app is missing and opens the app when it is there; the seed stays in the
fragment, which browsers do not send to the server.

## Everything else, from the document

- **Who has access**: the access list, read from the open ward. Add a device, Add a user,
  Remove, Make admin / Make member, Withdraw invite, Leave, Unlink (own devices), Details.
- **Access notices**: the client diffs the access list between the version it last dismissed and
  the current one, one line per entry that differs: joined as member or admin, no longer
  has access, now an admin or a member, a device added or unlinked, an invite made or used.
  The lines name whose entry changed, never who changed it; this device's own edits are news
  like any other.
- **The ward is no longer yours**: a head whose header lacks this device. The reason is read
  off the previous access list and the publisher: removed by *name*, or left, or this device
  unlinked. The ward stops pushing until a head names this device again, the user is told, and the app offers to delete the
  copy. Anything in the journal is no longer theirs to push.
- **Last admin**: a client rule: the only admin cannot step down, and cannot leave while
  others remain.
- **A second device of your own**: *Add a device*, send yourself the link, open it there.
  The first-run screen asks for a name and a device name, nothing else.
- **Cleared app data, a reimaged profile or a replaced device** gives a fresh identity; wards come back by invite from
  another holder, or from your other device. The server keeps no index of who holds what, so
  there is no ward list to restore from it.

## Saving

**A command writes and pushes as it is made.** Every command in the app is a completed
action: a note is one command carrying its finished text when the user clicks OK, and so
is a contact status, a contact attempt, a task change or an assignment. They arrive at the
speed a user fills in a dialog, so there is nothing to batch, and a local write is a
compress, an encrypt and a `QSaveFile` of a couple of hundred kilobytes. Saving on a timer
would buy nothing and would hold a change back from everyone else for as long as it ran.

**Automatic updates run on any device that holds the ward.** Geocoding and ward and stake lookup change families and ward details on their
own, the moment the network returns. They are refused while the directory is read-only,
which happens when the server holds a version this app is too old to read, and then
nothing is pushed at all. Their results are not journalled, for the reason given above.

**All writes go through `QSaveFile`**, so a crash mid-write leaves the previous file intact.

**A command is done when its save succeeds, and the push follows the save.** If the save
fails, the command's inverse is applied in memory, nothing is pushed, and the user is told the
reason. Memory, the local file and the server therefore never disagree, and a problem that
persists, such as a full disk, refuses each command with its reason until it is fixed.

**An undo is the command's inverse, run like any other command.** `Command` has
`CommandPtr inverse() const` in place of `undo`: for an update, the same command with
before and after swapped; for an add or a delete, the opposite command; in every case with
the heading "Undo: *original heading*" over the original's lines. Undo builds the inverse and sends it
through the commit step - `refusal`, `execute`, journal, save, push - and redo runs the
original again the same way. An inverse onto moved ground conflicts and is handed back like
any stale edit, so incoming versions leave the undo stack alone.

The journal and the undo stack are separate, for different reasons. The journal is an
append-only log of serialised entries for what has not reached the server, and an entry
leaves when its push lands. The undo stack owns command objects, as `CommandPtr` does
today, for what this user can take back, and an entry stays after it is pushed. An undo
moves the original from the undo stack to the redo stack and appends the inverse's
serialised form to the journal; the inverse never enters the undo or redo stack. A redo
moves the original back to the undo stack and appends its serialised form to the journal
again.

The undo stack is in memory and belongs to the session: it is cleared when the document is
closed, opened or switched, holds at most 100 entries with the oldest dropped, and its redo
side is cleared by any new command, as today.

## Where documents live

The app owns the file. Documents are stored in the local app data directory, named by
document id, and **the user never chooses or sees a path in the normal flow.** "Open" is a
list of the wards this device has access to, not a file browser. This is the largest of the
ease-of-setup simplifications.

**The list is local**, built from the wards this device holds. The server keeps no index of
who holds what, so the list is this device's own record and nothing restores it from the
server.

A new document is created in app data under a fresh id, and naming it is a display concern.
Its first push is `POST /documents`, made as soon as the network allows, and its journal
empties then like any other.

Two escape hatches remain, and both earn their place:

- **Save a copy** writes a `.wardplanning` file wherever the user likes, under a fresh
  document id. Users want backups.
- **Show in folder** reveals the app data directory, for support and for the rare hand-off.

**Opening a copy** reads the file and opens it read-only. Writing a copy back over a shared
document is not an available move, and the wording says so rather than implying the files
can be rejoined.

## Links and launches

**The running app receives a file or a link.** That is the one entry point: opening a
`.wardplanning` copy and opening a `wardplanning:join/...` link both arrive as a path or URL
handed to the app that is already running, which opens it. How it gets there is per
platform:

- **Windows and Linux**: one instance per user. The app listens on a `QLocalServer` named
  for the user; a second launch connects, sends its argument, and exits. This also keeps two
  processes from writing the same local file and journal.
- **macOS**: the system delivers files and links to the running app as `QFileOpenEvent`.
- **iOS and Android**, if a phone client is ever built: the system runs one instance and
  delivers links through `QDesktopServices::setUrlHandler`.

**Registration** is the installer's or bundle's job: the Windows installer registers the
`wardplanning:` URL scheme and the `.wardplanning` extension; the macOS bundle declares both
in `Info.plist` (`CFBundleURLSchemes` and a document type); the Linux `.desktop` file
declares `x-scheme-handler/wardplanning` and the file type; a phone build would add an
Android intent filter or an iOS `CFBundleURLSchemes` entry.

## Single key holder notice

While the ward has exactly one user, the app shows a **persistent, dismissible notice**,
not a per-save interruption: *Only you can open this document. If you lose your devices, it
cannot be recovered. Add someone now.* A user with a single device also sees a gentler
suggestion to add a second one.

**While the ward has exactly one admin** and anyone else, admins see a second persistent,
dismissible notice: *You are the only admin. If you lose your devices, nobody can add or
remove users. Make someone else an admin.*

Every ward starts in these states and some will stay there, so a warning that fires on
every save would be trained away precisely where it matters most.

## Emergency archives are removed

Archiving an ended emergency as a snapshot of the whole document is removed. Each archive
would be a complete copy of the ward directory, sealed to the devices as they stood when it
was written, so removing someone would mean re-keying every archive. The feature's value, a
record of a past emergency, does not carry that weight.

**The summary report is the record of an emergency.** It is the only thing that outlives
the response, so ending an emergency is a confirmation whose report option is offered
clearly rather than as an afterthought. The report is a plaintext PDF written wherever the
user chooses, which is worth saying in the same breath.

## If the server goes away

The server could stop, whether abandoned, unaffordable, or deliberately retired. This
design does not build a second sharing transport against that possibility. **Ward Planning
is open source, so a maintainer can adapt it**, and a rescue path used once, in a scenario
that may never arrive, should not shape the interface every user touches weekly.

What the design does keep are the things that make such an adaptation possible, each of
which costs nothing to hold on to:

1. **The document is self-sufficient.** Its access list and data key live in the
   file, so every member can open, edit and share it with no server state to restore. A
   successor needs the blobs and nothing else.
2. **The format is documented publicly** in the repository, as a specification rather than
   only an implementation. This is what makes "someone could adapt it" true rather than
   aspirational.
3. **Every member holds a complete, readable copy**, and Save a copy gets it out of the app.

**A successor restores the service rather than members detaching from it.** Because
documents carry only their id and the app supplies the server address, continuity is an
operator task that members never see: at the same domain, the successor deploys the
open-source server from the export of blobs and apps reconnect unchanged; at a new domain,
the successor also ships an app release pointing at the new address, and every existing
document follows it.

Members keep editing their local copies throughout, and sharing resumes once the service
returns.

## Failure paths

| Condition | Message |
|---|---|
| A command could not replay | Your changes could not be applied, because the ward changed first. These need doing again: *the to-redo list*. The one that clashed: *its description*, and what that now reads. |
| A removal among them | ... and *name* still has access until you remove them again. |
| A rule refused a replayed command | The wording the app gives for that operation, as it would if the user tried it now: *You are the only admin...* |
| The local save failed | Your change could not be saved: *reason*. Nothing was changed. |
| The server is unreachable | Working offline. Your changes are saved here and will go up when the connection returns. |
| A download failed to validate | The server returned a version this app could not accept. Nothing was changed. |
| Head is older than this copy | The server returned an older version of this ward. Nothing was changed. |
| Header format is newer | This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work. |
| Invite link already used or withdrawn, or not yet pushed | This invite has been used or withdrawn, or has not reached the server yet. Ask the sender. |
| Invite link for a ward the server does not have | The server has no such ward. Check the link. |
| Opening a link while offline | Connect to the internet to join a ward, then open the link again. |
| Not an admin | In *Who has access*, a member sees *To add or remove users, ask an admin: names* in place of the Add a User button. |
| Head lacks this device | Removed by *name* / You left / This device was unlinked by *name*, with an offer to delete this copy |

## Debugging

The document's plain JSON (`Document::toJson`) serves a developer-only "export decrypted
JSON" action, off by default, which warns that it writes unencrypted member data.

## Earlier versions

Ward Planning reads only its own `.wardplanning` documents and its own settings. Files and
settings from earlier releases are not migrated; an `.emergencyplan` file is treated like
any other file the app does not own.

Startup restores the last document from the ward list by id. A failure is reported and the
record left alone.

## Identity recovery

Device keys live only in each device's local app data. They survive an uninstall and
reinstall, because the uninstaller leaves local app data in place: `installer.iss` has no
`[UninstallDelete]` for it, and keeps none. Clearing local app data, a reimaged profile, or
a replaced device loses them.
**A user with another working device** invites the new one from it, and no one else is
involved. **A user who has lost every device** is removed by an admin and invited again,
which seals the data key to the new device. The single-holder and single-admin notices, and
the suggestion to add a second device, are what keep a ward out of the state where neither
is possible. There is no exported recovery key file.

## Testing

**The harness is part of the work.** `tests/CMakeLists.txt` lists no sources, so the suite
starts here: one Qt Test executable linked against `EmergencyPlanLib`, with `tests/services`
for format, crypto, journal and replay, and `tests/server` for the fake server and the
conformance suite.

- **The server interface** is one abstract class in the client with the five calls and the
  channel: create, head, fetch, push, and open channel. `HttpServer` implements it for the
  app; `FakeServer` implements it in process for tests, holding versions in memory and able
  to drop a reply after storing a write, delay a notice, or refuse to answer.
- **A test client** is a full client - its own identity, app data directory, document,
  journal and undo stack - so a multi-user test is two or three of them against one
  `FakeServer`, driven step by step with no timers.
- **Every command type is covered by the registry.** Each type registered for `fromJson`
  also registers a sample instance, and the round-trip test iterates the registry, so a new
  command type cannot be added without a sample and a passing round-trip.

Format and crypto, in `tests/services`:

- `DocumentCrypto` round-trip
- two consecutive saves of an unchanged document produce different nonces
- mutating any header byte causes decryption to fail, including the document id
- the known-answer vectors for the fingerprint, the invite keypairs derived from a fixed
  seed, and the signed line
- a header with an unknown format version is refused whole; the directory is read-only and
  nothing pushes, while emergency response edits save locally; the state clears as soon as
  the server again serves a version the app understands
- a header or payload with a missing or malformed required field is refused, never read in
  part
- a version whose header lists a key the access list lacks, or whose access list has a key with no
  header entry, is refused
- a head whose version is below this copy's is refused and changes nothing
- a removal rotates the key, after which the removed user's devices cannot unseal; a write
  that removes nobody keeps the key; the written header has exactly the expected entries,
  each matching an access-list key
- a device the server mentions but this client did not admit is never sealed to
- Save a copy produces a file with a fresh document id and the whole access list as its
  recipients; Open a copy opens it read-only without touching the shared document

The journal and replay, in `tests/services`:

- every registered command type's sample round-trips through the journal, and the journal
  survives closing the app
- an emergency response mutation and an access-list change each enter the journal, replay,
  and reach the server by the same path a directory edit does
- an automatic geocode result applies and pushes, and when that push loses it is absent
  from the replayed document and the journal is untouched by it
- an automatic result is refused while the server holds a version this app cannot read
- a family edit opened, then a remote version changing that family applied, then the edit
  confirmed: the edit conflicts, the remote change is still there, and the journal is empty
- a geocode lands between an offline edit to a family's phone and its replay: the edit
  applies, and the family holds both the new phone and the new coordinates
- two offline edits to different members of one family both apply
- an undo after a remote edit to a different family applies and reaches the server as its
  own version; an undo after a remote edit to the same field conflicts and leaves the
  remote value
- a journal whose commands all find what they expect applies in full and pushes, and the
  pushed document holds every one of their effects
- one conflict in a journal of several leaves the server's version exactly as it stood,
  including the changes that would have fitted, and the journal's descriptions are in the
  to-redo list with the journal empty
- a command whose target someone else deleted is a conflict
- the to-redo list survives closing the app, and each description equals its exact
  expected heading and lines, values included, after the family it names is renamed
- after an abandoned replay, a new command pushes successfully
- the journal, the in-flight push record and the to-redo list round-trip through the local
  file, and a pushed version's payload carries none of them
- a save that fails leaves the document in memory as it was, the local file as it was, and
  nothing pushed
- two responders working offline: every attempt from both sides lands, asserted by exact
  attempt counts per family and each attempt's notes
- two responders offline each record an attempt on a family that had no record: both land,
  in one record holding both attempts
- two responders offline mark the same family OK and Unable to Reach: both attempts land,
  neither journal is abandoned, and the family's status is the later outcome
- two responders who each start a response offline produce two events, each holding its own
  responder's attempts, and neither journal is abandoned
- an import replayed onto a version where someone else added a family applies, and that
  family is still there afterwards, asserted by its id; an import replayed onto a version
  where someone else edited a family the import changes is a conflict
- undoing a directory import that removed a family restores its people's team, tag, asset
  and ministering memberships
- a local save that removed someone still accepts the pre-removal version from the
  server, and the journal replays onto it: the save did not rotate, so the one key this
  device trusts is the one the server's version is under
- a push that carries an access change rotates, and the version after it names the old
  key among its former keys
- replaying a command does not change the command: the same journal entry replayed onto a
  moved version and then onto an unmoved one conflicts and then applies, which fails if
  anything captured its start condition while running
- an access-list command replays: two admins removing different users both apply; removing the
  same user twice applies in full; two different roles for one user is a conflict
- the last-admin and last-user rules refuse the command that would break them, both when
  the user tries it and when it replays onto a version where the rule has come to apply:
  the ward is left with an admin, and the message is the rule's rather than a conflict's
- a removal made offline by an admin who is demoted before it replays is refused with the
  member's wording, and the user it named is still there

Against the fake server, in `tests/server`:

- pushing from the current base version succeeds and advances it
- pushing from a stale base version is refused, and the journal replays on the fetched
  version and pushes again
- a write whose signature is not from a device the current header names is refused
- a push from an exported copy is refused, because no document exists under the copy's id,
  and the ward's document keeps its version and contents
- a downloaded version that fails validation replaces nothing
- edits made while unreachable are kept in the journal and replay on reconnect
- a push the server stores but whose reply never arrives: on the next sync the device
  recognises its own version, the entries it carried leave the journal without a replay or
  a conflict, and nothing is applied twice, asserted by exact counts of the added elements
  (contact attempts, tasks, responses) and an unchanged access list; repeated with a journal
  that sets one field twice, and with the app closed before the next sync
- a command made while a push is in flight stays in the journal when that push succeeds,
  and goes out with the next push
- a server ahead with an empty journal fast-forwards without a prompt
- a device whose head lacks it stops pushing, keeps its file until the user agrees, and
  says who removed it, both mid-session and on next open; when a later head names it
  again, it resumes syncing
- a user joins only through an invite the current header names, and a used or withdrawn
  invite is refused
- a join whose answer is lost, with the link opened again, opens the ward as a member and
  never reports the invite as used or withdrawn
- two admins removing each other: the first stands and the second's write is refused
- the channel refuses an open not signed by a device the current header names, and accepts a
  key holder's
- the channel reports who is present, reports a newcomer to those already there, and reports
  a departure on close
- a version notice on the channel causes the other clients to fetch and validate

The fake and the real server are checked against each other. One conformance suite, written
against the client's server interface, runs against both: against the fake in the ordinary
test run, and against the real server over HTTP when `WARD_PLANNING_SERVER_URL` is set. A
disagreement is resolved in whichever implementation is wrong, never by loosening a test.

## Deliberate limits

- **One conflict abandons a whole replay.** The user redoes everything in that journal
  from the to-redo list, rather than part of it being applied for them or a rule
  choosing a winner.
- **Responders cannot see each other's work while the network is down.** Their commands
  replay when it returns. Local-network peer sync would close this and is possible future
  work.
- **Losing access stops sharing; deleting the copy is the user's choice.** Copies taken
  out of the app, and devices that never reconnect, keep what they had either way.
- **Roles are a convention.** A modified client could act as an admin, and already holds the
  data.
- **One admin can remove anyone** from the ward's future. Every removal is announced, and
  everyone keeps the copy they hold; recovery is a new ward built from the directory PDFs.
- **A stolen, unlocked device acts as its owner**, with its owner's role. Its owner
  unlinks it and any device it added; otherwise any admin removes its owner.
- **A device's first download is taken as it stands.** A newly joined device, or one whose
  app data was cleared, has no earlier version to check the first against.
- **An intercepted invite link gets in**, visibly, and is removed.
- **A channel-open signature replays**, so presence can be lied about.
- **Joining needs the server**, so users are added before an emergency rather than during
  one.
- **Reports stay plaintext.** `ReportGenerator` writes member-data PDFs to a user-chosen
  path.
- **Emergency archives are gone**, along with the record of past emergencies.
- **Sharing and live editing depend on the server being up.** Editing does not.
- **A hostile server operator could keep members on separate or stale copies** without them
  noticing; nothing is exposed, and each member keeps their own work.
- **There is no folder-based sharing mode.** Continuity if the server stops rests on the
  project being open source, the format being documented, and every member holding a
  complete file.
- **Operating the server is ongoing work** (uptime, backups with a tested restore,
  certificate renewal, and abuse handling) carried by whoever runs it rather than by the
  ward.

## Hosting notes

Preliminary findings to seed the server's own design. Figures marked *verified* were checked
against Cloudflare's documentation on 2026-09-13; the rest are recalled or estimated and
need confirming.

**Cloudflare Workers with one SQLite-backed Durable Object per document** is the
recommendation. A Durable Object is a single-threaded unit with its own storage, which maps
directly onto what this design asks: atomic version checks, the blob, and the live channels
for one ward held in one place, so broadcasting a version notice to everyone present is a
local operation. Durable Objects accept WebSockets and can hibernate them, which is what
makes a held-open channel per reader affordable. Patching, TLS, uptime and DDoS protection
belong to the platform, which suits a server operated by one volunteer. It also runs away
from any local disaster the ward is responding to.

The runner-up is a small VPS running a plain server over SQLite, with Caddy for TLS and
Litestream for continuous backup. It is portable and anyone can self-host it, in exchange
for the operator carrying patching, uptime and restore testing.

**Tailscale serves the operator, and only for development.** Members reach the server over
the public internet with no extra client, because a tailnet would reintroduce a second
access list. Tailscale Funnel can expose a home machine publicly over HTTPS, which is
useful for testing against a real server; production belongs on infrastructure that stays
up when the operator's house does not.

| | Cloudflare Workers + Durable Objects | VPS + SQLite + Caddy + Litestream |
|---|---|---|
| Reliability | Distributed; rare global incidents, which take much of the web down too | One machine; roughly 99.9%, recovered by the operator |
| Cost | Free tier, or about $5/month paid (recalled) | About $4-6/month plus cents for backup storage (recalled) |
| Maintenance | Deploying code | OS updates, monitoring, backup checks, restore tests: about an hour a month plus incidents |
| Backups | Built-in point-in-time recovery, believed 30 days (recalled) | Litestream to object storage, set up and tested by the operator |
| Portability | Tied to Cloudflare's APIs | Runs anywhere |
| Abuse handling | Rate limiting and DDoS protection included | Built by the operator |

Server downtime stops sharing and live editing and leaves local editing untouched, so
reliability is not the deciding factor. The choice is maintenance against portability.

**Capacity needs working out for a held-open channel.** Free-plan limits (*verified*):
100,000 Worker requests and 100,000 Durable Object requests per day, reset at midnight UTC;
100,000 SQLite rows written and 5 million read per day; 5 GB storage total, 1 GB per object;
2 MB maximum row or BLOB size. What is not yet estimated is how a hibernatable WebSocket per
open ward bills against those limits, and what a version per command costs for a ward in
ordinary use. Both
numbers decide how many wards the free plan holds, so they belong in the server's own design
rather than guessed at here.

**Storage is ample.** At an estimated 200 KB compressed per document, 5 GB holds about
25,000 documents. Every document reaches the server, throwaways included, and the store
grows with documents rather than with edits. Pruning is an operator action and not part of
this interface.

**The 2 MB BLOB limit** is checked against a real document carrying an emergency response. A
larger payload is chunked across rows or stored in R2.

**Running unattended** needs billing on the free tier or on a payment method that outlives
the operator, the domain registered for the maximum term with auto-renew, and no secrets
that need hand rotation.

**Trust model: hostname plus TLS.** Whoever controls the domain is the server, which is
exactly what lets a successor take over at the same address. Clients decrypt every blob
themselves and seal only to keys an earlier version already admitted, so a lapsed domain
re-registered by a stranger yields ciphertext and fingerprints, and stops sharing until a
successor acts. Such a stranger cannot remove anyone, learn who has access, or cause a copy
to be deleted.

**Handing over** needs a second Super Administrator on the Cloudflare account, credentials
(registrar, Cloudflare, GitHub) in a password manager with emergency access, a second owner
on the GitHub repository, a runbook in the repository covering deploy, data location,
restore and migrating to another host or domain, and a scheduled export of all ciphertext
blobs to a second provider the successor controls. The export is ciphertext and fingerprints
only, which is a smaller thing to hold carefully than a members list would be.

**Surviving with no successor**: every member already holds a complete, editable local copy
and keeps working; sharing is what pauses.

## Open points

- **The live channel is not specified** to the level the rest of the wire is: the open
  request and where its signature rides, the presence and version-notice messages, the
  heartbeat interval and timeout, the reconnect rule, and how errors map. The conformance
  suite needs that before it can be written.
- **Whether head polling stays** as the channel's fallback, and at what interval.
- **Where the server runs** is not chosen, and free-plan capacity under a held-open channel
  is not estimated.
- **Reads are open.** Anyone with a document id can download ciphertext. If that ever
  matters, the same signature can gate reads; it costs nothing in the format.
- **Creating documents is open.** A per-address limit on `POST /documents` is the first
  answer; a shared secret in the app is the second.
- **Access-list size in the payload** is a few hundred bytes per user and does not matter.
- **Pruning unused documents.** Every document reaches the server, so throwaways
  accumulate. The head already carries the current version's publish time; adding a
  last-read time per document, touched when a head is read, would let an operator rule
  prune a document nobody has opened or written in a year.
- **Recovery from a copy.** Recovery is a new ward rebuilt from the PDFs. A saved copy could
  be published as the new ward instead, keeping notes, ministering and emergency data the
  PDFs lack: one "Publish as a new ward" action on an opened copy, which creates the document
  and removes users from its access list as it goes, so the device that caused the trouble
  is not carried over.
