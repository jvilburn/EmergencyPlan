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
| Running instances | One per user (see *Receiving an invitation*) |
| Where the local file lives | Local application data (`QStandardPaths::AppLocalDataLocation`), keyed by document id; the user never chooses a path |
| Third-party sync folders | Not built. Continuity rests on the project being open source. |
| The split of responsibility | **The ward says who may read it; the server stores versions and relays changes.** Besides the version condition, the server checks only that a write is signed by a device the current header names. |
| Who has access | The access list inside the encrypted payload: users, their devices, and open invites |
| Key model | Envelope encryption, one data key per document, sealed to each device |
| Key generation | Silent, on first run. No user-visible key material. |
| Server authentication | None. Each write and each channel carries an Ed25519 signature the current header admits. |
| Joining | A one-time invite link |
| Roles | Admin or member, held in the access list, a convention among honest clients |
| Who may add | An admin adds a user; anyone adds their own devices |
| Who may remove | An admin removes a user; anyone removes their own devices, or leaves |
| Removal | One push without their devices, which rotates the data key |
| Concurrent use | Presence while connected, every edit a version, and offline commands replayed onto the current version. Content and access changes take the same path. |
| A replay that changes nothing | Runs to completion and pushes, though the pushed version matches the server's. It is rare, so it follows the ordinary path. |
| Private key storage | A file in local app data, readable only by its user, holding the secret halves, as SSH keys are kept. Local app data does not roam. |
| Crypto library | libsodium 1.0.22 or later from vcpkg, to be pinned in the manifest |
| Document storage | One encrypted payload. Replay happens on the model, so storage stays simple. |
| Encryption coverage | Every document; emergency archives are removed |
| File extension | `.wardplanning`, for each ward's file in app data |

### Settled, and not for review to reopen

Each of these was weighed and decided. A review that finds one should treat it as the
design's answer, not a gap:

- **Replay, never reconciliation** of two finished versions.
- **Emergency responses are a keyed collection**; two offline starts are two events.
- **Ending an emergency saves its report and removes the response.** A change replayed onto
  a response that has ended conflicts, which abandons that responder's journal to the
  to-redo list. It takes a responder whose work in that response has not landed when the
  end does; one working only in a response they started offline is unaffected.
- **No base copy, no save counter, no debounce.**
- **No exported copies.** A copy would be sealed to the ward's devices, each of which already
  holds the ward; *Show in folder* reveals the ward's own file.
- **1.0 `.emergencyplan` files are not migrated**, and their teams, tags, assets, special
  needs and notes are not carried over.
- **An unreadable or restored head stalls sharing** until the operator repairs it (see
  *Threat model*); there is no client-side republish.
- **Tags stay**: add, update and delete are converted with the rest, though nothing calls
  them yet.
- **No bandwidth estimate**; whole-blob-per-edit holds during an emergency too.
- **No guards against a future developer's mistake**, such as a command type left out of
  the registry.
- **Items under *Open points* are known**, the live channel's wire detail among them.
- **The server is trusted to number versions.** Older content served under a higher number,
  by a server bug or a renumbered restore, is not detected.
- **Server error recovery is out of scope**: repairing a bad version, restoring from backup,
  and what an export must carry for a successor belong to the server's own design.
- **Two directory imports can add one new family or member twice** when both list it and
  neither has landed when the other is built. An import gives each family and member it
  cannot match a fresh id, so the one that lands second finds none of its additions already
  there and adds them. The duplicate stays until a user removes it.

### Words the user sees

The app is for casual users, so what it shows is plain words, never the design's own terms.
This document uses its terms for precision; the interface uses these:

| In this design | In the app |
|---|---|
| document | ward |
| access list | *Who has access* |
| user | user, or the user's name |
| device | device |
| invite (user / device) | *Add a user* / *Add a device*, and the link message |
| remove a user | *Remove* |
| remove a device | *Remove device*; *Remove this device* for your own |
| leave | *Leave* |
| withdraw an invite | *Withdraw invite* |
| role | *admin* / *member*; *Make admin* / *Make member* |
| conflict, abandoned replay | *Your changes could not be applied, because the ward changed first* |
| to-redo list | *These need doing again* |
| journal, unpushed changes | *Your changes are saved here and will go up when the connection returns* |
| no longer this device's | *You no longer have access to this ward* / *You left this ward* |

Terms with no place in the interface at all: push, fetch, replay, version, head, base,
journal, landed mark, key, data key, rotation, seal, fingerprint, header, payload, document
id, link (as a verb), unlink. New interface text follows the same rule: a word a casual
user would use, or none.

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
server fault: a head whose version is below the base this device holds is refused, and
the device keeps its copy and stops pushing, the accepted stall below. A hostile server can
relabel an old version, which is the accepted stale-version case below.

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
content, remove its owner's other devices, add devices of the thief's own, and on an
admin's device anything an admin can do. Its owner removes it from another device, or any
admin removes the owner, who is then invited again. Planted devices are shown for what
they are: each appears under its owner's name, and the owner removes any they do not
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

**Accepted: an unreadable or restored head stalls sharing.** A version every client
refuses - written by a client bug, or corrupted in storage - or a server restored to
versions older than members hold leaves every device unable to push until the server
again serves a version each can take. Every device keeps its local copy and keeps working
on it, and nothing in those copies is lost. Repairing the server is the server's own
design, outside this one.

**Cleaned up when the user asks.** A removed member's local copy: their app offers to
delete it once it learns the ward is no longer theirs.

**Outside the app's reach.** Reports exported by `ReportGenerator`, which remain
plaintext PDFs.

**Known to the server, and accepted.** Per document: the ciphertext and its size, which
leaks approximate ward size; the fingerprints in each version's header, which count the
devices with access without naming anyone; which fingerprint published when, and from
what address; which fingerprints hold a ward open at the same time; and, because a device
has one identity, which wards share a device. The server learns
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
`\n` is always the boundary, and the header stays readable in a text viewer. On the wire, a
version travels in this same framing, as the raw request or response body, and
`GET /documents/{id}/head` returns the header line as its body, with the version,
publisher fingerprint and publish time in response headers (see *What the server is*).

**Header**

- format version, which starts at 1
- document id: 128 random bits generated by the app when the document is created, and
  never changed
- payload nonce
- recipient entries, one per device and one per open invite: `{fingerprint, data key
  sealed to that X25519 public key}`

The header is a JSON object with the keys `format` (an integer), `document`, `nonce`, and
`recipients`, an array of objects with the keys `fingerprint` and `sealedKey`. Every binary
value is base64url without padding: the document id is 16 bytes (22 characters), the nonce
24 bytes (32 characters), a fingerprint 16 bytes (22 characters), and a sealed key 80 bytes
(107 characters), a 32-byte data key plus `crypto_box_SEALBYTES`, which is 48.

**A deleted ward's final version** is an ordinary header with an empty `recipients` array
and a fresh `nonce`, as on any write, and its body ends at the header's `\n` with nothing
after it; the signed line's payload hash is BLAKE2b-256 of zero bytes. A reader recognises
it by the empty `recipients`, before any payload rule applies (see *Who has access, changed
by two users*). No local file takes this form: the deleting device removes its copy, and
every other device keeps the copy it had.

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

**The reader for a format version reads exactly what that version's `toJson` wrote.** A
field `toJson` leaves out when empty reads as empty, and that is the only optional kind;
every other field is required, and nothing is defaulted, coerced, or accepted in a second
shape. Since a reader only ever meets its own writer's output, a strict parse costs nothing
in practice, and it never guesses.

A new release that changes the payload raises the format version, and reads the previous
version's files in one of two ways. A release that only adds fields `toJson` leaves out
when empty needs nothing more: a previous-version payload lacks them, and they read as
empty. A release that renames, removes or reshapes a field carries a small upgrade from the
previous version to its own, run once when it first reads an older payload and tested like
any other code, before its strict reader. Each reader accepts one shape, and an older shape
reaches it only through a named upgrade. This concerns versions of the `.wardplanning`
format, not the 1.0 `.emergencyplan` files, which are not read at all. The model `fromJson` functions have no way to fail today, so they gain one:
a missing or malformed required field refuses the whole payload rather than reading part of
it.

**When a newer app wrote the version on the server**, this app keeps the local copy it
has and **pushes nothing**, with a persistent notice: *This ward was saved by a newer
version of Ward Planning. Update to see the latest changes and share your work.* **Every
edit stays available** and is journalled and saved locally as usual, so users keep working
through an update; once updated, the app takes the server's version and replays the
journal like any other. The update notice offers the download from the server's latest
release, without the option to dismiss it while this state lasts. The state is checked
again on every head, so it lasts only while the server keeps presenting a version this app
cannot read. The server supports a format before any app writes it.

