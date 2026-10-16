# The Shared Ward

A ward is an encrypted document that carries its own access list. The users holding one
open see each other, and see each other's work as it happens.

This is the whole design: what the file is, who can read it, how it is shared, and what
the server does.

**This is a design document, not an implementation plan.** It says what the system does and
why. Which code changes, where, and what the code does today belong to the implementation
plan, and a review of this document judges the design.

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
list.** Sharing is: send someone an invitation, they install and open it. No provider account,
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
| The split of responsibility | **The ward says who may read it; the server stores versions and relays changes.** Besides the version condition, the server checks only that a write's header is well formed and names its document, and that its signer is named by the current header, or for a new document by its own. |
| Who has access | The access list inside the encrypted payload: users, their devices, and open invites |
| Key model | Envelope encryption, one data key per document, sealed to each device |
| Key generation | Silent, on first run. No user-visible key material. |
| Server authentication | None. Each write and each channel carries an Ed25519 signature the current header admits, or for a new document its own header. |
| Joining | A one-time invitation file, opened in the app |
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

- **Code sites and what the code does today are not in this document**; they belong to the
  implementation plan.
- **Replay, never reconciliation** of two finished versions.
- **Emergency responses are a keyed collection**; two offline starts are two events.
- **A family with no address cannot be placed on the map**: coordinates belong to addresses
  (see *Saving*), so the coordinate fields are disabled for it.
- **Ending an emergency saves its report and removes the response.** A change replayed onto
  a response that has ended conflicts, which abandons that responder's journal to the
  to-redo list. It takes a responder whose work in that response has not landed when the
  end does; one working only in a response they started offline is unaffected.
- **No base copy, no save counter, no debounce.**
- **No exported copies.** A copy would be sealed to the ward's devices, each of which already
  holds the ward; *Show in folder* reveals the ward's own file. The developer-only export
  under *Debugging*, off by default, is the one exception.
- **1.0 `.emergencyplan` files are not migrated**, and their teams, tags, assets, special
  needs and notes are not carried over.
- **An unreadable or restored head stalls sharing** until the operator repairs it (see
  *Threat model*); there is no client-side republish. A ward this device has already had
  from or given to the server, which the server now answers `404` for - deleted and since
  pruned, pruned while in use, or lost - stalls the same way, with an offer to delete the
  copy; the device cannot tell which it was. A ward not yet on the server makes its first
  push as usual.
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
- **Directory-import matching is settled during implementation**, against real
  directories (see *Imports*).
- **A directory's date is its PDF file's modification time**, the only date a directory
  PDF carries.
- **Two directory imports can add one new family or member twice** when both list it and
  neither has landed when the other is built. An import gives each family and member it
  cannot match a fresh id, so the one that lands second finds none of its additions already
  there and adds them. The duplicate stays until a user removes it, or until the next
  directory import that is the newer source: that import matches one copy and removes the
  other, with anything attached only to it.

### Words the user sees

The app is for casual users, so what it shows is plain words, never the design's own terms.
This document uses its terms for precision; the interface uses these:

| In this design | In the app |
|---|---|
| document | ward |
| access list | *Who has access* |
| user | user, or the user's name |
| device | device |
| invite (user / device) | *Add a user* / *Add a device*, and the invitation and its message |
| remove a user | *Remove* |
| remove a device | *Remove device*; *Remove this device* for your own |
| leave | *Leave* |
| withdraw an invite | *Withdraw invite* |
| role | *admin* / *member*; *Make admin* / *Make member* |
| conflict, abandoned replay | *Your changes could not be applied, because the ward changed first* |
| to-redo list | *These need doing again* |
| journal, unpushed changes | *Your changes are saved here and will go up when the connection returns* |
| no longer this device's | *You no longer have access to this ward* / *You left this ward* / *You removed this device from this ward* |