**The server address lives in the app; the document carries only its id.** A file handed
over on a USB stick still knows which document it is. The app supplies the server, from a
built-in address with an advanced override for testing and self-hosting. Moving the
service to a new domain is therefore an app release, and every existing document follows
without being rewritten. Releases are published on the server itself, so that release
reaches members only while the old address still answers; continuity keeps the domain
(see *If the server goes away*), and a move made after the old address is gone reaches
members as a release they install by hand.

The override is app-wide, and an invitation works only between apps using the same server:
joining looks the document id up on this app's own server, and the host in a pasted join
link is ignored. A link that chose the server could send a new member to a server of an
attacker's making at the one moment its first download is taken as it stands (see *Threat
model*). Testers and self-hosters set the override on every app that shares their wards.

**One id for the document's whole life.** The id names the local file, keys the local
record, and is bound into the header from the first save. The first push registers that
same id with the server, so nothing is renamed, re-keyed, or re-encrypted when the document
reaches it.

**Payload**

The document's JSON, compressed, encrypted with XChaCha20-Poly1305 under the data key, as
**one payload**: replay happens on the model, in a client that holds the data key, so one
payload serves it fully. One payload means one nonce, one version, and
one write. The compression is Qt's `qCompress`: a 4-byte big-endian uncompressed length
followed by a standard zlib stream, so a reader outside Qt drops the four bytes and
inflates the rest.

**Layout.** The decrypted, inflated payload is one JSON object:

```
{
  "ward":   { ... },
  "access": { "users": [ ... ], "invites": [ ... ], "formerKeys": [ ... ] },
  "landed": { "<fingerprint>": <sequence number>, ... },
  "device": { ... }
}
```

`ward` is the ward's content: families, teams, tags, emergency assets, ministering,
emergencies in progress, the PDF dates, and the ward and stake records. `access` is the
access list (see *What the document carries*). `landed` maps each device's fingerprint to
its landed mark (see *The journal*). `device` is this device's own state. A local file and a
version on the server differ in that one part, each read strictly: a local file must carry
`device`, and a version must not, so a version that does is refused like any other invalid
version. Every field of each part - its JSON key, type and encoding - is specified in
`docs/FORMAT.md`, under the same format version as the header, and binary values are
base64url without padding, as everywhere else.

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
its current one (BLAKE2b, 32 bytes, unkeyed, zero salt, personalised `wp-former-key`),
appended at each rotation.
A client takes a version only when its key is one the client already holds for that ward,
or when the version names one of those among its former keys. Keys are held per ward, so
a version of another ward does not descend. Only someone who held an
earlier key can name it, and the hashes travel only inside the encrypted payload, so the
server can neither forge a descendant nor learn them. The list grows by one entry per
rotation, a handful a year.

**Former keys are not content.** A write takes them from the version it builds on and
appends the old key's hash when it rotates, so they are never something a command sets and
never something a user is asked about. The landed marks (see *The journal*) are carried
the same way: every write takes them from the version it builds on, sets only its own
device's entry, and drops the entries of devices its access list no longer holds.

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
anyone else. The one exception is the save after Leave or Remove this device, which keeps
the key of the version pushed from (see *Saving*), since that is the last key this device
can open.

A push that succeeds is followed by a local save of the version it pushed: under the new
key if it rotated, with the new base, and with the journal trimmed by this device's mark.

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
regardless: it adds `libsodium` to `vcpkg.json`, which lists only `libmupdf` today, with
`"version>=": "1.0.22"` and a `builtin-baseline`, under the `x64-windows-static-md` triplet
that MuPDF already uses.

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
| `POST /documents` | anyone | Stores version 1 of a new document under its client-made id. Rate-limited by address. An id that already exists gets its own answer, so a client retrying after a lost reply fetches instead, and its landed mark empties the journal. |
| `GET /documents/{id}/head` | anyone with the id | Version, publisher fingerprint and time, and the header. |
| `GET /documents/{id}` | anyone with the id | The current version: header and blob. Ciphertext; unreadable without a key. |
| `PUT /documents/{id}` | a key holder | Conditional on `Ward-Base-Version`. Signed. |
| the live channel | a key holder | Open while the ward is open. Carries presence and version notices. |
| `GET /releases/latest` | anyone | The current app version per platform, with each installer's download address. |
| `GET /download` | anyone | A web page offering the installers, for people who do not have the app yet. |
| `GET /join` | anyone | The page an invite link opens: *Download*, and *Copy invitation*, which copies the invitation built from the fragment. The server never sees the fragment. |

**Signed requests.** A write carries the signer's two public keys and an Ed25519 signature
over one UTF-8 line:

```
wardplanning-v1 <call> <document id> <base version> <header hash> <payload hash>
```

`<call>` is `create` for a `POST`, `push` for a `PUT`, and `channel` for opening the live
channel. The base version is a decimal integer. The hashes are BLAKE2b-256 of the exact
bytes on either side of the body's first `\n`. **Every binary value on the wire is
base64url without padding, as in the header**: the document id in this line, in URLs and
in the invite link (22 characters), the hashes (43), the keys, the signature, and the
invite seed. A `create` has no base version and signs `0`; `channel` signs `-` for the
fields it lacks, which no version or base64url value can be. No field contains a space, so
the line is unambiguous, simple to build on any server, and readable in a log.

The server computes the signer's fingerprint from the keys exactly as clients do, and
accepts a `PUT` only if the *current* header names that fingerprint. There are no
sessions, no challenge, no registration, no token. The first version of a document is the
exception: anyone may create one, and the `POST` is accepted when the header it carries
names its signer. Opening the live channel carries the same signature, so only key holders
appear as present.

**Requests and answers.** Everything but the body rides in HTTP headers:

| Header | On | Holds |
|---|---|---|
| `Ward-Base-Version` | `PUT` requests | the version the push builds on |
| `Ward-Signer` | `POST` and `PUT` requests | the signer's X25519 public key and Ed25519 verify key, joined by `.` |
| `Ward-Signature` | `POST` and `PUT` requests | the Ed25519 signature of the signed line |
| `Ward-Version` | answers to `head`, `GET`, `POST` and `PUT`, and a `412` | the current version, which after a stored write is that write's |
| `Ward-Publisher` | answers to `head` and `GET` | the fingerprint that published the current version |
| `Ward-Published` | answers to `head` and `GET` | when the current version was published, ISO 8601 in UTC |

| Status | When |
|---|---|
| `201` | a `POST` stored version 1 |
| `200` | a read, or a `PUT` that stored the next version |
| `400` | the body, its framing, its header or a `Ward-` field is malformed, or a `PUT`'s header names a different document |
| `403` | the signature does not verify, or the header the signer must be in does not name them: the posted header for a `POST`, the current one for a `PUT` |
| `404` | no document has the id in the URL |
| `409` | a `POST` for an id that already exists |
| `412` | a `PUT` whose base version is not the current one |
| `429` | the address is over its rate limit |

When more than one applies, the first of `429`, `400`, `404`, `409`, `403`, `412` is
given. A client answered `412` fetches the current version and replays onto it. One
answered `403` fetches the head: a header that no longer names this device is the ward no
longer being this device's, and otherwise the push has failed and goes again with the next.

**Reads are open** to anyone who knows a document id, because the design never relied on
the server to protect content and the id is 128 random bits. A read limit by address stops
scraping. It is set so that every responder of a ward behind one shared address - a
church's or a home's network - can fetch each version as it lands during an emergency.

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

Because a version can replace the document under any nested event loop - a dialog's
`exec()`, a message box - code holds ids and captured values across one, never pointers or
references into the document. A command is built from what was captured when its target
was chosen, and a target that no longer exists when it runs makes it conflict.

**So a connected ward's base is always current**, and that is what makes replay the
exception rather than the path. For a conflict, two users have to change the same thing
while one of them has it open in a dialog, or commit inside the gap between one version
landing on the server and the notice of it arriving at the other.

**It also keeps the cost of a conflict small while users work together.** The journal
empties as each push succeeds, so while connected, losing that race costs a redo of recent
work.
A journal long enough for abandoning it to hurt belongs to a device that has been
disconnected, which is also when nobody else is publishing.

A ward is small enough that sending the whole encrypted blob per edit costs nothing worth
designing around, and edits are rare enough that per-operation encrypted deltas would be a
protocol built for traffic that does not exist. This holds during an emergency too, when
several responders are logging attempts at once, and the design carries no bandwidth
estimate because none would change it.

**A download replaces nothing until it validates.** The key unseals, the payload decrypts
and reads strictly as a version, without `device`, the header matches the access list, the
version is not below the base this device holds,
and the version descends from one this device holds. Any failure keeps the local file
untouched: the local file is the one copy that has to survive a version nobody can read.
The device keeps working on that copy and pushes nothing while the head fails; see the
accepted stall under *Threat model*. A head that leaves this
device out is not a fault: it is the ward no longer being this device's, handled under
*Everything else, from the document*, and a head that names no recipient at all is a
deleted ward (see *Who has access, changed by two users*); both are recognised from the
header, before anything is unsealed.

## The journal

Every command run in a ward this device holds is appended to its journal, and leaves the
journal when this device sees a version on the server that carries it. Content and access
changes go into the same journal in the order they were made, because they are the same
kind of thing: an edit to one encrypted document. **The journal holds this device's
commands that it has not yet seen land on the server**, and the interface shows that
changes are waiting while it holds any.

**The journal lives inside the local file.** The local file's encrypted payload holds what
a pushed version holds - the ward, its access list and the landed marks - plus, under
`device`, the state that belongs to this device alone:

- the journal and its sequence counter
- the to-redo list
- the base version this device holds, which the push condition and the rolled-back check
  use
- the access list as of the last dismissed access notice, which the next notice is a diff
  against
- dismissals of the single-holder and single-admin notices
- the failed list of addresses and unit numbers that did not resolve
- whether the ward is no longer this device's, so a device reopened offline still knows
- the seed of each open invite this device made (see *Invites*)

A pushed version's payload holds the ward, its access list and the landed marks. In memory these are separate, so a push serialises the ward and
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

**The start condition is what the user saw.** An edit captures the values it will change
when its target is chosen: when the panel or dialog opens, or when the user picks a
different target inside it, which is also when it shows that target's current values. The
command is built from those, rather than from the document re-read when the user confirms.
An incoming version may change the same thing while the dialog stands open; the confirm
then conflicts and is handed back, and the other change stands. Every write that follows a
dialog's `exec()` is such an edit.

**A start condition covers exactly what the command changes.** An update command is built
from the difference between what the edit opened with and what it saved, and holds the
before and after of the changed fields only. The fields are typed per command:
`UpdateFamilyCommand` holds an optional address change, an optional coordinates change, and
a change per member keyed by `PersonId`. Each field it holds is judged on its own: one
still at its before value is set, one already at its after value is left, and one at
anything else makes the whole command a conflict that touches nothing. A command whose
fields are all already at their after values is the result-already-holds case. A geocode
therefore collides only with an edit that changed the coordinates, and two users editing
different members of one family both apply.

The same holds one level down. A member change is per field - name, gender, phones, email,
birthday, callings, parent flag, units, special-need note - each judged on its own. A
member added carries the whole new `Person`, and its result holds if that `PersonId` is
already there; a member removed carries the person as last seen, and its result holds if
they are already gone. Members are matched by `PersonId`, and their order is not part of
any start condition.

Every update command takes this shape, not only the family's. Single values - a name, a
leader, a response area - are per field. Sets - a team's members, an asset's people, a
tag's people and families - are held as the ids added and the ids removed: an addition is
done if the id is present, a removal if it is absent. `UpdateTeamCommand`,
`UpdateEmergencyAssetCommand` and `UpdateTagCommand` hold whole objects today, and convert
to this, so two users adding different members to one team both apply.

**References stay whole.** Teams, tags, assets, ministering groups and districts, and task
assignments refer to people, families and teams by id, and replay keeps every such id
pointing at something that exists:

- **An addition's start condition includes that the id it adds exists.** Adding Bob to a
  team, replayed after Bob was removed, conflicts.
- **A removal carries its cleanup and where it saw the removed thing.** Removing a member,
  deleting a family, and an import's removals all use one cleanup routine. It takes each
  removed person out of every team, person-level tag, asset and ministering group that
  holds them, clears any leader or presidency role they held, and unassigns every task
  assigned to them; a removed family it also takes out of every family-level tag and
  ministering group that holds it. Deleting a team unassigns every task assigned to that
  team the same way. The command holds that list of places as the user saw it, and its
  start condition includes it: if what it removes has been added or assigned somewhere
  since, the removal conflicts rather than leave that reference behind. `Document::cleanupPersonReferences`,
  which has no callers today, becomes the person half of this routine.
- **A leader is a member.** A leader change's start condition includes that the new leader
  is on the team.

A removal therefore conflicts with a concurrent membership change or task assignment for
the same person, which is two users disagreeing about that person and is handed back as
such.

A response's family records and contact attempts are the log of what happened during it,
and keep the ids they were made with. A record holds its family's name and address as they
stood when the record was made, so it reads the same after the family is removed; an
attempt whose `who` has been removed since shows no name there. The log leaves the document
when the emergency ends, as its summary report (see *Emergency archives are removed*).

Online the journal is almost always empty: a command enters it and leaves it as the push
succeeds. Offline it grows, and it survives closing the app, which is what makes a week of
disconnected work no different from a second of it.

**The ward records what each device has landed.** Each journal entry takes this device's
next sequence number, from a counter kept with the journal. The ward's payload carries a
map from device fingerprint to the highest sequence number that version includes, and a
push sets this device's entry to the last one it carries. On any fetch, whoever published
it, the device drops the journal entries at or below its own mark and replays the rest.
One rule covers a push that succeeds, a push whose answer never arrived, one followed by
someone else's push before this device hears of it, and the app closed in between. One
push is in flight at a time, so a command made while it is in flight sits above the mark
and goes out with the next.

On taking any version, the device sets its counter to the larger of the counter and its
own mark in that version, so new entries always number above anything that landed, even
after an older local file comes back from a restore of app data. A device that cannot read
its local file, but whose fingerprint the head still names, downloads the current version
as itself and starts with an empty journal; whatever the unreadable journal held is lost.

**Making every write a command is the core of this work.** What runs through
`CommandHistory` today is family update; team add, update, delete and member removal;
emergency-asset add, update, delete and unassign; and the two imports. Ministering changes
only by import. The tag commands exist and convert with the rest, though nothing in the
interface calls them yet: `AddTagCommand`, `UpdateTagCommand` and `DeleteTagCommand` stay,
while `AssignTagToPersonCommand`, `AssignTagToFamilyCommand`,
`UnassignTagFromPersonCommand` and `UnassignTagFromFamilyCommand` are deleted, since each is
an `UpdateTagCommand` holding one id added or removed. `AddFamilyCommand`, `AddTeamMemberCommand` and
`AssignEmergencyAssetToPersonCommand` have no callers - families arrive by import, and the
team and asset dialogs change members through their update commands - so they are deleted.
`RemoveTeamMemberCommand` and `UnassignEmergencyAssetFromPersonCommand` are deleted too:
under the update shape above, removing one member is an `UpdateTeamCommand` or
`UpdateEmergencyAssetCommand` holding that one id removed, and, for a team leader, the
leader change as well, both fixed at construction. `RemoveTeamMemberCommand` today clears
the leader inside `execute` and does not restore it on undo.
The Delete Family action, whose confirmation ends in "Delete not yet implemented" today, is
wired to `DeleteFamilyCommand`, which carries the same cleanup as an import's removal,
worked out at construction. The rest has to join them, because a write that is not a command cannot
replay:

- **Emergency responses become a keyed collection.** `Document` holds one
  `std::optional<EmergencyResponse>` today, and `EmergencyManager` assumes at most one:
  `startEmergency` returns early when one is active, and `isActive()` gates the response
  views throughout the interface. Each response gains an id, the document holds them by it,
  and the interface lists the emergencies in progress and switches between them. Ending
  saves the summary report and then removes the response, as `endEmergency` clears it
  today (see *Emergency archives are removed*).
- **Contact status becomes the outcome of an attempt.** `FamilyResponseRecord` stores one
  `m_contactStatus` today, overwritten by the OK and Unable to Reach buttons. Instead,
  `ContactAttempt` gains a `std::optional<ContactOutcome>` (`OK` or `UnableToReach`), the two
  buttons each record an attempt carrying their outcome, and a family's status is the outcome
  of its latest attempt that has one, or Not Contacted when none does. Latest means the
  greatest `timestamp`, ties broken by `ContactAttemptId`. A command inserts an attempt at
  its place in that order, most recent first, as `CODING_STYLE.md` asks of embedded child
  objects, and a task likewise by `createdAt`, ties broken by `TaskId`; `prepend` does this
  today only because attempts arrive in time order on one device. So the stored order is the
  displayed order whatever order replay lands them in, the same attempts always make an
  equal list, and a family's status is its first attempt that has an outcome. Timestamps
  come from each device's clock, so a wrong clock can show an older outcome as current; every
  attempt stays in the history, and this is accepted. `method` and `who`
  become optional: a button press records only its outcome and time, shown as *Marked OK*
  or *Marked Unable to Reach*, while the contact dialog fills in both. `ContactStatus` and
  `m_contactStatus` are deleted, and `effectiveStatus()` reads the attempts. Needs Help
  still comes first while a family has unresolved tasks, whatever its latest outcome.