Terms with no place in the interface at all: push, fetch, replay, version (of a ward; the
app's own version is an everyday word), head, base,
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
so once a device holds a version, the server cannot hand it a ward of the server's own
making; its first download is the accepted exception below.

**Defended against.** The server operator reading member data; the operator is the
application's author, and the design keeps that from being a matter of trust. Anyone who
obtains a file without ever having held a key. A removed user writing anything, which
the server refuses because the current header no longer names them. A user removed by
an admin reading a later version obtained some other way, which is under a key they never
held.
Silent corruption or truncation.

**No server answer destroys anything the user has not chosen to give up.** A head that
leaves this device out stops this copy pushing until a head names this device again, and
tells the user they no longer have access; the device keeps reading the head while the ward
is open, and resumes syncing if a later one includes it. The copy itself stays until they
remove it. Deleting a local copy is tidiness, not enforcement, so it is the user's action
rather than the server's; files copied out of app data, and devices that never reconnect,
keep what they had either way. The one copy a server answer removes is the one whose user chose
*Delete ward*: a head naming no one confirms that push landed when its reply was lost, and
a hostile server confirming it falsely leaves on the server only a ward the user chose to
give up. A hostile operator, or a bug, can therefore cut a member off from sharing, which
it could do anyway by refusing to serve, and can destroy nothing the user has kept.

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

**Accepted: an intercepted invitation gets in.** Whoever opens the invitation first gets in,
under the name they give. Interception of a device invite puts the interceptor in as one of
the inviter's devices, so the inviter sees a device they did not add and removes it, which
rotates the key. Interception of a user invite shows up in the access list as a user the
inviter did not expect, or under the expected name if the interceptor guessed it; either
way the real recipient finds their invitation already used and says so. In both cases the entry
is visible and removable, which is the accepted answer at this threat level.

**Accepted: a device's first download is taken as it stands.** A device holding no
readable version of a ward - newly joined, after its app data was cleared, or with a local
file it cannot read - has nothing for the first version to descend from, so a hostile operator could hand it a ward of its own
making at that moment. What the device then writes into it would be readable by the
operator. Every later version must descend from the one it took. A device with an
unreadable file downloads rather than waiting to be invited again, so a corrupted file
does not lock its user out of their own ward.

**Accepted: a departing device knows the next key.** Leave and *Remove this device* are
the departing device's own push, and that push rotates, so that device generates the new
key; a modified client could keep it and read later versions. Both are voluntary, the
device already held everything, and the next removal rotates again.

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
on it, and nothing in those copies is lost. A ward the server no longer has - deleted and
pruned, pruned while in use, or lost - stalls the same way, with an offer to delete the
copy. Repairing the server is the server's own design, outside this one.

**Cleaned up when the user asks.** A removed member's local copy: their app offers to
delete it once it learns the ward is no longer theirs.

**Outside the app's reach.** Summary reports, which remain
plaintext PDFs.

**Known to the server, and accepted.** Per document: the ciphertext and its size, which
leaks approximate ward size; the fingerprints in each version's header, which count the
devices with access without naming anyone; which fingerprint published when, and from
what address; which fingerprints hold a ward open at the same time; and, because a device
has one identity, which wards share a device. The server learns
no names, no roles, and no history of who was added or removed. The user guide says so
plainly.

**Known to anyone holding a document id, and accepted.** Reads are open, so a removed
member, or anyone who has held an invitation, can read what a head and a version show
without a key: the fingerprints, the current version's publisher and time, and the
ciphertext's size. That is a count of devices and a few timestamps, naming no one, and it
is accepted at this threat level.

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
and a fresh `nonce`, as on any write, so a header has one shape and one parse; its body
ends at the header's `\n` with nothing after it, and the signed line's payload hash is
BLAKE2b-256 of zero bytes. Nothing can be written after it: its header names no signer, so
every `PUT` is refused, and a `POST` of the same id finds it already exists. A reader recognises
it by the empty `recipients`, before any payload rule applies (see *Who has access, changed
by two users*). No local file takes this form: the deleting device removes its copy, and
every other device keeps the copy it had.

The whole header is the associated data of the payload's AEAD, so anyone holding the data
key detects any change to it on decrypt. **The associated data is the header line's exact
stored bytes, without its `\n`, never a re-encoding.** A writer builds the header bytes once, nonce and
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

**One thing no format version changes: a file opens with a one-line JSON header whose
`format` key is an integer.** A reader reads it first. A value above every version it knows is a newer format, which it reports as
such (see *When a newer app wrote the version on the server* below); a version it knows is
read by that version's reader, through its upgrade where it has one; anything else it
cannot read fails validation. Keeping that one key fixed is what lets a reader tell a newer
app's file from a broken one.

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
format, not the 1.0 `.emergencyplan` files, which are not read at all. A missing or
malformed required field refuses the whole payload rather than reading part of it.

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
joining looks the document id up on this app's own server. An invitation carries no server,
so none can send a new member to a server of an attacker's making at the one moment its
first download is taken as it stands (see *Threat model*). Testers and self-hosters set the override on every app that shares their wards.

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
emergencies in progress, the PDF dates, the ward and stake records, and the table of
address coordinates. `access` is the
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
every recipient entry's fingerprint is computed from exactly one device's or invite's pair
of public keys in the access list, and every device and open invite in the access list has
exactly one entry. A version that fails
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
anyone else. The one exception is a device that has left: it keeps the key of the version
it left from (see *Saving*), since that is the last key it can open.

A local save always writes the current document, with this device's state beside it. A
push that succeeds changes that state - the new base, the new key if it rotated, and the
journal trimmed by this device's mark - and is saved as any change is, so a command made
while the push was in flight stays in the file and the journal. A crash before that save
leaves the file under the key the push replaced, which is the lost-reply case and needs
nothing more: the next push meets a newer base, the fetch's landed mark trims the journal,
and the newer version names that key among its former keys.

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

**Library pin.** The facts in this paragraph were looked up on 2026-10-15. CVE-2025-69277
(`crypto_core_ed25519_is_valid_point` accepting points outside the main subgroup; fixed by
`ad3004e`, released in 1.0.20-stable and 1.0.21) does not affect any call this design
makes. The build requires 1.0.22 or later, the current release at that date, regardless.

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

**Signed requests.** A write carries the signer's two public keys and an Ed25519 signature
over one UTF-8 line:

```
wardplanning-v1 <call> <document id> <base version> <header hash> <payload hash>
```

`<call>` is `create` for a `POST`, `push` for a `PUT`, and `channel` for opening the live
channel. The base version is a decimal integer. The hashes are BLAKE2b-256 of the exact
bytes on either side of the body's first `\n`. **Every binary value on the wire is
base64url without padding, as in the header**: the document id in this line, in URLs and
in an invitation (22 characters), the hashes (43), the keys, the signature, and the
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
was chosen, and a target that no longer exists when it runs makes it conflict. The session
replaces a document's contents by assignment, so a reference to the document itself stays
valid across a version and a reference into it does not.

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

**A download replaces nothing until it validates**, by these checks in this order:

1. The header's `format` is one this app reads; a newer one is the notice under *File
   format*.
2. The header names the document id this device asked for, which the client checks
   whatever the server accepted.
3. The header names this device. One that names no recipient is a deleted ward (see *Who
   has access, changed by two users*), and one that names others but not this device is the
   ward no longer being this device's (see *Everything else, from the document*); neither is
   a fault, and both are recognised before anything is unsealed.
4. The data key unseals.
5. The payload decrypts and reads strictly as a version, without `device`.
6. The header matches the access list.
7. The version is not below the base this device holds.
8. The version descends from one this device holds.

Any other failure keeps the local file untouched: the local file is the one copy that has
to survive a version nobody can read. The device keeps working on that copy and pushes
nothing while the head fails; see the accepted stall under *Threat model*.

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
- whether the ward is no longer this device's, and whether it left by its own choice, so a
  device reopened offline still knows, and knows whether its copy is read-only
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
directory over the other user's work without noticing. The two imports take their previous
state at construction for this reason.

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
`UpdateFamilyCommand` holds an optional address change and a change per member keyed by
`PersonId`. Each field it holds is judged on its own: one
still at its before value is set, one already at its after value is left, and one at
anything else makes the whole command a conflict that touches nothing. A command whose
fields are all already at their after values is the result-already-holds case. Two users
editing different members of one family both apply, and a family edit never meets a
geocode, which writes the table of address coordinates instead (see *Saving*).

The same holds one level down. A member change is per field - name, gender, phones, email,
birthday, callings, parent flag, units, special-need note - each judged on its own. A
member added carries the whole new `Person`, and its result holds if that `PersonId` is
already there; a member removed carries the person as last seen, and every field of that
is its start condition, so an edit to that member since makes the removal conflict, and
its undo re-adds exactly what it removed. Even a phone change blocks it: the remover was
looking at the old phone, and being told someone changed that person first costs a redo.
Its result holds if they are already gone. **Every removal of content takes this shape**
(an access-list removal takes a user whatever they hold by then; see *Who has access,
changed by two users*): a member, a
family (by a delete or an import), a team, a tag, an asset, a task and a contact attempt
are each held as last seen, which is the removal's start condition, so a change to the
thing since makes the removal conflict, it being gone already means the result holds, and
the undo re-adds exactly what was removed. Members are matched by `PersonId`, and their order is not part of
any start condition.

Every update command takes this shape, not only the family's. Single values - a name, a
leader, a response area - are per field. Sets - a team's members, an asset's people, a
tag's people and families - are held as the ids added and the ids removed: an addition is
done if the id is present, a removal if it is absent, so two users adding different
members to one team both apply.

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
  since, the removal conflicts rather than leave that reference behind.
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

**Every write is a command, and making it so is the core of this work**, because a write
that is not a command cannot replay. That covers the directory's edits and imports,
ministering, tags, the emergency response and the access list. In particular:

- **Deleting a family** is a command, carrying the same cleanup as an import's removal,
  worked out at construction.
- **Emergency responses are a keyed collection.** Each response has an id, the document
  holds them by it, and the interface lists the emergencies in progress and switches
  between them. Ending saves the summary report and then removes the response (see
  *Emergency archives are removed*).
- **A family's contact status is the outcome of an attempt.** A contact attempt may carry
  an outcome, OK or Unable to Reach; the two buttons each record an attempt carrying theirs,
  and a family's status is the outcome of its latest attempt that has one, or Not Contacted
  when none does. Latest means the greatest timestamp, ties broken by the attempt's id. A
  command inserts an attempt at its place in that order, most recent first, as
  `CODING_STYLE.md` asks of embedded child objects, and a task likewise by when it was
  created, ties broken by its id. So the stored order is the displayed order whatever order
  replay lands them in, the same attempts always make an equal list, and a family's status
  is its first attempt that has an outcome. Timestamps come from each device's clock, so a
  wrong clock can show an older outcome as current; every attempt stays in the history, and
  this is accepted. An attempt's method and who are optional: a button press records only
  its outcome and time, shown as *Marked OK* or *Marked Unable to Reach*, while the contact
  dialog fills in both. Needs Help still comes first while a family has unresolved tasks,
  whatever its latest outcome.
- **The response lives only in the document.** The emergency being shown is read from the
  document by its id, and every change to it goes through a command. Starting an emergency
  is one command that adds the response with a record for every family, keeping names and
  addresses as they stood at the start. A family added after the start has no record and
  reads as Not Contacted, and a command that writes to a family's record creates it when
  missing, from the name and address snapshot the command took at construction, so it
  depends on nothing but the response id. Two responders touching a new family both land,
  since each change is an addition. Statistics - the progress bar and the report - count
  the response's records plus every directory family without a record as Not Contacted,
  by one rule, so the two agree when the emergency ends. The report tallies an attempt with
  no method as a status mark, on its own line.
- **The emergency response's changes are these commands:**

  | Command | Covers | On replay |
  |---|---|---|
  | `StartEmergency` | start, with a record per family | an addition; never conflicts |
  | `EndEmergency` | end, removing the response once its report is saved | conflicts if the response differs from what the report holds; result holds if already gone |
  | `RecordContactAttempt` | logging a contact, and the OK and Unable to Reach buttons | an addition |
  | `DeleteContactAttempt` | the inverse of `RecordContactAttempt`, which is how an attempt is undone | as last seen; result holds if already gone |
  | `AddTask` | adding a task | an addition |
  | `UpdateTask` | the task dialog, resolve, reopen, assign, unassign, notify | per value, as `UpdateFamilyCommand` |
  | `DeleteTask` | removing a task | as last seen; result holds if already gone |
  | `AddTaskCategory` | adding a category | result holds if present |

  `UpdateTask` holds typed optional changes, each with its before and after, to four
  values: category, description, assignment (assignee, notes and notification as one
  value), and resolution (resolved, notes and time as one value, the time fixed at
  construction). A change of assignee sets the notification to none, since the new
  assignee has not been told.
- **The access list** is new in its entirety. Invite, remove a user, remove a device,
  withdraw an invite, set a role and leave are each a command, which is what lets an access
  change replay like any other. Join is a command too, run once by the joining device under
  the invite's key, and refused with *This invite has been used or withdrawn* when the
  current header no longer names the invite.
- **Commands serialise.** Every command has `toJson` and `fromJson` through a type
  registry, so a journal entry reaches disk and comes back as the command it was; every
  command has an `inverse` in place of an undo; and every command is saved as it is made,
  so nothing is ever unsaved and the app has no unsaved state to track.
- **The update check**: on start, the app reads the server's `GET /releases/latest` and,
  when it is newer than the running version, shows a dismissible notice offering the
  download. Installers are published to the server, which serves them and the download
  page. The app's version is stated once, in the build, and reaches the running app and the
  installer from there. Versions compare numerically, part by part, so 1.10.0 is newer than
  1.9.0.
- **The app is named Ward Planning** in every name a user or the system sees. The installer
  upgrades the old install in place. App data and the tile cache follow the name, so both
  move; nothing is carried over from the old directories (see *Earlier versions*), and the
  tile cache refills as maps are viewed. The one file type registered with the system is
  `.wardinvite` (see *Receiving an invitation*).
- **Receiving an invitation**: opening a `.wardinvite` file, and the instance lock that
  hands an invitation to the app already running (see *Receiving an invitation*).
- **`docs/FORMAT.md` is new**, written with the code since the models change in this work.
  It specifies every field of the payload, `device` included, and changes in the same
  commit as any `toJson` it describes; a release that changes it raises the format version,
  as under *File format*.
- **The user guide is rewritten** for wards, *Who has access* and invites, in the words
  under *Words the user sees*, and carries the statements this design promises it makes:
  what the server and anyone holding a document id can see, and that roles are a convention
  among honest clients, not a security measure. The project's other docs change with the
  code they describe.
- **The special-needs dialog becomes one "Special Need" dialog.** Opened on an existing
  need, its person is locked; opened from empty space, picking a person fills in that
  person's current note, and that is when its start condition is taken.

**Automatic updates are the one exception, and stay out of the journal.** Geocoding and
ward and stake lookup produce derived data: an address in, coordinates out. A result is
recomputable by definition and is never a user's work to redo, so putting "set
coordinates for 12 Elm St" in the list of what this user did would describe
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
not allow it, and replay asks the same questions of the document in front of it, because
a command made when a rule allowed it can arrive where the rule does not. The rules:

- The only admin cannot step down, and cannot leave while others remain.
- A leave made while others remained cannot land where its user is now the only one: *You
  are now the only member, so leaving would delete Tanglewood Ward.*
- A user cannot remove their last device: *To stop using this ward, use Leave*, so Leave's
  rules apply.
- A member cannot add users, remove users, set roles or withdraw someone else's invite.

These are refusals rather than start conditions because what changed is somebody else's
entry. When a step-down replays onto a ward whose other admin has left, the stepping-down
admin's own role is untouched, so its start condition matches; a start condition should
not be reaching into another user's entry to notice.

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

**Ending an emergency turns it into its report.** Ending asks where to save the summary
report first. Then, with no event loop between, it reads the response by id, writes the
report from it, and runs `EndEmergency`, which removes the response and holds it as the
report was written from. If the response is gone by then, ended by someone else while the
dialog stood open, there is nothing to end and the user is told so. A version that changed the response in the meantime makes the end
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
  groups and districts it builds: an older PDF naming someone the newer directory does not
  list is out of date about them.

A family an import updates is held at the same typed level as `UpdateFamilyCommand` -
address, and a change per member keyed by `PersonId` - and only for families
that actually differ. The import service builds merged families, and the command compares
them with the existing ones field by field, holding only what differs. How the directory
import matches families and members is settled during implementation (see *Settled*).
**An import never changes a person's special-need note**, which is the app's own and which
no PDF carries.

Both imports work all of this out at construction. An import then conflicts only with an edit to something it also changes. A
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
takes their whole entry, whatever devices it holds by then, and every invite they have
made by then, since the intent is that this person is out; a device the user added or an
invite they made in the meantime goes with them, and the removal conflicts with neither. Two admins setting one user's role
differently is a conflict, so the second admin's replay is abandoned and they do it again
knowing what the first one did.

Mutual removal needs no rule of its own: if two admins remove each other while offline,
whoever pushes first stands, and the other's write is refused because the current header no
longer names them. They learn the ward is no longer theirs, and their own pending removal
stops mattering. A ward cannot be emptied by two users leaving at once.

Leaving needs the server, since the version without you must be held there before your
copy changes (see *Saving*).

**The last user's Leave deletes the ward.** Leave is one command: it takes its user out of
the access list, with their devices and every invite they made. When no one remains, the
version it writes has an empty access list, and that is a deleted ward (see *File
format*): its header names no recipient, and it carries no payload, since no one is left
to read one. It is how a throwaway ward is removed. When the user is the only one, the
confirmation says so: *You are the only member of Tanglewood Ward. Leaving deletes it: it
stops being shared and is removed from this device, and any other device of yours will
offer to remove it too.* **Delete ward** / Cancel.

An empty access list comes only this way: a user cannot remove their own last device, and
removing others always leaves the admin who removed them. On replay the existing rules
decide. A Leave confirmed as a deletion that replays where another user has since joined
is refused by the last-admin rule when the newcomer is a member, and otherwise leaves the
ward to them. A Leave made while others remained is refused where its user is now the only
one, so no ward is deleted by a leave that did not ask to delete it.

A head with no recipients is a deleted ward to every device: this device
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
   invite's `label`. Both are offered only while connected, and the invitation is saved
   only once that version is on the server, so every invitation a user can send is one that
   works. Since
   inviting and joining both need the server, a ward's users are added before an emergency
   rather than during one.

   **The invitation is a small file**, such as `Tanglewood Ward - for Bob.wardinvite`: one
   line of JSON holding the format, the document id and the seed, and nothing else. The app
   saves it where the user chooses and opens that folder with the file selected, ready to
   attach to an email, drag onto a stick or send in a text, with a short message to go with
   it:

   > Alice Smith has invited you to Tanglewood Ward. Install Ward Planning from
   > https://<server>/download, then open the attached invitation.

   The download link carries no secret, and no page the server serves ever holds the seed.
   The user sends the file by any channel they trust with it.

   The device that made the invite keeps its seed in its device-local state, saved with the
   invite before the push, for as long as the access list it holds names that invite. So
   *Save invitation* on the open invite in *Who has access* writes the file again on that
   device, and a push whose reply is lost needs nothing more: once the next fetch shows the
   invite landed, the invitation can be saved. The seed regenerates the invite's private
   keys, and it sits in the same encrypted file as the data key those keys unseal, so keeping
   it exposes nothing that file does not already hold.
2. **Joining** on a device, once the app has the invitation (see *Receiving an
   invitation*): derive the invite keypair from the seed, fetch the current version, unseal
   the data key with the invite's private key, validate as any download is validated, ask
   the user to confirm by the ward's name, then write the next version: the invite entry becomes a device
   entry, under the inviter's user for a device invite, under a new user for a user
   invite, named by the joiner, signed by the invite's signing key, whose verify key the
   current header names. The device then downloads the current version as itself and writes
   it. A device that still holds a copy of the ward from before it lost access treats that
   version as any head naming it again: its kept journal replays onto it. The removal's
   rotation put the key of that copy among the former keys, so the version descends from
   it. Before using an invite, the device reads the head: if it already names this device's
   own fingerprint, an earlier join landed and only its answer was lost, so the device
   opens the ward as itself and leaves the invite alone. A join that loses to a newer
   version fetches it and joins again on top of it.
   From then on the device signs as itself. The ward opens.
3. **Withdrawing** an unused invite is a write without its entry, by whoever made it or by
   an admin. Removing a user, and a user leaving, withdraws the invites they made.

A join and a withdrawal each drop the invite's key from the recipients, so each rotates
the data key; for a join, the joining device generates the new key, as the member it now
is. The seed lives on in the invitation wherever it was sent, and reads are open to anyone
with the document id, so without rotation an invitation found later would read every
version after it. With it, a used or withdrawn invitation opens nothing.

The invitation is the gate. Whoever opens it first gets in, visibly, and can be removed.

## Everything else, from the document

- **Who has access**: the access list, read from the open ward. Add a device, Add a user,
  Remove, Make admin / Make member, Withdraw invite, Save invitation (open invites this
  device made), Leave, Remove device (own devices), Details.
- **Access notices**: the client diffs the access list between the version it last dismissed and
  the current one, one line per entry that differs: joined as member or admin, no longer
  has access, now an admin or a member, a device added or removed, an invite made or used.
  The lines name whose entry changed, never who changed it; this device's own edits are news
  like any other.
- **The ward is no longer yours**: a head whose header lacks this device. A device that
  left by its own Leave or *Remove this device* is told *You left this ward* or *You
  removed this device from this ward*, and its copy is read-only (see *Saving*). Any other
  device is told *You no longer have access to this ward*; a removed device cannot read the
  versions that followed, so it names no one. The ward stops pushing until a head names
  this device again, and the app offers to delete the copy, in a notice that stays while
  the state lasts. Until the user accepts, a removed device's copy stays open for editing
  as before: every edit is journalled and
  saved locally, and the journal replays if a later head names this device, whether the
  ward was taken away by a server fault or the user is invited again (see *Invites*).
  Accepting deletes the copy and its journal. Editing stays open because the device cannot
  tell a real removal from a server fault or a hostile head that leaves it out, and a
  server answer may deny sharing but not use (see *Availability* under *Threat model*).
- **Last admin**: a client rule: the only admin cannot step down, and cannot leave while
  others remain.
- **A second device of your own**: *Add a device*, send yourself the invitation, open it there.
  The first-run screen asks for a name and a device name, nothing else.
- **Cleared app data, a reimaged profile or a replaced device** gives a fresh identity; wards come back by invite from
  another holder, or from your other device. The server keeps no index of who holds what, so
  there is no ward list to restore from it.

## Saving

**A command writes and pushes as it is made.** Every command in the app is a completed
action: a note is one command carrying its finished text when the user clicks OK, and so
is a contact attempt, an OK or Unable to Reach, a task change or an assignment. They arrive at the
speed a user fills in a dialog, so there is nothing to batch, and a local write is a
compress, an encrypt and a `QSaveFile` of a couple of hundred kilobytes. Saving on a timer
would buy nothing and would hold a change back from everyone else for as long as it ran.

**Automatic updates run on any device that holds the ward.** Geocoding and ward and stake
lookup change the table of address coordinates and the ward details on their own. They run after an import, when a
ward opens, whenever this device takes a version from the server by fast-forward or
replay, and after a committed command that gives a family an address the table of address
coordinates lacks, which the command knows at construction. Each run skips what is already done and what is on the failed list, so a
result lost with a push is simply computed again. Several online devices may compute
the same result at once; the results are identical, and the duplicate work is accepted.
Their results are not journalled, for the reason given above, so while the server holds a
version this app is too old to read they apply locally, drop at the replay that follows the
update, and are computed again.

**Coordinates belong to addresses, not families.** The ward holds a table from an address's
text to its coordinates, and a family's place on the map is its address's entry, so a
family whose address changes is at the new address's entry, or unmapped until it has one.
Families at one address share an entry, and an address changed back finds its entry again.
The geocoder queues each address some family has and the table lacks. A geocode result adds
the entry for its address and never replaces one already there. Coordinates a user types in
the family edit panel are their work, so they are a journalled command that sets the entry
for that family's address, replacing what is there, and its undo restores the entry it
replaced. A family with no address has no entry to set, so the panel's coordinate fields
are disabled for it, and such a family stays off the map; this is accepted. Each automatic batch also drops the entries no family's address uses, so the
table holds only what is used; an address that comes back later is geocoded, or typed,
again.

**An automatic batch pushes once.** Results apply in memory as they arrive, and the save
and push happen once, when the batch finishes or when a lookup completes, so an import
costs one more version rather than
one per family. Addresses and unit numbers that fail to resolve go on a failed list, kept
by their text in the device-local state, and are not tried again until the text changes. A
family whose address is on it stays unmapped until someone corrects the address or types
its coordinates in the family edit panel. These triggers are the only ones;
nothing geocodes on a document change as such.

A batch requests each address once and finishes when its last request has answered. A
failed address covers every family at it, since the failed list is kept by address text.

**All writes go through `QSaveFile`**, so a crash mid-write leaves the previous file intact.

**The push follows the save.** If the save fails, memory and the in-memory journal keep
the change, nothing is pushed, and the user is told the reason. The next save that
succeeds, once the problem is fixed, writes the document and journal together with the
change in them, and pushes as usual; one write still carries both, so the local file never
disagrees with itself. Every push carries only what this device has already saved, except
Leave and *Remove this device* (below), and memory runs ahead of the file only while saves
are failing. While they are, the app tries again on every command, when
its window regains focus, and when it is closed; if that last attempt fails too, closing
asks first: *Your latest changes could not be saved and will be lost. Close anyway?* A
crash or power loss while saves are failing loses the unsaved changes.

**Leave and *Remove this device* never touch the open document.** They are the two
commands that take this device out of the access list, and a local save seals to exactly
that list, so a file holding them would be one this device cannot open. They are offered
only while connected. Confirming one makes the ward read-only on this device at once,
since nothing should be made after it. The command's version is built beside the open
document, through the ordinary replay, and pushed; a push that meets a newer base replays
and pushes again, as any push does.

- **When it lands** - a confirmed push, or a head without this device after a lost answer
  - the device saves its document as it stood, still sealed to itself, flagged as left,
  read-only, with the offer to delete it, and with the journal emptied: everything in it
  came before the command and went out with its push.
- **When a rule refuses it**, the user sees that rule's wording and the ward is editable
  again.
- **When the app closes before an answer**, nothing was saved, so the next start lets the
  head decide: without this device it has left, and still naming it, the user is still a
  member and can leave again.

A device that has left is read-only because its user chose to go. A device that was removed
stays editable (see *Everything else, from the document*), because it cannot tell a real
removal from a server fault.

**An undo is the command's inverse, run like any other command.** `Command` has
`CommandPtr inverse() const` in place of `undo`: for an update, the same command with
before and after swapped; for an add or a delete, the opposite command; in every case with
the heading "Undo: *original heading*" over the original's lines. Undo builds the inverse and sends it
through the commit step - `refusal`, `execute`, journal, save, push - and redo runs the
original again the same way. An inverse onto moved ground conflicts and is handed back like
any stale edit, so incoming versions leave the undo stack alone.

The journal and the undo stack are separate, for different reasons. The journal is an
append-only log of serialised entries for what has not reached the server, and an entry
leaves when its push lands. The undo stack owns command objects, for what this user can
take back, and an entry stays after it is pushed. An undo
moves the original from the undo stack to the redo stack and appends the inverse's
serialised form to the journal; the inverse never enters the undo or redo stack. A redo
moves the original back to the undo stack and appends its serialised form to the journal
again.

The undo stack is in memory and belongs to the session. It is cleared when the document is
closed, opened or switched, and its redo side by any new command. It holds at most 100
entries, with the oldest dropped.

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
another machine without the keys that open them. The ward files and the keys sit in
sibling folders, `wards` and `keys`, so the folder *Show in folder* opens holds no key.

**The list is local**, built from the wards this device holds. The server keeps no index of
who holds what, so the list is this device's own record and nothing restores it from the
server.

A new document is created in app data under a fresh id, and naming it is a display concern.
**A ward's name comes from what it has imported**: *name* Ward for one ward and *name*
Stake for wards of one known stake, among its cases. A ward the rule gives no name, such as one that has imported nothing, shows as
*Untitled ward*. The name is read
from the payload, so the ward list, the window title, the invite message and the join
confirmation all use it, and a joiner sees it once the invite's key has unsealed the ward.
Its first push is `POST /documents`, made as soon as the network allows, and its journal
empties then like any other.

**Show in folder** reveals the `wards` folder in app data, where each ward is a
`.wardplanning` file in the documented format, and nothing that opens one: a user who
copies that folder to a stick for backup or support carries only files that need a key
the stick does not hold. It is there for support, and for getting the data out if the
server goes away. Backup is the local file on every member's device and the server's copy. The app
writes no other copies, apart from the developer-only export under *Debugging*, off by
default: anything it exported would be sealed to the ward's devices, each of which already
holds the ward.

## Receiving an invitation

**Opening an invitation joins.** The installer registers the `.wardinvite` type, so
double-clicking an invitation opens Ward Planning on it. Dropping one onto the app's window
does the same, and *Join a ward*, prominent on the first-run screen where a new user with no
wards lands and in the ward list too, offers a file picker. The app reads the seed from the
file, fetches the ward, unseals it with the invite's key, and asks *Join Tanglewood Ward?*
with the ward's own name, read from the ward rather than from the file's name; confirming
runs the join. Nothing is typed, pasted or copied.

**One instance per user.** macOS, iOS and Android run one instance of an app themselves.
On Windows the app holds a per-user `QLockFile` in app data while it runs, with
`setStaleLockTime(0)` so that only a dead process, not age, makes the lock stale; a second
launch that cannot take the lock hands what it was opened with to the running app over a
per-user local socket, and exits. The running app opens an invitation it is handed, so
double-clicking an invitation while Ward Planning is open joins in the open window; a second
launch with nothing to hand over brings that window forward. This keeps two processes from
writing the same local file and journal.

**On phones**, an attachment opens in Ward Planning through the platform's own open-in
flow, which the phone ports use.

## Single key holder notice

While the ward has exactly one user, the app shows a **persistent, dismissible notice**,
not a per-save interruption: *Only you can open this ward. If you lose your devices, it
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
| Head is older than this copy | The server has an older copy of this ward than yours. Nothing was changed. Your copy is safe and you can keep working; sharing resumes when the server catches up. |
| Head fails validation | The ward on the server could not be read. Your copy is safe and you can keep working; sharing resumes when the server has a readable copy. |
| A held ward is not on the server | This ward is no longer on the server. Your copy is safe and you can keep working; it is no longer shared. With an offer to delete this copy. |
| Header format is newer | This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work. |
| An invitation already used or withdrawn | This invite has been used or withdrawn. Ask the sender. |
| An invitation for a ward the server does not have | The server has no such ward. Ask the sender for a new invitation. |
| A file opened that is not an invitation | This file is not a Ward Planning invitation. |
| An invitation opened while offline | Connect to the internet, then open the invitation again. |
| Leave or Remove this device while offline | Connect to the internet to leave this ward. / Connect to the internet to remove this device. |
| Add a user or Add a device while offline | Connect to the internet to invite someone. |
| The invite's push loses to a conflict | Your invite could not be created, because the ward changed first. Try again. |
| Not an admin | In *Who has access*, a member sees *To add or remove users, ask an admin: names* in place of the *Add a user* button. |
| Head lacks this device | You left this ward. / You removed this device from this ward. / You no longer have access to this ward. Each with an offer to delete this copy. |
| Head names no recipient | Tanglewood Ward has been deleted, with an offer to remove this copy. |
| A leave lands where its user is now the only one | You are now the only member, so leaving would delete Tanglewood Ward. |

## Debugging

The document's plain JSON serves a developer-only "export decrypted
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
reinstall, because the uninstaller leaves local app data in place. Clearing local app data, a reimaged profile, or
a replaced device loses them.
**A user with another working device** invites the new one from it, and no one else is
involved. **A user who has lost every device** is removed by an admin and invited again,
which seals the data key to the new device. The single-holder and single-admin notices, and
the suggestion to add a second device, are what keep a ward out of the state where neither
is possible. There is no exported recovery key file.

## Testing

**The harness is part of the work**, since the suite starts here: one Qt Test executable linked against `WardPlanningLib`, with `tests/services`
for format, crypto, journal and replay, and `tests/server` for the fake server and the
conformance suite. The instance lock needs two processes, so `tests/lock` is a second,
small executable that starts copies of itself, each taking the lock through the app's own
lock function. `tests/spec` is a third, a reader of the format built without Qt or any of
the app's code, from `nlohmann-json`, `zlib` and `libsodium`, so that it shows the specification complete rather than the app agreeing
with itself.

- **The server interface** is one abstract class in the client with the calls the app
  makes: create, head, fetch, push, open channel, and latest release. `HttpServer` implements it for the
  app; `FakeServer` implements it in process for tests, holding versions in memory and able
  to drop a reply after storing a write, delay a notice, or refuse to answer.
- **A test client** is a full client - its own identity, app data directory, document,
  journal and undo stack - so a multi-user test is two or three of them against one
  `FakeServer`, driven step by step with no timers. The document, journal, sync and replay
  live in a `WardSession` class with no global state, which a test creates as many of as it
  needs; the app holds one session for the interface.
- **Each registered command type has a sample.** Each type registered for `fromJson` also
  registers a sample instance with a document for each outcome the type can give: one where
  its start condition holds, and, where the type has them, one where its result already
  holds and one where it conflicts. The round-trip test iterates the registry.
- **Every outside dependency has a fake in `WardSession`**: the server, the geocoder, the
  ward and stake lookup, and the clock that stamps attempts and resolutions. A test sets
  each, so nothing reaches the network and timestamp ordering is decided by the test. Every
  timestamp the app writes comes from that clock: a command reads it at construction, and
  the report's end time comes from it too.

Format and crypto, in `tests/services`:

- `DocumentCrypto` round-trip
- two consecutive saves of an unchanged document produce different nonces
- the header is bound as associated data: with the unmutated file opening as the positive
  control, a header changed only where parsing, unsealing and the document id check all
  accept it - another recipient's `sealedKey`, re-encoded validly - fails the payload's
  AEAD check, asserted as that failure; and a local file whose header id and file name are
  both changed to another ward's id fails the same way
- the known-answer vectors for the fingerprint, the invite keypairs derived from a fixed
  seed, and the signed line for each of `create`, `push` and `channel`
- a local file holding a value in every field of every part, written by the app when the
  test runs, is read back by the reader in `tests/spec`, written only from this section and
  `docs/FORMAT.md`, which parses every field strictly and recovers each of those values, so
  a `toJson` change that writes a field `docs/FORMAT.md` lacks fails it
- a header whose `format` is above every known version is refused whole and reported as
  newer, while one below every known version, or not an integer, fails validation as
  unreadable; for the newer case, nothing pushes while every
  edit journals and saves locally; the state clears as soon as the server again serves a
  version the app understands, and the journal then replays
- a header or payload with a missing or malformed required field is refused, never read in
  part; a payload written by the previous format version reads correctly, through its
  upgrade where the release has one
- a version whose header has a fingerprint matching no device's or invite's key pair, one
  built from one device's X25519 key and another's verify key included, or whose access
  list has a device or invite with no header entry, is refused
- a head whose version is below this copy's is refused and changes nothing
- a removal rotates the key, after which the removed user's devices cannot unseal; a write
  that removes nobody keeps the key; the written header has exactly the expected entries,
  each matching an access-list key
- a device the server mentions but this client did not admit is never sealed to
- a version under a key this device never held for the ward, naming none of its keys
  among its former keys, is refused and changes nothing
- a version of another ward this device holds, served under this ward's id, is refused by
  the document id check before anything is unsealed, and changes nothing; the same version
  with its header's id rewritten to this ward's fails to decrypt and changes nothing
- after a join, and after a withdrawal, the invite's seed can no longer unseal the current
  version
- an invitation file written by *Add a user* reads back to the same document id and seed,
  and any other file, a `.wardplanning` among them, is refused as not an invitation

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
  applies, and the family has both the new phone and its address's new entry
- an address edit places the family at the new address's entry on every device that takes
  it, unmapped until the geocode for it lands, on the editing device as well; a geocode
  for the old address that lands before the edit, or after it, leaves the family at the new
  address's entry and none other
- typed coordinates set the entry for the family's address, are not replaced by a later
  geocode of that address, and are restored to the previous entry by their undo
- an automatic batch drops the entry of an address no family has any more, and keeps every
  entry in use
- geocoding the families of an import pushes one version for the whole batch, and an
  address that fails to resolve is not tried again - on later versions, after closing and
  reopening the app, or by a command that gives another family a new address - until its
  text changes
- a batch with two families at one address sends one request for it, maps both families
  by their shared entry, finishes, and pushes once
- a command that changes no address starts no geocoding
- two offline edits to different members of one family both apply
- an offline removal of a member, replayed after someone changed that member's phone,
  conflicts and leaves the member with the new phone; the same removal replayed onto an
  unchanged member applies, and its undo restores a member equal to the one removed
- an offline task deletion, replayed after another responder resolved that task, conflicts
  and leaves the task resolved with its notes; an offline team deletion, replayed after
  someone added a member to that team, conflicts and leaves the team
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
- the only user's Leave, made while an invite is open and replayed after that invite's join
  landed: with the newcomer a member, it is refused with the last-admin wording and the
  ward keeps both users; with the newcomer an admin, it applies, the ward stays with the
  newcomer, and nothing is deleted
- Add a user and Add a device are not offered offline, and an invitation is saved only
  after the version naming it is on the server; an invite whose push loses saves none
- an invite whose push is stored but whose reply is dropped, with the app closed before the
  next fetch, offers *Save invitation* after that fetch, and the invitation joins; once the invite is
  used, withdrawn, or abandoned in a replay, the seed is gone from the local file and no
  invitation is offered; *Add a user* with no answer to who it is for makes no invite
- Leave and Remove this device are not offered offline; once confirmed, the ward accepts
  no edit; on a push refused by a rule, the ward is editable again and the local file is
  unchanged; on a push that succeeds, the local file still opens on this device, read-only,
  marked as left, with an empty journal; with the answer dropped, the next head ends in
  that same state; with the app closed before an answer and the push stored, the next
  start finds the device left, and with the push not stored, finds it still a member
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
- re-importing the same directory keeps every special-need note and every family on the
  map, and holds no change for families the PDF leaves as they were
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
- running a command does not change the command: one command object, executed where it
  applies, serialises to the same entry it gave before it ran, and the same object executed
  next on a moved version conflicts and leaves that version untouched, which fails if
  `execute` captured anything
- an access-list command replays: two admins removing different users both apply; removing the
  same user twice applies in full; two different roles for one user is a conflict; a
  removal replayed after that user added a device applies and takes the new device too; one
  replayed after that user made an invite withdraws it, and that invite's seed cannot
  unseal the version the removal wrote; a user who leaves has their open invites withdrawn
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
- a ward this device has had on the server, now answered `404`, keeps its copy and journal, pushes nothing,
  never sends a `POST` for it, accepts and saves new edits, and offers to delete the copy
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
  agrees, and says it no longer has access, both mid-session and on next open; an edit made
  in that state is journalled and saved; when a later head names it again, it resumes
  syncing and its journal, that edit included, replays; a removed user invited again joins
  from the device holding the old copy, and its journal replays onto the joined version
- a user joins only through an invite the current header names, and a used or withdrawn
  invite is refused
- a join whose answer is lost, with the invitation opened again, opens the ward as a member and
  never reports the invite as used or withdrawn
- two admins removing each other: the first stands and the second's write is refused
- the channel refuses an open not signed by a device the current header names, and accepts a
  key holder's
- the channel reports who is present, reports a newcomer to those already there, and reports
  a departure on close
- a version notice on the channel causes the other clients to fetch and validate

The instance lock, in `tests/lock`:

- while one process holds the lock, a second is refused it; once the holder is killed, a
  new process takes it; a second process started with an invitation's path hands that path
  to the holder, which receives it, and exits

The fake and the real server are checked against each other. One conformance suite, written
against the client's server interface, runs against both: against the fake in the ordinary
test run, and against the real server over HTTP when `WARD_PLANNING_SERVER_URL` is set. A
disagreement is resolved in whichever implementation is wrong, never by loosening a test.

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
  prune a document nobody has opened or written in a year. Pruning a ward still in use
  leaves its members on the missing-ward stall (see *Settled*), which such a rule has to
  weigh against the storage it frees.
- **Recovery from a member's local file.** Recovery is a new ward rebuilt from the PDFs. A
  member's own local file could be published as the new ward instead, keeping notes,
  ministering and emergency data the PDFs lack: one "Publish as a new ward" action, which
  creates a document under a fresh id with no former keys or landed marks, and removes users
  from its access list as it goes, so the device that caused the trouble is not carried
  over.