- **`EmergencyManager` keeps no copy of its own.** It holds `m_response` and writes it back
  through `persistResponseData`; instead it reads the response from the document, by the id
  of the emergency being shown, and every change goes through a command. Starting an
  emergency is one command that adds the response with a `FamilyResponseRecord` for every
  family, as `startEmergency` does today, keeping names and addresses as they stood at the
  start. `syncFamilies`, which adds records for new families on each document change, is
  deleted: a family added after the start has no record and reads as Not Contacted, and a
  command that writes to a family's record creates it when missing, from the name and address snapshot the command took at
  construction, so it depends on nothing but the response id. Two responders
  touching a new family both land, since each change is an addition. Statistics - the
  progress bar and the report - count the response's records plus every directory family
  without a record as Not Contacted.
- **Everything that lets the user choose where a ward is stored is deleted**; the PDF import
  and report export dialogs stay. Known sites:
  - `DocumentManager`: the `filePath` property and `filePathChanged`, `setFilePath`,
    `openDocument(path)`, `saveDocument`, `saveDocumentAs`, `saveDocumentOnly`,
    `ensureFilePath`, `maybeRenameForWard`, and `setEmergencyResponse`, whose work the
    emergency commands take over
  - `JsonService`'s plaintext `loadDocument` and `saveDocument`; the payload's JSON reuses
    the models' `toJson` and `fromJson`
  - `MainWindow`'s File > Open and Save with their file dialogs, and the "use Save As to
    save manually" hint on a failed save
- **All archive code is deleted** with the archives themselves; the summary report saved
  when an emergency ends is how it is looked back on:
  - `EmergencyManager`: `m_savedResponse`, `loadArchive`, `closeArchive`, `reopenArchive`,
    `saveArchive`, `isViewingArchive`, the `archiveViewOpened` and `archiveViewClosed`
    signals, and `endEmergency`'s `archive` parameter
  - `DocumentManager`'s overlay: `m_archiveDocument`, `setArchiveDocument`,
    `clearArchiveDocument` and `isViewingArchive`
  - `ArchiveBrowserDialog` and `EmergencyBanner::setArchiveName`
  - `MainWindow`'s `onOpenArchive`, `onArchiveViewOpened`, `onArchiveViewClosed`,
    `m_openArchiveAction`, `m_closeArchiveAction`, the Open and Close Archive menu items,
    and the reopen-from-archive path
  - the end dialog's Archive && End and Discard && End buttons and its *Generate summary
    report* checkbox, replaced by one End confirmation that always saves the report first;
    cancelling the report's save dialog ends nothing, in place of today's *Skip Report*
    question
  - the `archiveViewOpened`/`archiveViewClosed` connections in `FamilyTreeModel`,
    `MinisteringModel`, `TeamsTreeModel` and `WardListView`, and the `isViewingArchive`
    checks in `ActionButtonsWidget`, `TeamsView` and `WardListView`
  - `EmergencyResponse::m_endedAt` and its accessors, since no response in the document
    has ended; `ReportGenerator` takes the end time as an argument
- **The emergency response** is written by `EmergencyManager` mutating it in place, outside
  the command path. Its mutations become these commands:

  | Command | Covers | On replay |
  |---|---|---|
  | `StartEmergency` | start, with a record per family | an addition; never conflicts |
  | `EndEmergency` | end, removing the response once its report is saved | conflicts if the response differs from what the report holds; result holds if already gone |
  | `RecordContactAttempt` | logging a contact, and the OK and Unable to Reach buttons | an addition |
  | `DeleteContactAttempt` | removing an attempt | result holds if already gone |
  | `AddTask` | adding a task | an addition |
  | `UpdateTask` | the task dialog, resolve, reopen, assign, unassign, notify | per value, as `UpdateFamilyCommand` |
  | `DeleteTask` | removing a task | result holds if already gone |
  | `AddTaskCategory` | adding a category | result holds if present |

  `UpdateTask` holds typed optional changes, each with its before and after, to four
  values: category, description, assignment (assignee, notes and notification as one
  value), and resolution (resolved, notes and time as one value, the time fixed at
  construction). A change of assignee sets the notification to none, since the new
  assignee has not been told; `assignToTeam` and `assignToPerson` keep the previous
  assignee's notification today.
- **The access list** is new in its entirety. Invite, remove a user, remove a device,
  withdraw an invite, set a role and leave are each a command, which is what lets an access
  change replay like any other. Join is a command too, run once by the joining device under
  the invite's key, and refused with *This invite has been used or withdrawn* when the
  current header no longer names the invite.
- **Every model `fromJson` gains a way to fail**, so a missing or malformed required field
  refuses the whole payload; today each reads a missing field as empty or false.
- **`Command` itself** gains `toJson` and `fromJson` with a type registry, so a journal
  entry reaches disk and comes back as the command it was. `undo` becomes `inverse` on
  every command, and `CommandHistory`'s `markSaved` and `isDirty`, with
  `DocumentManager::isDirty` and its `dirtyChanged` signal, are deleted: every command is
  saved as it is made, so nothing is ever unsaved.
- **The update check** is new: on start, the app reads the server's `GET /releases/latest`
  and, when it is newer than the running version, shows a dismissible notice offering the
  download. Installers are published to the server, which serves them and the download
  page. The app states no version today: `main.cpp` sets no `setApplicationVersion`,
  `CMakeLists.txt`'s `project` declares none, and `1.0.1` lives only in `installer.iss`. The
  version moves to `project(... VERSION x.y.z)`, which reaches the app as its application
  version and is written at configure time into a file `installer.iss` includes in place of
  its own `MyAppVersion`, so it is stated once. Versions compare numerically, part by part,
  so 1.10.0 is newer than 1.9.0.
- **The app is renamed Ward Planning throughout.** The application name (`main.cpp:12`),
  the window title (`MainWindow.cpp:530`), the installer's name and publisher
  (`installer.iss`), the executable, and the CMake project and its targets (`EmergencyPlan`,
  `EmergencyPlanLib`) take the new name, with everything that names them, such as
  `build.bat`'s deploy step. The installer keeps its `AppId`, so the release upgrades the
  old install in place. App data and the tile cache are named by Qt after the application,
  so both move; nothing is carried over from the old directories (see *Earlier versions*),
  and the tile cache refills as maps are viewed.
- **Receiving an invitation** is new: *Join a ward*, which reads the clipboard when clicked
  and otherwise offers a paste box, and the Windows instance lock. `installer.iss` registers
  `.emergencyplan` today, and that association is removed; nothing is registered in its
  place.
- **`docs/FORMAT.md` is new**, written with the code since the models change in this work.
  It specifies every field of the payload, `device` included, and changes in the same
  commit as any `toJson` it describes; a release that changes it raises the format version,
  as under *File format*.
- **`ARCHITECTURE.md`, `IMPORT_DESIGN.md` and `USER_GUIDE.md` change with the code they
  describe.**
  `ARCHITECTURE.md` gives commands `execute` and `undo`, `CommandHistory`'s stacks and an
  `AddFamilyCommand` example; it moves to `execute`, `inverse` and `refusal`, the journal
  and `WardSession`. `IMPORT_DESIGN.md` says a re-import replaces family and person data;
  it states that the special-need note and the coordinates are the app's own and stay.
  `USER_GUIDE.md` describes `.emergencyplan` files, File > Open, Save As and archives; it is
  rewritten for wards, *Who has access* and invites, in the words under *Words the user
  sees*, and carries the statements this design promises it makes: what the server knows,
  and that roles are a convention among honest clients, not a security measure.
- **Edits that re-read on confirm** take their start condition when their target is chosen,
  as above. The sites known today are a starting list, not a complete one: the family edit
  panel (`MainWindow.cpp` builds `UpdateFamilyCommand` from the live document, while the
  panel already holds the copy it opened with); the team dialog in `TeamsView.cpp`; the
  asset dialog in `EmergencyAssetView.cpp`; the special-needs dialogs in `NeedsSubView.cpp`,
  which can switch person; and the notify, resolve and assign-to-team actions in
  `WardListView.cpp`, `TaskListView.cpp` and `TeamsView.cpp`.
- **The special-needs dialog becomes one "Special Need" dialog.** Opened on an existing
  need, its person is locked; opened from empty space, picking a person fills in that
  person's current note, and that is when its start condition is taken. Today it reuses
  `WardListDialog::selectPersonWithName`, which sets the note once at open and never on a
  change of selection (`WardListDialog.cpp:292-317`), so it either gains a
  selection-changed signal from the picker or stops reusing it.

**Automatic updates are the one exception, and stay out of the journal.** Geocoding and
ward and stake lookup produce derived data: an address in, coordinates out. A result is
recomputable by definition and is never a user's work to redo, so putting "set
coordinates for the Smith family" in the list of what this user did would describe
something they did not do and cannot retype - and under one-conflict-abandons-the-replay, a
stale result could throw away work they did do. So an automatic result applies to the
document and pushes like anything else, and if that push loses it is simply not in the
replayed document, and the batch that runs after the replay computes it again (see
*Saving*).

## Replay

**A command replays onto the current version, or it is handed back to the user.** This
is the whole answer to two users working at once, and there is no second mechanism.

**It applies to every change a user makes.** Editing a family, recording a contact attempt,
importing a directory, inviting someone, removing a user, changing a role: all of them are
commands, all of them enter the journal, and all of them replay by the one rule. Who may
read the ward lives in the same encrypted payload as the ward itself, so a change to it is
a change to the document and needs no path of its own. Two writes take a path of their
own, each described where it lives: automatic updates, which are derived data and stay out
of the journal (see *The journal*), and the join, which a device makes under the invite's
key before it has a journal (see *Invites*).

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
not allow it - the only admin cannot step down, a leave made while others remained cannot
land where its user is now the only one (*You are now the only member, so leaving would
delete Tanglewood Ward*), a user cannot
remove their last device (*To stop using this ward, use Leave*, so Leave's rules apply), a member
cannot add users, remove users, set roles or withdraw someone else's invite - and replay
asks those same questions of the document in front of it, because a command made when a
rule allowed it can arrive where the rule does not. The last admin's own role is untouched
in that case, so the start condition matches; what changed is somebody else's entry, which
a start condition should not be reaching into.

A refusal is its own question, asked before `execute`:

```
QString refusal(const Document& document) const;
```

It returns the rule's wording, or an empty string when nothing refuses. It is the same
function the interface asks when deciding whether to offer the action, so each rule lives
in one place. Content commands return an empty string; only the access-list commands carry rules. An admin-only command
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

**Ending an emergency turns it into its report.** Ending saves the summary report where the
user chooses, then runs `EndEmergency`, which removes the response and holds it as the
report was written from. A version that changed the response in the meantime makes the end
conflict, and the coordinator ends it again with a report that includes the change, so the
report of the end that removes a response holds everything in it. Ending a duplicate leaves
its records in its own report. A change replayed onto a response that has ended conflicts,
which is accepted under *Settled*.

**While the network is down, responders cannot see each other's work.** The server relays
nothing it cannot reach, and each device holds its own partial picture until connectivity
returns. Cell data and tethering often survive when home internet does not, and ward
emergency plans already assume radios and runners for coordination. Peer-to-peer sync over
a local network would genuinely close the gap and is a substantial subsystem of its own; it
is named here as possible future work rather than folded in.

Any responder can work any family. Contact attempts, outcomes, tasks and responses are all
additions, so two responders reaching the same family both land. Task edits are the
exception: two users setting the same value of a task differently is a conflict, which is
the right answer for a real disagreement, while changes to different values both apply.

## Imports

An import is one command, and **it follows the same rule as every other: its start
condition covers exactly what it changes.** At construction it works out every change it
will make, each with its before and after:

- **A ward directory import**: the families it adds, updates and removes; for each removed
  family, its cleanup (see *References stay whole*); the directory PDF date; and the ward record, created if missing,
  whose result holds if one is already there.
- **A ministering import**: its organisation's districts and groups, the families it adds
  or updates, and the ministering PDF date. The districts and groups are held as one value,
  the organisation's whole collection before and after, because the parser gives every
  group and district a new id on each import, so a comparison by id would find every one
  removed and added anyway. Only this import creates or rearranges them; the cleanup of a
  removed member also changes them, and a cleanup or another ministering import landing
  in the meantime makes the import conflict. The people and families its groups refer to
  follow *References stay whole*. When the directory is the newer source - the ministering
  PDF is older than the directory's, or has no date once a directory is imported - a family
  or person the import cannot match is not added, and the import leaves them out of the
  groups and districts it builds. `mergeFamilies` points the groups at such an unadded id
  today (`MinisteringImportService.cpp:156-164` and `:330-334`); an older PDF naming someone
  the newer directory does not list is out of date about them.

A family an import updates is held at the same typed level as `UpdateFamilyCommand` -
address, coordinates, and a change per member keyed by `PersonId` - and only for families
that actually differ. The import service builds merged families as it does now, and the
command compares them with the existing ones field by field, holding only what differs.
**An import never changes a person's special-need note, and keeps a family's coordinates
unless its address changed.** `WardDirectoryImportService::preserveIds` copies the existing
person's note onto the rebuilt person and passes the existing coordinates when the address
is unchanged; today it drops the note and takes the PDF's coordinates, so every re-import
wipes special needs. A geocode that lands meanwhile collides only with an import that
changed that family's address or coordinates.

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
device's public keys, because those are what the data key is sealed to. In outline, with
each field's key and encoding in `docs/FORMAT.md`:

```
access:
  users:
    - id: <user id>
      name: "Alice Smith"
      role: admin | member
      devices:
        - { name: "Laptop", publicKey, verifyKey }
        - { name: "Pixel 8", publicKey, verifyKey }
  invites:
    - { for: device | user, maker: <user id>, role and label: "Bob" (user invites only), publicKey, verifyKey }
  formerKeys: [...]
```

The header names one recipient per key: every device, and every open invite. Header and
access list must agree exactly, except in a deleted ward's final version, whose header
names no recipient and which carries no payload. The server sees fingerprints only.

**User ids** are 128 random bits, made when someone creates a ward or joins it through
*Add a user*, so they are per ward and distinct from the directory's `PersonId`, which
names a ward member. A device joining through an *Add a device* invite takes the inviter's
user id, name and role; one joining
through an *Add a user* invite makes a new user with the name it gives. A user invite's
`label` is who the inviter made it for, so open invites can be told apart in the list; it
is not carried onto the user. A device invite has no label and shows as *a device for* its
maker's name: every open device invite a user made joins as one of that user's devices, so
any of them serves any of their devices.

**Roles are a convention among honest clients.** The server cannot read the access list, so it
cannot tell an admin from a member. The rule (admins add users and remove users; everyone
adds and removes their own devices) is what the app offers, and it exists to keep
well-meaning users from over-sharing.

## Who has access, changed by two users

An access change is a command like any other and replays like one. Two admins removing
different users both apply. Two admins removing the same user: the second finds its
intent already satisfied and there is nothing to do. A removal names the user by id and
takes their whole entry, whatever devices it holds by then, since the intent is that this
person is out; a device the user added in the meantime goes with them. Two admins setting one user's role
differently is a conflict, so the second admin's replay is abandoned and they do it again
knowing what the first one did.

Mutual removal needs no rule of its own: if two admins remove each other while offline,
whoever pushes first stands, and the other's write is refused because the current header no
longer names them. They learn the ward is no longer theirs, and their own pending removal
stops mattering. A ward cannot be emptied by two users leaving at once.

Leaving needs the server, since the version without you must be held there before your
copy goes: Leave pushes first and saves after (see *Saving*).

**The last user's Leave deletes the ward.** It is how a throwaway ward is removed. The
confirmation says so: *You are the only member of Tanglewood Ward. Leaving deletes it: it
stops being shared and is removed from this device, and any other device of yours will
offer to remove it too.* **Delete ward** / Cancel. The push is a final version whose
header names no recipient, which also withdraws any open invites, and which carries no
payload to read. A head with no recipients is a deleted ward to every device: this device
removes its copy, and any other device of the same user shows *Tanglewood Ward has been
deleted*, and offers to remove its copy, keeping the copy and its journal until the user
agrees, as with any head that leaves a device out. If the answer to that final push is lost, the next head, naming no one, tells the
device the push landed. The server keeps the empty version until the pruning rule under
*Open points* clears it, so no delete call is needed.

An invite made on one device and a join landing on another are different commands and cross
without troubling anyone.

## Invites

An invite is a one-time bearer key. Its 32-byte seed yields both of its keypairs through
`crypto_kdf_derive_from_key` with context `wpinvite`: subkey 1 is the seed for
`crypto_box_seed_keypair`, and subkey 2 the seed for `crypto_sign_seed_keypair`.

1. **Add a device** or **Add a user** (admins only for the latter) generates an invite
   keypair from a random 32-byte seed, writes a new version whose header seals the data key
   to it, with an `invites` entry saying what it is for, and pushes it. *Add a user* first
   asks who the invite is for, and makes nothing until that is answered; the answer is the
   invite's `label`. Both are offered only while connected, and the link is shown only once
   that version is on the server, so every link a user can copy is one that works.

   The device that made the invite keeps its seed in its device-local state, saved with the
   invite before the push, for as long as the access list it holds names that invite. So
   *Copy link* on the open invite in *Who has access* shows the link again on that device,
   and a push whose reply is lost needs nothing more: once the next fetch shows the invite
   landed, the link is there. The link is:
   `https://<server>/join#<document id>/<seed, base64url>`. It is an ordinary web link, so
   mail and text apps make it clickable, and the ward id and seed ride in the `#` fragment,
   which browsers do not send to the server. What the user copies is a short message:

   > Alice Smith has invited you to Tanglewood Ward in Ward Planning. Open this link to
   > join: https://<server>/join#...

   The page the link opens has two steps: **Download**, for someone who does not have the
   app yet, and **Copy invitation**, which puts `Ward Planning invite: <document id>/<seed>`
   on the clipboard, followed by *then choose Join a ward in Ward Planning* (see
   *Receiving an invitation*). The user sends the message by email, text, or any channel
   they trust with it.
2. **Joining** on a device, once the app has the invitation (see *Receiving an
   invitation*): derive the invite keypair from the seed, fetch the current version, unseal
   the data key with the invite's private key, validate as any download is validated, ask
   the user to confirm by the ward's name, then write the next version: the invite entry becomes a device
   entry, under the inviter's user for a device invite, under a new user for a user
   invite, named by the joiner, signed by the invite's signing key, whose verify key the
   current header names. The device then downloads the current version as itself and writes
   it. Before using an invite, the device reads the head: if it already names this device's
   own fingerprint, an earlier join landed and only its answer was lost, so the device
   opens the ward as itself and leaves the invite alone. A join that loses to a newer
   version fetches it and joins again on top of it.
   From then on the device signs as itself. The ward opens.
3. **Withdrawing** an unused invite is a write without its entry, by whoever made it or by
   an admin. Removing a user, and a user leaving, withdraws the invites they made.

A join and a withdrawal each drop the invite's key from the recipients, so each rotates
the data key; for a join, the joining device generates the new key, as the member it now
is. The seed lives on in whatever message carried the link, and reads are open to anyone
with the document id, so without rotation a link found later would read every version
after it.

The link is the gate. Whoever opens it first gets in, visibly, and can be removed.

An invite link is about 100 characters: fine to click or paste, long to type. A QR code
can show it for a camera later if a phone client ever wants one.

## Everything else, from the document

- **Who has access**: the access list, read from the open ward. Add a device, Add a user,
  Remove, Make admin / Make member, Withdraw invite, Copy link (open invites this device
  made), Leave, Remove device (own devices), Details.
- **Access notices**: the client diffs the access list between the version it last dismissed and
  the current one, one line per entry that differs: joined as member or admin, no longer
  has access, now an admin or a member, a device added or removed, an invite made or used.
  The lines name whose entry changed, never who changed it; this device's own edits are news
  like any other.
- **The ward is no longer yours**: a head whose header lacks this device. The user is told
  *You left this ward* when this device pushed the leave itself, and *You no longer have
  access to this ward* otherwise; a removed device cannot read the versions that followed,
  so it names no one. The ward stops pushing until a head names this device again, and the
  app offers to delete the copy. The journal is kept with the copy and replays if a later
  head names this device.
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

**Automatic updates run on any device that holds the ward.** Geocoding and ward and stake
lookup change families and ward details on their own. They run after an import, when a
ward opens, whenever this device takes a version from the server by fast-forward or
replay, and after a committed command that clears coordinates, which the command knows at
construction: `startBatchGeocoding` and `checkForIncompleteWards` each skip what is already
done and what is on the failed list,
so a result lost with a push is simply computed again. Several online devices may compute
the same result at once; the results are identical, and the duplicate work is accepted.
Their results are not journalled, for the reason given above, so while the server holds a
version this app is too old to read they apply locally, drop at the replay that follows the
update, and are computed again.

**Coordinates always belong to the current address.** An edit that changes a family's
address clears its coordinates in the same command, unless that edit also set coordinates
by hand, so "has coordinates" means "has coordinates for this address" on every device.
The geocoder therefore queues exactly the families with an address and no coordinates, and
`BackgroundGeocodingService`'s `m_familyAddresses` and its change detection are deleted. A
geocode result carries the address it was computed for and applies only if the family
still has that address and no coordinates; otherwise it is dropped.

**An automatic batch pushes once.** Results apply in memory as they arrive, and the save
and push happen once, when the batch finishes (`onGeocodingFinished`, which already saves
once today) or when a lookup completes, so an import costs one more version rather than
one per family. Addresses and unit numbers that fail to resolve go on a failed list, kept
by their text in the device-local state, and are not tried again until the text changes. A
family whose address is on it stays unmapped until someone corrects the address or types
its latitude and longitude in the family edit panel. The `documentChanged` hook in
`BackgroundGeocodingService` is replaced by these explicit triggers.

**All writes go through `QSaveFile`**, so a crash mid-write leaves the previous file intact.

**The push follows the save.** If the save fails, memory and the in-memory journal keep
the change, nothing is pushed, and the user is told the reason. The next save that
succeeds, once the problem is fixed, writes the document and journal together with the
change in them, and pushes as usual; one write still carries both, so the local file never
disagrees with itself. Every push carries only what this device has already saved, except
Leave and Remove this device, which push first (below), and memory runs ahead of the file
only while saves are failing. While they are, the app tries again on every command, when
its window regains focus, and when it is closed; if that last attempt fails too, closing
asks first: *Your latest changes could not be saved and will be lost. Close anyway?* A
crash or power loss while saves are failing loses the unsaved changes.

**Leave and Remove this device push first and save after.** They are the two commands that
take this device out of the access list, and a local save seals to exactly that list, so
saving first would write a file this device cannot open. They are offered only while
connected, run through the ordinary replay, and push before anything is saved. A push that
fails changes nothing on this device; one that succeeds puts the device in the
no-longer-yours state, with its copy kept until the user deletes it. The save after that
push writes the version the device pushed from, not the one it pushed: the document as it
stood before the leave, under the access list and key it already carries and so still
sealed to this device, with the no-longer-yours flag set and the journal emptied by the
landed mark.

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

The undo stack is in memory and belongs to the session. It is cleared when the document is
closed, opened or switched, and its redo side by any new command, as today. It holds at
most 100 entries with the oldest dropped, which is new: `CommandHistory` has no cap today.

- **An undo whose inverse conflicts or is refused** changes nothing, so the original stays
  on the undo stack and the user is told why, as with any command that fails.
- **A command that conflicts or is refused on its first run** never enters the undo stack.
- **Access-list commands are not undoable**, and never enter the undo stack: undoing a
  removal would re-seal to someone just removed, and a leave or a join cannot be undone
  from inside the ward. Undo is for content.
- **`StartEmergency` and `AddTaskCategory` are not undoable either**: undoing a start would
  delete a response other responders may already be writing to, and an unused category does
  no harm. Undoing `EndEmergency` adds the response back as it was removed; the report
  already saved stays where it is.
- **An abandoned replay clears the undo stack**, whose entries then describe work that did
  not land; the to-redo list covers it.

## Where documents live

The app owns the file. Documents are stored in the local app data directory, named by
document id, and **the user never chooses or sees a path in the normal flow.** "Open" is a
list of the wards this device has access to, not a file browser. This is the largest of the
ease-of-setup simplifications.

App data, wherever this design says it, is `QStandardPaths::AppLocalDataLocation`, which
on Windows is under the user's local, non-roaming AppData. The ward files, the device keys
and the instance lock all live there, so a roaming profile never carries ward files to
another machine without the keys that open them.

**The list is local**, built from the wards this device holds. The server keeps no index of
who holds what, so the list is this device's own record and nothing restores it from the
server.

A new document is created in app data under a fresh id, and naming it is a display concern.
Its first push is `POST /documents`, made as soon as the network allows, and its journal
empties then like any other.

**Show in folder** reveals the app data directory, where each ward is a `.wardplanning` file
in the documented format, for support and for getting the data out if the server goes
away. Backup is the local file on every member's device and the server's copy. The app
writes no other copies: anything it exported would be sealed to the ward's devices, each of
which already holds the ward.

## Receiving an invitation

**Join a ward reads the clipboard when the user clicks it.** The button is prominent on
the first-run screen, where a new user with no wards lands, and is in the ward list too.
If the clipboard holds text beginning `Ward Planning invite:`, or a full join link, the
app fetches the ward, unseals it with the invite's key, and asks *Join Tanglewood Ward?*
with the ward's own name; confirming runs the join. Otherwise the same dialog shows a
paste box: *Copy the invitation from the page your invite link opened, then paste it
here*, which accepts either form. The app reads the clipboard only on that click. This
works right after a fresh install, and needs no link scheme, no registration with the
system, and nothing handed from one process to another.

**One instance per user.** macOS, iOS and Android run one instance of an app themselves.
On Windows the app holds a per-user `QLockFile` in app data while it runs, with
`setStaleLockTime(0)` so that only a dead process, not age, makes the lock stale; a second
launch that cannot take the lock says *Ward Planning is already running* and exits. This
keeps two processes from writing the same local file and journal.

**For the phone ports**, verified `https` links - Android App Links and iOS Universal
Links, with their verification files served by the server - would let tapping the invite
link open the app directly. This is an option for then, not part of this design.

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

**The summary report is the record of an emergency.** Ending saves it and then removes the
response from the document, so the document holds only emergencies in progress, and
nothing in it describes a past one through ids that may since name nothing. The report is a
plaintext PDF written wherever the user chooses, which is worth saying in the same breath.

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
   only an implementation: the header, sealing and wire in this design, and every payload
   field in `docs/FORMAT.md`. This is what makes "someone could adapt it" true rather than
   aspirational.
3. **Every member holds a complete, readable copy**, a `.wardplanning` file in app data that
   *Show in folder* reveals.

**A successor restores the service rather than members detaching from it.** Because
documents carry only their id and the app supplies the server address, continuity is an
operator task that members never see: at the same domain, the successor deploys the
open-source server from a current export of blobs and apps reconnect unchanged (an export
older than what members hold is the accepted stall under *Threat model*); at a new domain,
the successor also ships an app release pointing at the new address, and every existing
document follows it. Releases and the update check live on the server, so that release
reaches members automatically only if it is published while the old address still
answers; this is why the handover keeps the domain.

Members keep editing their local copies throughout, and sharing resumes once the service
returns.

## Failure paths

| Condition | Message |
|---|---|
| A command could not replay | Your changes could not be applied, because the ward changed first. These need doing again: *the to-redo list*. The one that clashed: *its description*, and what that now reads. |
| A removal among them | ... and *name* still has access until you remove them again. |
| A rule refused a replayed command | The wording the app gives for that operation, as it would if the user tried it now: *You are the only admin...* |
| The local save failed | Your change could not be saved: *reason*. It is kept while the app stays open and saved as soon as saving works again; closing asks first. |
| The server is unreachable | Working offline. Your changes are saved here and will go up when the connection returns. |
| Head is older than this copy | The server returned an older version of this ward. Nothing was changed. Your copy is safe and you can keep working; sharing resumes when the server has the latest version. |
| Head fails validation | The ward's latest version on the server could not be read. Your copy is safe and you can keep working; sharing resumes when the server has a readable version. |
| Header format is newer | This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work. |
| Invite link already used or withdrawn | This invite has been used or withdrawn. Ask the sender. |
| Invite link for a ward the server does not have | The server has no such ward. Check the link. |
| An invitation found while offline | Connect to the internet to join a ward, then copy the invitation again. |
| Leave or Remove this device while offline | Connect to the internet to leave this ward. / Connect to the internet to remove this device. |
| Add a user or Add a device while offline | Connect to the internet to invite someone. |
| The invite's push loses to a conflict | Your invite could not be created, because the ward changed first. Try again. |
| Not an admin | In *Who has access*, a member sees *To add or remove users, ask an admin: names* in place of the Add a User button. |
| Head lacks this device | You left this ward. / You no longer have access to this ward. Either with an offer to delete this copy. |
| Head names no recipient | Tanglewood Ward has been deleted, with an offer to remove this copy. |
| A leave lands where its user is now the only one | You are now the only member, so leaving would delete Tanglewood Ward. |

## Debugging

The document's plain JSON (`Document::toJson`) serves a developer-only "export decrypted
JSON" action, off by default, which warns that it writes unencrypted member data.

## Earlier versions

Ward Planning reads only its own `.wardplanning` documents and its own settings. Files and
settings from earlier releases are not migrated; an `.emergencyplan` file is treated like
any other file the app does not own. A ward starts from the directory and ministering PDFs;
teams, tags, assets, special needs and notes in a 1.0 `.emergencyplan` file are not carried
over.

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
starts here: one Qt Test executable linked against `WardPlanningLib`, with `tests/services`
for format, crypto, journal and replay, and `tests/server` for the fake server and the
conformance suite. The instance lock needs two processes, so `tests/lock` is a second,
small executable that starts copies of itself, each taking the lock through the app's own
lock function.

- **The server interface** is one abstract class in the client with the calls the app
  makes: create, head, fetch, push, open channel, and latest release. `HttpServer` implements it for the
  app; `FakeServer` implements it in process for tests, holding versions in memory and able
  to drop a reply after storing a write, delay a notice, or refuse to answer.
- **A test client** is a full client - its own identity, app data directory, document,
  journal and undo stack - so a multi-user test is two or three of them against one
  `FakeServer`, driven step by step with no timers. `DocumentManager` and
  `EmergencyManager` assert a single instance today, and `BackgroundGeocodingService` binds
  to `DocumentManager::instance()`, so the document, journal, sync and replay move into a
  `WardSession` class with no global state, which a test creates as many of as it needs;
  the singletons become thin wrappers that hold the app's one session for the interface.
- **Each registered command type has a sample.** Each type registered for `fromJson` also
  registers a sample instance with a document for each outcome the type can give: one where
  its start condition holds, and, where the type has them, one where its result already
  holds and one where it conflicts. The round-trip test iterates the registry.
- **Every outside dependency has a fake in `WardSession`**: the server, the geocoder, the
  ward and stake lookup, and the clock that stamps attempts and resolutions. A test sets
  each, so nothing reaches the network and timestamp ordering is decided by the test. The
  models read the system clock themselves today, in `TaskNotification::create`,
  `ContactAttempt::create`, `ResponseTask::create`, `ResponseTask::resolve` and
  `EmergencyResponse::create`; each takes the time as an argument instead, which a command
  reads from `WardSession`'s clock at construction, and the report's end time comes from
  the same clock.

Format and crypto, in `tests/services`:

- `DocumentCrypto` round-trip
- two consecutive saves of an unchanged document produce different nonces
- mutating any header byte causes decryption to fail, including the document id
- the known-answer vectors for the fingerprint, the invite keypairs derived from a fixed
  seed, and the signed line for each of `create`, `push` and `channel`
- a fixed local file holding a value in every field of every part, written by the app, is
  read back by a reader written only from this section and `docs/FORMAT.md` - plain JSON,
  base64url, zlib and libsodium, without Qt - which parses every field strictly and
  recovers each of those values, so a `toJson` change that `docs/FORMAT.md` lacks fails it
- a header with an unknown format version is refused whole; nothing pushes while every
  edit journals and saves locally; the state clears as soon as the server again serves a
  version the app understands, and the journal then replays
- a header or payload with a missing or malformed required field is refused, never read in
  part; a payload written by the previous format version reads correctly, through its
  upgrade where the release has one
- a version whose header lists a key the access list lacks, or whose access list has a key with no
  header entry, is refused
- a head whose version is below this copy's is refused and changes nothing
- a removal rotates the key, after which the removed user's devices cannot unseal; a write
  that removes nobody keeps the key; the written header has exactly the expected entries,
  each matching an access-list key
- a device the server mentions but this client did not admit is never sealed to
- a version under a key this device never held for the ward, naming none of its keys
  among its former keys, is refused and changes nothing
- a version of another ward this device holds, served under this ward's id, is refused and
  changes nothing
- after a join, and after a withdrawal, the invite's seed can no longer unseal the current
  version
- the invitation reader, which *Join a ward* gives the clipboard's text or the paste box's,
  reads a copied invitation and a full join link to the same document id and seed, gives
  the same result for a join link on another host, and finds no invitation in other text

The journal and replay, in `tests/services`:

- every registered command type's sample round-trips through the journal, judged by
  behaviour: on each of the sample's documents, the original and the rebuilt command give
  the outcome that document was registered for and leave equal documents, the document
  unchanged where the result already holds or the command conflicts; both give equal
  descriptions; and the journal survives closing the app
- an emergency response mutation and an access-list change each enter the journal, replay,
  and reach the server by the same path a directory edit does
- an automatic geocode result applies and pushes, and when that push loses it is absent
  from the replayed document, the journal is untouched by it, and the geocode then runs
  again and the coordinates come back
- a family edit opened, then a remote version changing that family applied, then the edit
  confirmed: the edit conflicts, the remote change is still there, and the journal is empty
- a geocode lands between an offline edit to a family's phone and its replay: the edit
  applies, and the family holds both the new phone and the new coordinates
- an address edit clears the family's coordinates on every device that takes it, and the
  geocode for the new address then fills them, on the editing device as well; a geocode
  computed for the old address that arrives afterwards is dropped
- geocoding the families of an import pushes one version for the whole batch, and an
  address that fails to resolve is not tried again - on later versions, after closing and
  reopening the app, or by a command that clears other coordinates - until its text changes
- a command that changes no address starts no geocoding
- two offline edits to different members of one family both apply
- a special-need note edit and a phone edit to the same person, offline on two devices,
  both apply
- two users offline each add a different member to the same team, and both members are
  on the team afterwards
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
- every item of the device-local state round-trips through the local file, and a pushed
  version's payload carries none of it; a version from the server carrying `device` is
  refused and changes nothing
- the only user's Leave asks before deleting, pushes a version naming no recipient that
  withdraws any open invites, and removes this device's copy; the pushed body is exactly a
  header with `recipients` empty and its `\n`, and afterwards a `PUT` from any former
  device gets `403` and a `POST` of that id `409`; the user's second device
  shows the ward as deleted and keeps its copy and journal until the user agrees; an
  invitee's later join is refused as withdrawn; with the answer to that final push dropped,
  the next head, naming no one, removes this device's copy without asking again
- Add a user and Add a device are not offered offline, and an invite's link is shown only
  after the version naming it is on the server; an invite whose push loses shows no link
- an invite whose push is stored but whose reply is dropped, with the app closed before the
  next fetch, offers *Copy link* after that fetch, and the link joins; once the invite is
  used, withdrawn, or abandoned in a replay, the seed is gone from the local file and no
  link is offered; *Add a user* with no answer to who it is for makes no invite
- Leave and Remove this device are not offered offline; on a push that loses, the local
  file is unchanged and still opens on this device; on a push that succeeds, the local file
  still opens on this device, holding the version pushed from, marked no longer this
  device's, and with every journal entry the push carried gone from the journal
- a save that fails leaves the local file as it was and nothing pushed, with the change
  still in memory; once saving works again, regaining focus saves and pushes it with no
  further command; closing while saves still fail asks before discarding it
- two responders working offline: every attempt from both sides lands, asserted by exact
  attempt counts per family and each attempt's notes
- two responders offline each record an attempt on a family that had no record: both land,
  in one record holding both attempts
- two responders offline mark the same family OK and Unable to Reach: both attempts land,
  neither journal is abandoned, and the family's status is the outcome with the later
  timestamp, whichever responder reconnects first; the two reconnect orders leave equal
  documents, with the attempts newest first
- two responders who each start a response offline produce two events, each holding its own
  responder's attempts, and neither journal is abandoned
- ending a response saves a report holding every one of its attempts and tasks, then removes
  the response; a responder offline records attempts in it and reconnects after the end:
  their journal is abandoned to the to-redo list, which holds each of those attempts
- an end replayed onto a version where another responder added an attempt to that response
  conflicts, and the response is still there with that attempt
- undoing an end puts the response back equal to what was removed, and the report file is
  left in place
- an end whose report cannot be written leaves the response in progress and adds nothing
  to the journal
- an attempt on a family with no record replays onto a version where an import removed that
  family, and lands with the snapshot it took at construction
- an import replayed onto a version where someone else added a family applies, and that
  family is still there afterwards, asserted by its id; an import replayed onto a version
  where someone else edited a family the import changes is a conflict
- undoing a directory import that removed a family restores its people's team, tag, asset
  and ministering memberships
- re-importing the same directory keeps every special-need note and every family's
  coordinates, and holds no change for families the PDF leaves as they were
- a ministering import from a PDF older than the directory, naming one family and one member
  of a matched family that the directory does not list, applies on its first run, adds
  neither, and leaves no group or district holding an id the document lacks
- a ministering import replayed after a member removal's cleanup changed a group conflicts,
  and the group keeps the cleanup's result
- deleting a family in a family-level tag and a ministering group's families, whose people
  are in teams, person-level tags, assets, ministering and task assignments, removes the
  family and its people from every one of them, and undoing the delete restores each;
  removing one member through the edit panel does the same for that person
- deleting a team unassigns the tasks assigned to it, and undoing the delete assigns them
  again
- an offline addition of a person to a team, replayed after that person was removed,
  conflicts; an offline removal of a person, replayed after someone added them to another
  team, conflicts, and so does one replayed after someone assigned them a task; in each, no
  team or task is left holding a missing id
- a local save that removed someone still accepts the pre-removal version from the
  server, and the journal replays onto it: the save did not rotate, so the one key this
  device trusts is the one the server's version is under
- a push that carries an access change rotates, and the version after it names the old
  key among its former keys
- replaying a command does not change the command: the same journal entry replayed onto a
  moved version and then onto an unmoved one conflicts and then applies, which fails if
  anything captured its start condition while running
- an access-list command replays: two admins removing different users both apply; removing the
  same user twice applies in full; two different roles for one user is a conflict; a
  removal replayed after that user added a device applies and takes the new device too; a
  user who leaves has their open invites withdrawn
- the last-admin rule, and the rule that a leave made while others remained cannot land
  where its user is now the only one, refuse the command that would break them, both when
  the user tries it and when it replays onto a version where the rule has come to apply:
  the ward is left with an admin, two users leaving at once leave one of them in it, and
  the message is the rule's rather than a conflict's;
  removing a user's last device is refused with *To stop using this ward, use Leave*
- a removal made offline by an admin who is demoted before it replays is refused with the
  member's wording, and the user it named is still there

Against the fake server, in `tests/server`:

- pushing from the current base version succeeds and advances it
- pushing from a stale base version is refused, and the journal replays on the fetched
  version and pushes again
- a write whose signature is not from a device the current header names is refused
- each status in the status table is given for a request built to meet only its condition,
  with the `Ward-` headers the header table lists; a stale `PUT` from a signer the current
  header no longer names gets `403`, and a malformed `PUT` to an unknown id gets `400`
- a `403` whose head no longer names the device puts it in the no-longer-yours state; a
  `403` whose head still names it leaves the journal as it was, and the next push carries
  it
- a `PUT` to a document id the server does not hold is refused, and every other document
  keeps its version and contents
- a `POST` whose answer is lost, retried, is told the id already exists; the client then
  fetches, and its landed mark empties the journal
- a downloaded version that fails validation replaces nothing
- edits made while unreachable are kept in the journal and replay on reconnect
- a device given back an older local file, its counter behind the ward's mark for it, makes
  a new edit after its next fetch, and that edit reaches the server
- a device whose local file cannot be read, still named by the head, opens the ward with
  the current version and an empty journal
- a push the server stores but whose reply never arrives: on the next fetch the entries it
  carried leave the journal by the device's mark, without a replay or a conflict, and
  nothing is applied twice, asserted by exact counts of the added elements (contact
  attempts, tasks, responses) and an unchanged access list; repeated with a journal that
  sets one field twice, with the app closed before the next sync, and with another device
  pushing on top of the landed version, changing one of the same fields, before this device
  hears of it
- a command made while a push is in flight stays in the journal when that push succeeds,
  and goes out with the next push
- a server ahead with an empty journal fast-forwards without a prompt
- a device whose head lacks it stops pushing, keeps its file and journal until the user
  agrees, and says it no longer has access, both mid-session and on next open; when a later
  head names it again, it resumes syncing and its journal replays
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

The instance lock, in `tests/lock`:

- while one process holds the lock, a second is refused it; once the holder is killed, a
  new process takes it

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
- **Losing access stops sharing; deleting the copy is the user's choice.** Files taken out
  of app data, and devices that never reconnect, keep what they had either way.
- **Roles are a convention.** A modified client could act as an admin, and already holds the
  data.
- **One admin can remove anyone** from the ward's future. Every removal is announced, and
  everyone keeps the copy they hold; recovery is a new ward built from the directory PDFs.
- **A stolen, unlocked device acts as its owner**, with its owner's role. Its owner
  removes it and any device it added; otherwise any admin removes its owner.
- **A device's first download is taken as it stands.** A newly joined device, or one whose
  app data was cleared, has no earlier version to check the first against.
- **An intercepted invite link gets in**, visibly, and is removed.
- **A channel-open signature replays**, so presence can be lied about.
- **Joining needs the server**, so users are added before an emergency rather than during
  one.
- **Reports stay plaintext.** `ReportGenerator` writes member-data PDFs to a user-chosen
  path.
- **Emergency archives are replaced by the summary report** saved when an emergency ends;
  the response itself leaves the document.
- **Sharing and live editing depend on the server being up.** Editing does not.
- **A hostile server operator could keep members on separate or stale copies** without them
  noticing; nothing is exposed, and each member keeps their own work.
- **An unreadable or restored head stalls sharing** until the operator repairs it. A version
  every client refuses, from a client bug or storage corruption, and a restore older than
  what members hold both stop pushes; every member keeps working on their own copy.
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
- **Recovery from a member's local file.** Recovery is a new ward rebuilt from the PDFs. A
  member's own local file could be published as the new ward instead, keeping notes,
  ministering and emergency data the PDFs lack: one "Publish as a new ward" action, which
  creates a document under a fresh id with no former keys or landed marks, and removes users
  from its access list as it goes, so the device that caused the trouble is not carried
  over.
