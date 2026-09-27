# Encrypted Shared Document — Design

**Date:** 2026-08-30
**Status:** Design agreed; revised after design review; revised again to hold the
authoritative copy on a Ward Planning server rather than in a third-party sync folder;
revised on 2026-09-24 to replace two-person removal and the 7-day waiting period with
admin and member roles, and to make leaving immediate.

---

## Purpose

Ward leaders need custom views of member data that the official church tools do not
provide, and bishoprics commonly build those views in shared Google Sheets. Church
policy asks that member data live in official systems. Ward Planning resolves that
tension by keeping the data in a document only the bishopric can read.

**Sharing has to be something a busy, non-technical counselor can set up in a couple
of minutes.** That is the primary goal of this design, and it decides the shape of
everything below.

The obvious approach, an encrypted file in a folder that Google Drive or OneDrive
already synchronises, fails that test on one specific point: it creates **two access
lists that have to be kept in agreement.** Adding a counselor means sharing the folder
in the provider's own interface *and* granting access in the app; removing means both,
and forgetting the folder step leaves that person with write access and version
history. Each of those is a step a casual user performs wrong. Before any of it, every
member needs an account with the same provider, the sync client installed, and the
ability to find the synchronised folder on disk.

**So the app talks to a Ward Planning server directly.** One access list, owned by the
app. Sharing is: share the document, send the link, they install and click Request, an
admin clicks Grant. No provider account, no sync client, no folder path.

**Every member still keeps a complete local file.** It opens, edits, and saves with no
network at all, which is what makes the application dependable during an emergency,
the moment it exists for. The server holds the authoritative copy, in the same
encrypted bytes the local file holds, and can read none of them.

A document that has not been shared is simply a local file. The server enters the
picture when someone shares it.

## Scope

This design covers the encrypted document, its access roster, safe concurrent use,
and the contract between the app and the server. The server's own implementation,
hosting, and operation are a companion piece of work; what follows fixes the
interface that work has to present.

Two related ideas surfaced during design and are deferred to their own work:

- **Spreadsheet sync** — column-mapped add/remove against an external `.csv`/`.xlsx`
- **Native talks tracker** — speaking history held in the document itself

## Decisions

| Question | Decision |
|---|---|
| Primary goal | Sharing that a non-technical user completes in a couple of minutes |
| Where the authoritative copy lives | A Ward Planning server, reached by the app directly |
| Server address | Built into the app, with an advanced override; documents carry only their id |
| Local copy | Complete, always present, fully editable offline |
| Running instances | One per user; a second launch hands its arguments to the running window and exits |
| Where the local file lives | Application data, keyed by document id; the user never chooses a path |
| Third-party sync folders | Not built. Continuity rests on the project being open source. |
| Key model | Envelope encryption, one data key per document, wrapped per person |
| Key generation | Silent, on first run. No user-visible key material. |
| Server authentication | Ed25519 challenge-response against the devices in the document's current header |
| Write attribution | Every header signed by its writer's Ed25519 key |
| Revocation | Rotates the data key; the removed member's app deletes its local copy once the signed header chain proves the removal |
| What the server holds | Ciphertext, public keys, and write metadata. No key material. |
| Private key storage | qtkeychain (Windows Credential Store, macOS/iOS Keychain, Android Keystore, Linux Secret Service), built from source against the system Qt |
| Crypto library | libsodium ≥ 1.0.22 from vcpkg, confirmed to contain commit `ad3004e` |
| Who is on the roster | People. Each person has one or more linked devices; keys never leave the device that made them. |
| Roles | Each person is an **admin** or a **member**. The ward's creator is its first admin, and every version that lists anyone lists at least one admin. |
| Who may grant | Any admin adds a person; only a person links their own devices |
| Who may remove | Any admin, alone and at once: a person, another person's device, or another admin. An admin promotes and demotes, including stepping down while another admin remains. Every change to who has access is announced to all members. |
| Leaving | Any person, at once, by publishing the version that drops them — no rotation and nobody else involved. The last admin promotes someone first. |
| Fingerprint confirmation | Shown by default for device keys this device has not seen before |
| Recovery | Link from another of your devices; with none left, an admin removes the old person and grants you again; persistent notices appear while only one person holds access or only one person is an admin |
| Identity scope | Per person, across linked devices. Adding a phone is a link from your laptop, not a new grant. |
| Document storage | One encrypted payload. Merge granularity is a client concern, so storage stays simple. |
| Ordinary concurrency | The lease keeps one writer of the directory and ministering at a time while the server is reachable; roster changes never need it |
| Emergency concurrency | Emergency response records need no lease: responders record in parallel, online or offline, and records merge three ways |
| Encryption coverage | Every document; emergency archives are removed |
| File extension | `.wardplanning`, for exported copies and file association |

---

## Threat model

Naming this explicitly, because several design choices only make sense against it.

**What is being protected, and how much.** The document holds a ward's member directory:
names, addresses, phone numbers, birth dates, family relationships, and the ward's own
planning notes. That deserves real protection — it should not sit readable on a server or
in a stray file — but it is not money, credentials, or secrets anyone would mount an
elaborate attack for. Easier routes to the same data already exist: a focused phishing
attack on a ward leader's church website login yields the whole directory with far less
effort than anything this design could be attacked with.

So the defences are proportionate. The design protects against the realistic failures —
a server operator who can read what it stores, a lost or stolen device, a mistaken grant,
a file that leaks, a server bug — and keeps each defence cheap in code and invisible in
use. **Attacks that need collusion** (two members together, or a member with the server
operator) **or a chain of unlikely steps are accepted, not defended**, and are listed below
in a line each. Where a defence would cost usability or much code to stop such an attack,
the simpler design wins.

**Defended against.** The server operator reading member data — the operator is the
application's author, and the design keeps that from being a matter of trust. Anyone
who obtains a file without ever having held a key. A revoked person's access to
future versions, which the server refuses at the allow-list. Rollback to a
pre-revocation version, which the server refuses by never accepting a lower version
or key generation. Silent corruption or truncation.

**Detected, not prevented.** Tampering with a local file's header, including its
document id — the header is bound as associated data, so a modified local file fails
to decrypt rather than quietly attaching itself to a different document.

**Inside the trust boundary.** Every member can read everything and write whatever they
like into the document. Admins also decide who has access: an admin grants, removes, and
changes roles alone. A ward's admins are its bishopric and clerks — the people who would
hold that authority anyway — and the design trusts them with it.

**Accepted: one compromised admin device can remove everyone.** Removal deletes the
removed person's local copies, so an admin device in hostile hands could remove every
other person and leave the ward's data only on itself. This was weighed against
requiring a second admin's approval and judged the better trade for a ward: the
realistic threat is a lost device, not a hostile one, and what a lost device needs is
someone able to remove it quickly and alone. A hostile use is visible at once — every
roster change is announced to every member. **Recovery is a new ward:** someone creates a
fresh document, imports the ward directory and ministering PDFs again, and grants people
again; the old ward is abandoned to whoever took it. What the PDFs do not recreate — tags,
teams, emergency assets, special needs, notes, and past emergency records — is lost.
Content vandalism, already possible for any member, is recovered the same way.

**Accepted: a stolen, unlocked device acts as its owner.** It can read everything, write
content, unlink its owner's other devices, and link devices of the thief's own under its
owner's name — and, on an admin's device, anything an admin can do. Any admin removes it:
the device alone, or its whole person, who is then granted again. Planted devices are
shown for what they are: the access list says which device added each one, and unlinking
a device offers to unlink every device it added.

**Cleaned up, best-effort.** A revoked member's local copy, and every copy the app wrote
from it — exported copies and recovery copies still where the app put them. Their app
deletes them the next time it connects and verifies the revocation, and refuses to open
a copy of that document afterwards, which covers the ordinary case of a released leader
who would never think to remove them. A copy moved or sent elsewhere, or a device that
never connects again, keeps what it had.

**Accepted: a hostile server could show a device a ward it never joined.** Device public
keys appear in every header, so an operator could build a look-alike ward containing a
member's device and let it arrive with the ward list after a reinstall; data typed into it
would be readable by the operator. The server's own rule — a device is added only where it
asked or paired — is what normally prevents this, and an operator does not run its own
rule. Keeping a separate record of every ward this device asked for was judged more
machinery than the case is worth; a linked device does take its identity and its ward list
from the pairing, not from the server.

**Accepted: a remote access request is not verified.** Whoever holds a forwarded invite
link, or a hostile server, can put a request in front of an admin under any name; an
admin who grants it gives full access. Comparing a code remotely was judged too much
friction; name-match flags, grant announcements, and removal limit the damage, and the
in-person path verifies.

**Accepted: a hostile operator can split members onto separate copies.** The server
could show each member a different current version and accept each member's edits into
their own fork; every sync indicator would read *Up to date* while nobody saw anyone
else's work. Nothing is exposed and every member keeps their own edits. Detecting it
would need every header kept forever or members comparing versions directly, and the
gain to an attacker is disruption only.

**Outside the app's reach.** Reports exported by `ReportGenerator`, which remain
plaintext PDFs.

**Known to the server, and accepted.** Per document: the ciphertext and its size,
which leaks approximate ward size; the people and device keys with access, and
therefore how many people are on it and how many devices each uses; which device wrote
when, and from what address; the display name and device name attached to an access
request, a pairing, or a lease. The server learns the
shape of a bishopric without being able to read the directory. This is comparable to
what a sync provider learned in the folder-based design — it also saw account
identities and file sizes — with the difference that the operator is now the
application's author rather than a provider the ward already uses. The user guide says
so plainly.

**Availability.** The server going down denies *sharing*, not *use*. Every member
continues to open, edit, and save their local file, and uploads resume when the server
returns. This is the property that decides the whole shape of the design.

---

## File format

A `.wardplanning` file is a plaintext JSON header followed by an encrypted payload.
The header is readable without a key so the app can identify the format, locate the
reader's entry, and check versions without decrypting anything.

**Header**

- format version
- document id: 128 random bits generated by the app when the document is created,
  and never changed
- origin document id, present only on a copy: the id of the document it was copied from
- key generation counter
- save counter, the device fingerprint of the last writer, and the time of that write
- the last writer's X25519 public key and Ed25519 verify key, whose fingerprint must equal
  the last-writer fingerprint
- payload nonce
- recipient entries, one per device: `{kind, member id, device fingerprint, X25519
  public key, Ed25519 verify key, data key sealed to that device's public key}`
- admins: the member ids of the people who are admins, a subset of those in the
  recipient entries
- payload hash: 32-byte BLAKE2b of the encrypted payload, personalised `wp-payload`
- writer signature: Ed25519, by the last writer, over the associated data and the
  payload hash

The write time lets the conflict dialog say *who* saved and *when*. The name itself
is looked up by fingerprint in the reader's own roster — the header carries no names.
The `kind` tag names the sealing scheme.

**Parsing is strict; a newer version is refused, never guessed at.** A reader accepts a
header or payload only when it understands all of it: a format version it knows, every
recipient entry of a `kind` it knows, a payload version it knows, and every required
field present and well formed. Tolerant parsing — skipping what it does not recognise —
mostly produces data read wrongly, so anything else is refused whole. Adding a new
recipient kind, such as a hybrid post-quantum entry, is therefore a new format version.

**When a newer app wrote the version on the server**, this app keeps the local copy it
has, with the **directory read-only** and **nothing published**, and a persistent notice:
*This ward was saved by a newer version of Ward Planning. Update to see the latest
changes and share your work.* **Emergency response edits stay available** and are saved
locally, so responders keep working through an update; they merge after the app is
updated. The update notice offers the download. The state is **checked again on every
server check**, so it lasts only while the server keeps presenting a version this app
cannot read — a server that presents a false one once cannot freeze the app beyond that. The server supports a format before any app writes it.

**The server address lives in the app; the document carries only its id.** A file
handed over on a USB stick still knows which document it is, so the recipient's app
can offer to request access rather than making them hunt for a link. The app supplies
the server, from a built-in address with an advanced override for testing and
self-hosting. Moving the service to a new domain is therefore an app release, which is
the natural shape of an open-source maintainer adapting the project, and every
existing document follows without being rewritten.

**One id for the document's whole life.** The id names the local file, keys the local
record, and is bound into the header from the first save. Sharing registers that same
id with the server, so nothing is renamed, re-keyed, or re-encrypted when a document
is shared. Whether a document is shared is a property of the local record — it has a
base version — not of the id.

**Copies get a fresh id and a fresh key, and remember their origin.** An exported copy or
recovery copy is encrypted under a new data key — so nobody who once held the ward's key,
such as someone who has since left, can read it — and carries a new random id the server
has never seen, so a copy can never publish itself
into the ward's document, and its local records never collide with the original's. It
also carries the original's id as its origin, bound in the associated data like every
other header field, so the origin cannot be edited out without breaking the file.

**Copies the app writes are cleaned up with the document.** The app records the path of
every copy it writes, per original document. When a removal is proven, it deletes each
recorded copy still at its path along with the document, and remembers that this device
was removed from that document; **Open a copy** then refuses any copy whose origin is a
document this device was removed from. A copy the user moved, emailed, or backed up
elsewhere is out of the app's reach, as is anyone decrypting a copy with their own keys
outside the app.

**Payload**

Today's `JsonService` JSON, compressed, encrypted with XChaCha20-Poly1305 under the
data key, as **one payload**. Splitting the document into separately encrypted
sections was considered and set aside: reconciling two versions happens on the model,
in a client that holds the data key, so merge granularity owes nothing to storage
granularity. One payload means one nonce, one version, and one write.

**Nonce.** 192 bits from `randombytes_buf`, generated fresh on **every write**, never
derived from the save counter and never carried over from the loaded file.
XChaCha20-Poly1305's extended nonce exists precisely so random nonces are safe
without counters or stored state. This matters more than it looks: the data key is
long-lived, so a repeated nonce would XOR two versions of the ward directory against
each other and expose the Poly1305 one-time key. The design's own concurrency
scenario — two people saving at the same save counter — is exactly the case a
counter-derived nonce would break.

**The header is bound as associated data.** Format version, document id, key
generation, save counter, last-writer fingerprint, nonce, the full recipient entry list,
and the admin list are passed to the AEAD as associated data. Anyone holding the data key therefore
detects header tampering on decrypt.

**Every write re-seals.** A save rebuilds every recipient entry from the roster rather
than carrying the old ones forward, so a sealed key that was written wrong — by a bug or a
tampered app — stops locking its owner out of every later version. Sealing a 32-byte key
to a handful of devices costs nothing worth saving.

**Every write is signed.** The writer signs the associated data followed by the payload
hash, using their Ed25519 signing key. The payload hash and the signature are the two
header fields outside the associated data: the hash is computed from the ciphertext
the associated data produces, and the signature covers both. Because the hash is
carried in the header, the signature covers the payload too and a header can be
verified on its own, without the payload.

**A header verifies its own signature.** It carries its writer's public keys, bound in
the associated data and checked against the last-writer fingerprint when parsed, so the
signature is checked the same way everywhere — on open, on download, and in the chain —
without looking the writer up anywhere first. *Who* the writer is then comes from the
roster, or, for a leave, from the header before it: a leaver has just left the roster, and
the chain proves they were in it. Members check on every open that the payload hash
matches the payload and the signature verifies, and a version whose header and roster
disagree in any way is refused (see *The payload roster*).

**Roles live in the header.** A removed device can no longer decrypt, so it proves its
removal from headers alone — and that proof has to show the remover was an admin. The
header therefore carries the admin list in the clear, beside the recipient entries, and
the payload roster carries the same roles for display. Like member ids, the admin list
names no one.

**Headers form a verifiable chain of membership.** Recipient entries carry member ids
and verify keys, and the admin list says who may change them, so anyone holding one
trusted header can check the next without decrypting anything. Here *an admin* means a
person on the previous header's admin list. The next header is valid when:

1. Its writer device appears among the previous header's recipients with the same keys
   the header carries for it, and its signature verifies.
2. Its save counter rises. Its key generation rises by exactly one when a device or
   person is dropped by an unlink or a removal, and stays the same otherwise, so a key is
   never rotated without a membership change, and a header skipped for leaving membership
   unchanged never hides a rotation. The rising counter is what orders versions of one
   generation, and so what lets a device refuse an older one. A counter that only rises
   could be pushed to its maximum to freeze the ward, so it is capped at 2^53 - 1 (exact in
   every JSON reader) and the server refuses any upload that raises it by more than 2^20,
   about a million saves between publishes. With uploads limited to 600 an hour per
   device, reaching the cap would take one device some 1,600 years of continuous
   publishing. The chain itself requires only a rise, because a client walks it with
   headers missing.
3. Its writer device appears among its own recipients — except in a leave (rule 7).
4. Every device fingerprint belongs to exactly one person and matches the entry's own
   keys; no two entries share an X25519 key or a verify key; a device that appears in
   both headers has the same member id, X25519 key, and verify key in both — a device's
   keys never change; every admin is a person in the header; and a header that lists
   anyone lists at least one admin.
5. A device added to a person who was already present is written by one of that
   person's devices. Only a person links their own devices — an admin cannot attach a
   device to someone else.
6. A device dropped while its person remains is dropped by another of that person's
   devices or by an admin, and the key generation increases. A person's last device is
   never dropped this way; that removes the person.
7. A person dropped entirely — all their devices — is dropped by an admin, and the key
   generation increases; or the writer is that person, leaving: nothing else in the
   membership changes and the key generation stays the same.
8. A person added is added by an admin.
9. The admin list changes only by an admin's hand, apart from a leaver dropping off it.

Walking that rule forward from a header the reader already trusts proves each later
header was written by a device a known member had admitted, that every device change
came from its own person or an admin, and that every removal was made by an admin or
by the person who left.

**A leave does not rotate the key.** The leaver would generate the new key, and so know
it, so rotating would protect nothing against them; they are leaving by choice, their
app deletes its copies, and the server stops serving them the moment the leave lands. The
next removal or unlink rotates as usual.

**Only membership changes are kept in the chain.** A header's *membership* is its set of
recipients — member id, device fingerprint, and verify key — together with its admin
list. A header whose membership matches the previous
header's changes nothing the rule checks: the next writer is looked up in an identical list, drops are measured against
an identical list, the key generation cannot have moved, and increasing save counters
hold across the gap. So the server keeps a header only when membership changed, plus
the current one, and the chain grows with grants and removals rather than with every
publish. There is no age limit; a handful of changes a year stays small indefinitely. This is what lets a removed member's app confirm its
removal — including when the admin who removed it was granted or promoted after its
last sync — on evidence the server cannot forge.

Verify keys are public, and the server needs them anyway to authenticate devices, so
carrying them in the header reveals nothing new.

**The payload roster is authoritative.** It holds people:
`{member id, display name, role, devices}`, each device
`{device name, X25519 public key, Ed25519 verify key, fingerprint}`. The header's
recipient entries are an index into it, keyed by device fingerprint. This is what makes
revocation implementable — a fingerprint is one-way, so re-sealing to the remaining
devices requires their actual public keys, and only the roster has them. **The header and the roster must match exactly**: every recipient entry has a roster
device with the same member id and verify key, and every roster device has exactly one
entry, and the header's admin list names exactly the roster's admins. A version that
fails this is refused like any other invalid version — only a bug
or a tampered app could write one, and an unattributable key must never sit on the
access list.

**The roster lists people, each with their devices.** Adding Bob gives Bob access; Bob
then uses the app on his laptop and his phone by linking them, without asking anyone.
Keys still never leave the device that made them, so each device has its own entry
under its person, and the data key is sealed to each. The device name is what lets Bob
unlink "Surface Laptop" rather than guess between two rows.

**Names stay out of the header.** The reader finds their entry by device fingerprint,
and member ids are random, so the header carries no names. Anyone who obtains the file learns nothing about who the
bishopric is.

**Fingerprints are at least 128 bits:** 16-byte BLAKE2b over the device's X25519 public key
followed by its Ed25519 verify key, personalised `wp-fingerprint`.

**Every BLAKE2b in the format carries its own personalisation:** the string, zero-padded
to 16 bytes, in BLAKE2b's parameter block, with a zero salt: `wp-fingerprint`,
`wp-payload`, `wp-pair-commit` for a pairing commitment, `wp-pair-number` for the
comparison number, and `wp-request` for a signed request's body. A digest made for one purpose can never equal one made for another,
whatever the inputs and lengths. The strings are part of the format, and the companion
server reproduces them; the known-answer vectors pin them. They are an identity handle and a human comparison aid, not a
shortened secret. Because the fingerprint covers both keys, a device cannot keep its
identity while either key is swapped: a changed key is a different device, which only its
own person can add.

**A device checks its own entry.** A device refuses any version whose entry under its
fingerprint carries keys other than its own, so no writer can re-key someone else's
device and then sign in its name.

**What header tampering can and cannot do.** Adding a recipient entry requires
sealing the data key, which requires already holding it, so an outsider cannot add
themselves. Someone who edits a local file's header to strip entries produces a file
that fails authentication on the next open. Rollback — presenting an old, internally
consistent version whose generation predates a revocation — is refused in two places:

1. The server never accepts an upload at a lower version or key generation, so the
   shared copy cannot be rolled back at all.
2. A **key-generation and save-counter high-water mark** per document, kept in the device's keychain
   record beside its keys, so it survives an uninstall and cleared application data and
   is never deleted — not even when this device is removed from the ward. A local file or
   a server version presenting an older generation is refused before it is ever used.

**Post-quantum.** X25519 is exposed to harvest-now-decrypt-later, and hybrid
X25519 + ML-KEM-768 is now in production elsewhere. The exposure needs an attacker
holding the file today and a quantum computer later, against data with a
few-year sensitivity horizon, so this design stays classical. A hybrid entry kind can
be added later as a new format version, which every member's app picks up by updating.

**Compression before encryption** is safe here — there is no adaptive
chosen-plaintext channel, so CRIME-style attacks do not apply. It does leak
approximate ward size to the server, which is accepted.

**Primitives.** `crypto_box_seal` wraps the data key to an X25519 public key.
Anonymous sealed boxes mean granting access needs nothing from the granter's own key.
Note that sealed boxes use XSalsa20-Poly1305 internally, not the XChaCha20-Poly1305
of the payload; for a 32-byte data key this is immaterial, but it is worth not
mis-stating. `crypto_sign` (Ed25519) authenticates the app to the server and signs every header.

**Every signature is tagged with what it is for.** One signing key signs several kinds
of message, and a signature alone says only that the key approved some bytes. So each
signed message begins with a short ASCII tag and a single 0x00 byte — `auth` for a
login challenge, `request` for a mutating request, `header` for a document header,
`pairing` for what a device tells the device it has just paired with — and verification
prepends the tag it expects. The tag is never stored or sent; the signer and verifier
each supply it. This matters because the server chooses the login challenge: without
tags, a hostile server could send challenge bytes that are exactly a header of its own
making and collect an admin's genuine signature over them, which would let it forge a
removal and delete members' local copies. With tags, a signature the server could influence
starts with `auth` 0x00 and can never verify as document evidence.

**Library pin.** CVE-2025-69277 (`ge25519_is_on_main_subgroup` missing a `Y == Z`
check) does not affect any call this design makes. The build requires libsodium 1.0.22
or later regardless — the vcpkg baseline this project started from builds 1.0.20,
which predates the fix — and integration confirms `ad3004e` is an ancestor of the
release used.

---

## Identity

**An identity is a person with devices.** On first run a device generates **two
keypairs**: an X25519 pair that documents are sealed to, and an Ed25519 pair that
authenticates this device to the server and signs its writes. They are created
together, stored together as one record through `KeyStore`, and loaded together.
libsodium can derive one from the other, and this design does not: two independent keys
cost nothing and keep the sealing key and the signing key each doing one job.

The first screen asks one question: **"Do you already use Ward Planning on another
device?"**

- **No** — the device creates a new person: a random member id and a display name,
  editable later. It then requests access to a ward as that person.
- **Yes** — the device joins an existing person by linking. It shows a pairing code;
  the person enters it on a device they already use, confirms that both screens show the
  same six-digit number, and that device adds this one to every ward they belong to.

Asking this first is what keeps one person from becoming two by accident: a second
"Bob" granted from a phone would be a separate person on the roster, with none of the
first Bob's devices and a role of its own.

**Linking a device.** The two devices pair by **comparing a number shown on both
screens**, the way Bluetooth devices pair:

1. The new device picks a random value and posts a *commitment* — a hash of its public
   keys and that value — with its device name, and shows a short pairing code, which is
   only a lookup handle and not a secret.
2. The person enters that pairing code on a device they already use, which posts its own
   public keys to the pairing.
3. The new device then reveals its keys and random value; the existing device checks
   them against the commitment.
4. Both devices show the same six-digit number, computed from both devices' keys and the
   random value: *Do both screens show 482 193?* The person confirms on both.

Each device keeps the message it read from the other — the existing device keeps the
commitment before posting its keys, the new device keeps those keys before revealing —
and checks against what it kept, never against a value re-read later. The server relays
every step and can substitute keys, but it must commit to a substitute before it sees the
existing device's keys, and it learns the random value only after that, so it cannot
search for a match: a substitution succeeds with a one-in-a-million
chance per attempt, and every failed attempt shows the person two different numbers.
Each pairing step can be written once only, and a pairing expires after ten minutes.

Once confirmed, the existing device tells the new device who it now is — member id,
display name, and the wards it is being added to — **signed with the key the new device
just authenticated**, so that identity comes from the pairing and never from the server.
It then adds the new device under its own person in every ward document it belongs to. A background queue carries the change to each ward — the
open one at once, the others by taking each ward's lease in turn, applying the change on
top of the server's version, and publishing — retrying until every ward has it, and
showing what is still pending. The new device waits for its wards to appear, and learns
its member id and display name from the first ward's authenticated roster, never from
the server.

**Unlinking a device** — a lost phone, a retired laptop — is done by the same person
from any other of their devices, or by any admin, rotates the key in every ward through the same
background queue — so a lost phone is cut off everywhere within minutes, not when each
ward is next opened — and deletes the unlinked device's local copy when it next connects. A person's last device cannot be unlinked; that is
leaving. **Unlinking a device also lists every device it linked**, taken from the kept
headers that recorded each link and its writer — *This phone also added "Dell Laptop" on
3 September. Unlink it too?* — with **Unlink both** as the default, so a device planted from
a stolen phone does not outlive the phone.

**Identity outlives an uninstall.** The keypairs live in the platform credential store,
which an uninstaller does not touch, so a reinstall finds the same device identity and
the same access. Application data is less durable, and the ward list sits there — which is
why the ward list is fetched from the server rather than kept only locally. The
key-generation high-water marks live in the keychain record with the keys, for the same
reason.

There is no passphrase-protected identity export: linking covers new devices, and
an admin's grant covers losing all of them.

`KeyStore` is a narrow wrapper — `store`, `load`, `remove`, with `load` reporting
not-found distinctly — over qtkeychain, whose
job is to present that library's asynchronous jobs as something the document-open
path can sequence cleanly.

**Identity resolves before any document exists.** `main.cpp` constructs
`DocumentManager` before `MainWindow`, and `ensureFilePath()` will invent a path and
save an untitled document, while `autoSave()` can fire from a network callback with
no user present. So identity resolution is a startup precondition: until the keypairs
are loaded or created, no document is created and no save is attempted.

**A failed encrypted save is a modal error.** Today `autoSaveFailed` reaches a
transient `statusBar()->showMessage(...)` that the next status message overwrites — a
user would not notice their work had stopped being saved. A failed *upload* is not an
error and does not interrupt: it is a sync state, described below.

On Linux with no Secret Service running, qtkeychain has no fallback and the app has
no identity. The startup path reports this specifically rather than failing as a
damaged file.

---

## The server

The server is a blob store with a version check and an allow-list. It holds no key
material and never reads the payload; its only cryptography is verifying signatures
on plaintext headers and requests.

**Per document it stores:** the current encrypted blob; a monotonic version number;
the header of every version that changed membership, which forms the membership chain; the key
generation counter last seen; pending access requests; and the current lease.

**The allow-list is the current header's recipients.** The server keeps no separate
list: the devices allowed to read and write are exactly the `{member id, device
fingerprint, verify key}` entries of the latest accepted header. Every accepted header
has already passed the chain rule, so a grant, link, unlink, removal, or leave takes
effect on the server in the same upload that makes it — there is no second step to
fail, and no window in which a removed person can still fetch. Separately, short-lived pairings, which belong to no document.

**The interface it must present** — shape rather than a wire specification, which the
implementation plan carries:

| Endpoint | Purpose |
|---|---|
| `POST /documents` | Register a newly shared document under its client-generated id; refused if the id is taken, in which case the app re-registers under a fresh id; rate-limited and capped per device key |
| `GET /documents/{id}/head` | Version, key generation, lease holder, and the current header |
| `GET /documents/{id}/headers?after={base}&through={version}` | The membership-changing headers newer than `base`, then the header of `version`, in order; readable by current devices, and by a former device only through the header that removed it |
| `GET /documents/{id}` | The current version number, its header, and its blob, in one response |
| `PUT /documents/{id}` | Upload, conditional on the version the client started from |
| `POST` and `DELETE /documents/{id}/lease` | Take, refresh, release the edit lease |
| `POST /pairings` | A new device or invitee posts its commitment and device name; returns a pairing code; device authentication, rate-limited, expires in 10 minutes |
| `GET /pairings/{code}` | Read a pairing's current state; the two parties only |
| `PUT /pairings/{code}/responder` | The existing device or granter posts its public keys; member authentication; written once |
| `PUT /pairings/{code}/reveal` | The new device or invitee reveals its keys and random value; written once |
| `PUT /pairings/{code}/handover` | The existing device posts its signed handover — member id, display name, and wards; written once, and completing it is what permits the new device to be added under that person |
| `POST /documents/{id}/requests` | Ask for access; device authentication, the only write open to a non-member |
| `GET` and `DELETE /documents/{id}/requests` | List and clear, for members |
| `GET /documents/{id}/requests/mine` | The requesting device's own request status — pending, granted, or withdrawn |
| `GET /me/documents` | The documents whose current header lists this device, so a reinstall restores the ward list |

**A download is one consistent snapshot.** The version, its header, and its blob come
back in a single response, and the header chain is requested up to that same version, so
a publish landing mid-download can never pair a header with a different version's blob.
A payload hash that does not match therefore always means something is wrong.

**Two authentication levels.** *Device* authentication proves only that the caller holds
a key; it is enough to list the documents that key is on, to check one's own access
request, and to start a pairing or post an access request — all things a device does before it is
a member. *Member* authentication also requires the device to be in the document's
current header, and every other document endpoint requires it.

**Authentication.** The client fetches a challenge, signs it under the `auth` tag with
its Ed25519 key, and receives a short-lived token; every mutating request is
additionally signed under the `request` tag, with the device's own key, over the
endpoint's route, the session token, the document id (empty for a pairing), and a
`wp-request` BLAKE2b of the request's canonical body. A token taken from a device is
then not enough to change anything, and a captured signature cannot be replayed against
a different document, a different endpoint, or in another session. The canonical bodies are part of the protocol
(`ServerContract.cpp`); an upload's body is its base version and its header's signed
bytes and signature, which bind the blob through the payload hash. Posting an access request is the only write open to a non-member — by
definition, since it is how someone asks to become one — and it still carries device
authentication, which is what rate limiting counts against.

**Rules the server enforces.** Version and key generation never decrease. Writes are
conditional on the client's starting version. Only devices in the current header read
the blob or write. Devices that were once in a header may still read the header chain,
so a removed device's app can fetch and verify the headers that removed it. Every
uploaded header must extend the chain under the same rule clients apply — so a header
that grants, removes, or changes a role when its writer is not an admin, adds a device to
another person, or drops another person's device when its writer is not an admin, is
refused. **A device joins only a document it asked to join.** The server accepts a header that adds
a device only when that device has a pending access request for this document, or has
completed a pairing as initiator with a device of the person it is added under — which
stays valid for adding that device under that person in any of their wards. The first
header of a new document, listing only its creator, is exempt.
Anyone who has seen a header knows a device's public keys, so without this a stranger
could create a look-alike ward, add another member's device to it, and read whatever that
member typed into it. `/me/documents` therefore lists only documents a device genuinely
joined, and a reinstall restores them without asking. These checks are signature verifications
over plaintext headers; the server still never touches the payload or any key material.
Blob size is capped and requests are rate-limited.

**What the server cannot check.** It cannot decrypt, so it cannot verify that the
header's recipients match the roster inside the payload. Clients check it on every open
and download and refuse a version where they differ.

---

## Local-first sync

**Saving and publishing are separate operations on separate clocks.** This is the
central mechanism, and most of the behaviour below follows from it.

- **Save** writes the local file, debounced. It never touches the network and never
  fails for a network reason.
- **Publish** uploads the file conditional on the version this session started from.
  It may fail, retry, or sit queued for hours without blocking anyone's work.

**The local record**, kept per document id alongside the key-generation high-water
mark, holds the server version this file was last reconciled with and the local save
counter at that moment. The base version tells "the server moved"; a local file whose
save counter is above the recorded one tells "I moved" — it has unpublished changes.
Both are exact, neither relies on timestamps, and because they are stored, a pending
upload survives closing the app.

**The base copy.** Beside the local file, the app keeps the file exactly as it stood
at the base version — written whenever a download replaces the local file or a publish
succeeds. Its payload hash is recorded in the local record, and a base copy that does
not match is ignored. The base copy is what lets a merge tell *what changed here* from
*what changed there*, including deletions, without timestamps on any record.

**Sync state is a quiet, persistent indicator**, not a dialog: *Up to date*,
*Changes not yet uploaded*, *Server unreachable*, *Read-only, another member is
editing*.

**On open.** Authenticate, fetch the head. If the server is ahead and nothing local is
unpublished, download it, validate it completely, and only then replace the local
file. If nothing has moved, open the local
file. If both have moved, the same route as a refused publish: validate, adopt, and merge,
with the conflict dialog only where the merge cannot decide. If the server is unreachable, open the
local file, say so in the indicator, and reconcile when it returns.

**The lease keeps one writer of the directory at a time while the server is
reachable.** A member opening for edit takes a server lease with a TTL, refreshed by
heartbeat, released on a clean close; a stale lease can be taken over. The lease covers
families, ministering, and ward details. **Roster changes need no lease**: each is
applied on top of the server's current version and published, so no one can hold off a
removal, an unlink, or a grant by keeping the lease. This is what makes reconciliation of those unnecessary in ordinary use, which is
nearly all of the application's life: one person edits, everyone else reads, and no
version of them ever has to be merged with another.

**Emergency response records need no lease.** Contact status, contact attempts, tasks,
assignments, and notifications can be recorded by any member at any time, online or
offline. An emergency is when several responders work at once, and the response merge
below is built for exactly that, so the lease would only force all but one of them
into read-only — or teach them to disconnect in order to record a contact. A response
edit publishes like any other; a refusal takes the refused-publish route and merges.

**While an emergency is active, apps check for newer versions often** — every 30 seconds
rather than the ordinary slow interval — and fetch or merge what they find, so connected
responders see each other's records within about half a minute.

Offline, a member edits anyway, because that is the whole point of the local file. The
lease is a server construct and cannot speak for a disconnected device, so the promise
it makes is the one it can keep: while the server is reachable, one writer of the
directory.

**Read-only is enforced in `DocumentManager`, before anything changes**, not by
disabling widgets. Failing to hold the lease is the only source of read-only once the
archive overlay is gone, and it never blocks a response edit. A directory edit, undo, or
redo in a read-only session is refused at its entry point, before the document or undo
stack moves, and the user is told another member is editing the directory; the single
write method checks again as a backstop. Refusing only at the write would let the screen
show an edit that is never saved — or saved later over a newer version once the lease
frees. The old overlay shows the other failure: it gated `executeCommand`, `undo`, and
`redo`, left everything else to scattered UI checks, and none of `EmergencyManager`'s
mutators consulted it at all.

**A read-only session stays current.** It checks for newer versions on the ordinary
interval — every 30 seconds during an emergency — and fast-forwards to what the lease
holder publishes; it has no directory changes of its own, and its response changes
merge. **Replacing the in-memory document clears the undo stack**, so an undo is never
applied to a version it was not recorded against.

**Automatic updates are directory edits, and follow the lease.** Geocoding and ward and
stake lookup change families and ward details on their own, and they run the moment the
network returns — exactly when a responder reconnects. So they start only on a device
that may write the directory: an unshared document, or a shared one whose lease this
device holds. Results arriving after that stops being true are discarded, and families
still missing coordinates are picked up by the next batch on a device that holds the
lease. A responder's device therefore carries only response changes, and its merges stay
silent.

**A download replaces nothing until it validates.** The same checks apply wherever the
local file would be replaced by a server version: the header still includes this
identity, the header chain from the base copy's header holds, the key unseals, the
payload hash matches, the payload decrypts, the signature verifies against the
roster, and the key generation passes the local high-water mark. The chain starts at
the base copy — the last version this machine and the server agreed on — because local
saves never reach the server and so are not in its chain. Without
a usable base copy the chain check is skipped for a version this identity can decrypt,
since decryption and the signature check it directly. The operator is in
the threat model, and the local file is the one copy that has to survive a server
serving something wrong; any failure keeps it untouched and is reported as a server
fault.

**A refused publish always takes the same route.** The server refuses only when its
version has moved past this file's base version, so someone else's content is newer —
and a revocation is itself a new version, so a moved key generation always arrives
this way. The app:

1. Downloads their version and validates it completely before anything is replaced.
2. Takes their data key, key generation, and roster exactly. A session's local file
   never holds an unpublished roster change — every grant, link, unlink, removal, role
   change, and leave is built on the server's version and published on its own (see
   *Revoking access*) — so there is nothing of ours to carry over, and theirs is the one
   consistent with the server's allow-list. Two admins granting at the same time both
   succeed because each grant is re-applied on top of the other's when refused.
3. Carries this session's content changes through the ordinary paths: the silent
   merge, or the conflict dialog when both sides changed the directory. Nothing
   publishes this session's payload over theirs directly.

**If a newer version removed you, the local copy is deleted.** When the current header
carries no entry for this identity, or the server refuses this key at the allow-list,
the app fetches the headers after its base version and walks the chain from the base
copy's header, whose hash it recorded when that version was agreed. With no usable
base copy there is no trusted starting point, and nothing is deleted. If
every link holds and the chain reaches a header with no entry for this identity, the
removal is proven: the app deletes the local file and every recorded copy of it still at
its path, records that it was removed from this document, discards the in-memory session,
removes the ward from the list, and tells the user their access was removed — naming
the member when their name is in the roster it holds.

Anything short of that deletes nothing. A broken link, a missing header, or a chain
that still includes this identity is reported as a server fault, and the local copy
stays.

This is cleanup rather than enforcement, and the user guide says so: a copy taken out
of the app beforehand, or a machine that never connects again, keeps what it had.

**One write method.** Both remaining write paths run through it:

| Path | Reached from |
|---|---|
| `saveDocument` and `saveDocumentAs` | `autoSave()` |
| `saveDocumentOnly()` | `setEmergencyResponse`, called by every `EmergencyManager` mutation |

`saveDocumentOnly()` calls `JsonService::saveDocument` directly and deliberately skips
dirty-state tracking; that semantic is preserved, but the debounce, lease check, and
publish scheduling move above it.

**Debounced saving.** The real write churn is `EmergencyManager::persistResponseData()`,
called from 15 sites: every contact-status change, contact attempt, task
add/update/remove/resolve/reopen, assignment, and notification. During a live
emergency, the highest-concurrency moment this feature exists for, every button press
rewrites and re-encrypts the whole file. `autoSave()` additionally fires from
`onWardLookupComplete` and `onStakeLookupComplete`. (Geocoding is already fine:
`onFamilyGeocoded` is a silent update and only `onGeocodingFinished` saves.) The
debounce writes after a few seconds of idle, plus an immediate save on close.

**Ctrl+S forces an immediate flush** — a local save and a publish attempt. It is
currently a no-op when a path exists, which stops being acceptable once saves are
deferred.

**All writes go through `QSaveFile`**, so a crash mid-write leaves the previous file
intact.

### On conflict

Both sides changed the directory — the one case the merge cannot decide. By the time the
dialog appears their key, generation, and roster are already adopted and the emergency
response already merged; the choice is only whose directory to keep. The app names who
saved and when, taking the name from the header's last-writer fingerprint looked up in
the roster, then offers:

1. **Save mine as a copy** (default — destroys nothing): writes this session's version
   as a local copy, then takes theirs. The copy is never uploaded.
2. **Keep theirs**: takes their directory.
3. **Keep mine**: publishes a new version on top of theirs with this session's
   directory, the merged response, and the adopted roster and key — so it passes every
   server rule and never restores anyone they removed.

The internet is up whenever this dialog appears, so the two people can talk and settle
anything finer by hand.

**Recovery copies** are written to the user's Documents folder, named as artifacts,
`Tanglewood Ward (unsaved changes 2026-08-30 1441).wardplanning`, with a fresh document id.
They land somewhere visible precisely because ordinary documents do
not: a recovery copy is an artifact to be dealt with, not a document the app manages.
They are sealed to the **full current roster** — after a refused publish, the roster
adopted from the server — since they are never published, so re-keying them to one
person would buy nothing and would make the copy unreadable if that machine is the
thing that failed.

**Merging a recovery copy is manual**, and the wording says so rather than implying
the files can be rejoined. Because documents are opened from the ward list rather than
a file browser, taking one in is a deliberate act: **Open a copy** reads the file,
says whose roster it carries and when it was written, and opens it read-only. Writing
a copy back over a shared document is not an available move, which is what keeps a
stale roster from locking a bishopric out.
### The emergency response, and a network that is down

An emergency is the one time several people need to record work at once, and the one
time the network is least likely to be there. Those two facts arrive together, so the
design handles them together.

**Every responder records, connected or not.** Response records need no lease, so
each responder's device records contact results, task updates, and assignments into its
own local file and publishes when it can. Connected, others' records arrive within about
half a minute; disconnected, they merge on reconnect.

**Work is partitioned by convention, not by enforcement.** A response is already
organised by `ResponseArea` and `Team`, with tasks assigned to a team or a person, so
responders are working different families to begin with. The app does not restrict a
device to its assigned area — that would be UI and explanation spent on a rule people
already follow, and it would block the person who steps in to cover someone else.

**When two versions meet, response records merge three ways, silently.** The app compares its
own version (*mine*) and the server's (*theirs*) against the base copy (*base*), record
by record, re-encrypts the result, and uploads conditionally. Nothing is left for a
user to decide at two in the morning.

| Data | Rule |
|---|---|
| Any record or field changed on one side only | That side's value, including a deletion |
| Changed on both sides to the same value | That value |
| Contact attempts | Keyed by `ContactAttemptId`; every attempt from either side is kept, and an attempt edited on one side keeps that edit |
| A single value changed differently on both sides — a family's contact status, a task field, the response's name or start or end time | The reconnecting device's value |
| Task categories | A set: a category added on either side is kept, one removed on either side is removed |
| An emergency started on one side only | Kept |
| An emergency started independently on both sides | One response: records merged by family, name and start time from the server's side |
| An emergency ended on one side while the other recorded to it | The response is kept, with the recorded work; ending it again is one click, and losing contact records is not recoverable |

Because every attempt survives and only single values can be superseded, **the record
of who was contacted and when survives overlap intact.** That is the part a ward has
to be able to trust afterwards. "The reconnecting device wins" needs no clocks, and
responders' devices cannot be relied on to agree about the time.

**Which path a refused publish takes is decided against the base copy.** Everything
outside the emergency response is compared on each side:

- **At most one side changed it** — typically the lease holder edited a family while a
  responder recorded contacts — so that side's version of it is taken, and the response
  records merge as above. Silent.
- **Both sides changed it** — two people edited the directory, one of them while
  disconnected — so the ordinary conflict dialog runs. Nobody imports a ward directory
  during a disaster, so this is close to hypothetical.

Without the first rule, a responder's contact record would push the lease holder into
the conflict dialog over a change that does not conflict with theirs at all.

**What this does not do.** While the network is down, responders cannot see each
other's work. The server relays nothing it cannot reach, and each device holds its own
partial picture until connectivity returns. Cell data and tethering often survive when
home internet does not, and ward emergency plans already assume radios and runners for
coordination. Peer-to-peer sync over a local network would genuinely close the gap and
is a substantial subsystem of its own; it is named here as possible future work rather
than folded in.

---

## Where documents live

The app owns the file. Documents are stored in the application data directory, named
by document id, and **the user never chooses or sees a path in the normal flow.**
"Open" is a list of the wards this identity has access to, not a file browser. This is
the largest of the ease-of-setup simplifications, and it is available only because the
server gives a document an identity independent of where its bytes sit.

**The list comes from the server**, cached locally so it is there offline. A member
who reinstalls, or whose application data is cleared, signs nothing and configures
nothing: the same credential-store identity asks the server which documents it is on,
and the app downloads them again. Re-downloading is the ordinary cost of a document
having changed, which happens anyway.

`ensureFilePath()` and the untitled-document dance go with it. A new document is
created in app data under a fresh id, and naming it becomes a display concern.

Two escape hatches remain, and both earn their place for reasons unrelated to the
server:

- **Save a copy** writes a `.wardplanning` file wherever the user likes, under a
  fresh document id. People want backups.
- **Show in folder** reveals the app data directory, for support and for the rare
  hand-off.

## If the server goes away

The server could stop, whether abandoned, unaffordable, or deliberately retired. This
design does not build a second sharing transport against that possibility. **Ward
Planning is open source, so a maintainer can adapt it**, and a rescue path used once,
in a scenario that may never arrive, should not shape the interface every user touches
weekly.

What the design does keep are the three things that make such an adaptation possible,
each of which costs nothing to hold on to:

1. **The document is self-sufficient.** The roster, key generation, and save counter
   live in the file, and the server never becomes the sole authority for any of them.
   Conflict detection already requires this, so it is not a concession.
2. **The format is documented publicly** in the repository, as a specification rather
   than only an implementation. This is what makes "someone could adapt it" true
   rather than aspirational.
3. **Every member holds a complete, readable copy**, and Save a copy gets it out of
   the app.

**A successor restores the service rather than members detaching from it.** Because
documents carry only their id and the app supplies the server address, continuity is
an operator task that members never see:

- **Same domain:** the successor deploys the open-source server at the existing
  address from the export of blobs and header chains, and apps reconnect unchanged.
- **New domain:** the successor also ships an app release pointing at the new
  address, and every existing document follows it.

Members keep editing their local copies throughout, and sharing resumes once the
service returns.

---

## Sharing a document

A new document is local and private. **Share** registers it with the server: the app
posts the blob under the document's existing id; its header, listing only this person's
device, is the first allow-list. What the user gets back is an invite link
carrying the id, which is not secret, since it grants no access on its own.

## Granting access

**By request.** Someone opens an invite link, or opens a `.wardplanning` file that
carries a document id. The app offers "Request access", and one click posts a request
carrying this device's public keys, the person's id and display name, and the device
name. The next admin to open the document sees the request, grants it, and the app adds
a new person, as a member, with that one device — sealing the data key to it, adding roster and header
entries, and uploading, which puts the device on the server's allow-list in the same
step — and clears the request. The person's
other devices join later by linking, with no further grant.

**Every change to who has access is announced.** When a member's app sees versions newer
than the last it announced, it shows **one** dismissible notice for all of them — *3
changes to who has access* — expanding to the list: *Bob Wilson added Tom Hale*, *Bob
Wilson removed Tom Hale*, *Tom Hale left*, *Bob Wilson made Tom Hale an admin*. Only
changes to people are announced; linking and unlinking a person's own devices shows in
the access list instead. Announcements are tracked per document by the last version
announced, so someone granted again after leaving is announced again. This is what makes
a hostile admin's actions visible within minutes rather than silently effective.

**A request whose display name matches a current member** is flagged in the grant
dialog — *Bob Wilson already has access. If this is Bob's new phone, Bob should link it
from his other device instead.* — because granting it would make a second Bob.

The requester's app shows a **pending state** — the request is with the server,
waiting on an admin to open the document — and notices when access is granted.
Without it, a volunteer told "install this and you'll see the ward list" is left
staring at a failure.

**Remote requests are not verified, by choice.** Anyone holding an invite link can post a
request bearing any display name and device name, and links get forwarded more broadly
than intended — and a hostile server could swap the key inside a genuine request. An
admin who grants such a request gives its sender full access. Verifying a remote
request would mean comparing a code over a separate channel, and that friction is judged
too high for the primary goal of sharing in a couple of minutes, so it is accepted. What
remains in its favour: only admins grant, the grant dialog flags a name that matches a
current member, every grant is announced to all members, and a wrongly granted person is
removed by any admin at once. When the invitee is in the room, the
in-person path below compares a number and closes this. The server rate-limits requests
so the list cannot be flooded.

**In person.** The invitee and the granter pair exactly as two devices do when linking:
the invitee's app posts a commitment and shows a pairing code, the granter enters it in
the grant dialog, and both screens show a six-digit number to compare before the grant
is written. It authenticates a *public key* and never carries the data key. Nothing
self-adds a person: granting is always an action taken by an admin.

**Access has to be granted before it is needed.** Both paths run through the server,
so a bishopric that has not onboarded everyone cannot add anyone once the network is
down — which is exactly when the document matters most. The user guide says this
directly, and the single key holder notice is the standing nudge toward it.

## Revoking access

**Any admin removes a person, alone and at once.** Remove on a person in the access list
takes away every one of their devices: the app removes their roster record and header
entries, generates a fresh data key, re-encrypts, re-seals for everyone remaining, bumps
the key generation, and publishes. The accepted header is the new allow-list, so the
removed person's devices are refused from that moment. No one else approves, and nothing
waits. The removal is announced to every member.

**An admin removes one lost device the same way** — a person's stolen phone while they
keep their laptop — which also rotates the key. The person can do it themselves from any
other of their devices (see *Identity*).

**Admins manage roles.** Any admin promotes a member to admin or demotes an admin,
including another admin and themselves. Every version that lists anyone lists at least one
admin, so the last admin cannot step down while anyone else remains.

**Leaving needs nobody.** Confirming Leave hides the ward at once and publishes the
version that drops this person — all their devices, off the roster and the admin list.
Once that version is accepted, this device deletes its copies the same way a proven
removal does; the leaver's other devices delete theirs when their chain check proves the
leave. It does not rotate the key: the leaver would generate the new key and so know it,
so a rotation would protect nothing against them; they leave by choice, and the server
stops serving them the moment the leave lands. When the server is unreachable the ward
stays hidden and the version waits, like any publish.

**Every leave works the same way, including the last person's.** When nobody else remains,
the published version lists no one: nobody can read or write the ward again, and the
server lists it for no device, so it does not come back after a reinstall.

**The last admin names a successor before leaving.** A ward with members and no admin
could never grant or remove anyone again, so when the last admin presses Leave while
others remain, the app asks *Before you leave, choose who will be admin*, and one
confirmation publishes the promotion and then the leave. If a leave is refused because
this person has meanwhile become the last admin — two admins leaving at once — the ward
reappears with the same question.

**The dialog says what revocation does and does not do.** Future versions are closed
to that person the moment the upload is accepted, and their app deletes its local
copy the next time it connects. A copy they took out of the app beforehand, or a
device that never connects again, keeps what it had.

## Single key holder notice

While the roster holds exactly one person, the app shows a **persistent, dismissible
notice**, not a per-save interruption: *Only you can open this document. If you lose
your devices, it cannot be recovered. Add someone now.* A person with a single linked
device also sees a gentler suggestion to link a second one.

**While the roster holds exactly one admin** and anyone else, admins see a second
persistent, dismissible notice: *You are the only admin. If you lose your devices, nobody
can add or remove people. Make someone else an admin.*

Every ward starts in these states and some will stay there, so a warning that fires on
every save would be trained away precisely where it matters most.

---

## Emergency archives are removed

`EmergencyManager::endEmergency` currently snapshots the whole document plus the
emergency response into a dated file under `<name>_archives/`, browsable read-only
and reopenable as active. **That feature is removed as part of this work.**

Each archive is a complete copy of the ward directory, sealed to the roster as it
stood when the archive was written. Keeping them means granting access has to re-seal
every archive and revoking has to re-key every archive. The feature's value, a record
of a past emergency, does not carry that weight.

`endEmergency` becomes "clear the response data". The archive browser, the read-only
archive overlay in `DocumentManager`, and the archive checks scattered through the
views all go with it. Removing the overlay is a simplification this design wants
anyway: read-only becomes purely a matter of holding the lease, enforced in one place.

**The summary report becomes the record of an emergency.** Ending an emergency today
offers a "Generate summary report (PDF)" option alongside the archive choice. With
archives gone that report is the only thing that outlives the response, so it stays
and gains prominence: the three-button dialog collapses to a confirmation whose
report option is offered clearly rather than as an afterthought. The report is a
plaintext PDF written wherever the user chooses, which is worth saying in the same
breath.

---

## Product rename

The application becomes **Ward Planning**, and its documents become `.wardplanning`.
Encryption changes the format already, so identity and format change together and
compatibility breaks once.

The extension still appears in `src/main.cpp`, `DocumentManager.cpp`,
`MainWindow.cpp`, `installer.iss`, `packaging/macos/Info.plist`, `.gitignore`, and
`docs/USER_GUIDE.md`, though with documents kept in app data it now governs exported
copies, recovery copies, and file association rather than an everyday Open dialog.
Separately, the product identity, meaning `setApplicationName`, `setOrganizationName`,
the macOS bundle identifier in `CMakeLists.txt` and `Info.plist`, the CMake project
and target names, the vcpkg port name, and the installer's product strings, changes
with it.

`setOrganizationName` is currently empty, which matters beyond cosmetics: it is what
keys per-user storage locations, and both the key generation high-water mark and the
per-document base version are records that must land somewhere stable.

**`maybeRenameForWard()` becomes a display concern.** It currently renames the file on
disk once the ward is identified, so `Untitled.wardplanning` becomes
`Tanglewood Ward.wardplanning`. With documents stored by id there is nothing to
rename: the ward name is a label carried by the document and shown in the ward list,
set at the same moment as before, while the bytes stay where they are.

---

## Failure paths

Each gets its own message:

| Condition | Message |
|---|---|
| No identity on this device | Ask whether they already use Ward Planning elsewhere; link, or request access |
| Pairing reveal does not match its commitment, or the numbers differ | The devices could not be paired safely; start again |
| No entry for this key | You do not have access, or it was removed |
| Header unreadable | File is damaged |
| Payload authentication fails | File was altered or truncated |
| Key generation below the local high-water mark | This file is an older version; do not use it |
| Keystore unavailable | Name the platform store and how to unlock it |
| Server unreachable | Working from your local copy; changes upload when it returns |
| Verified revocation of this identity | Your access to the ward was removed by *name*; the local copy has been deleted |
| Verified leave by this person, seen on their other device | You left this ward; its copy on this device has been deleted |
| Header signature fails verification | The server returned a version that could not be verified; your local copy is unchanged |
| Upload rejected on version | Someone else saved first, so the conflict dialog |
| Lease held by another member | Directory read-only, naming who holds it; emergency response still editable |
| Header recipients and roster disagree | Refused like any invalid version: the server's copy could not be verified; your local copy is unchanged |
| Invite link unknown to the server | The document was deleted, or the link is wrong |
| Server version written by a newer app | This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work. Directory read-only; emergency response still editable locally. |
| Document file missing from app data | Name the ward and offer to fetch it from the server |
| Opening a copy of a document this device was removed from | Your access to this ward was removed, so its copies cannot be opened here |
| A member tries to grant, remove, or change a role | Only an admin can change who has access; name the ward's admins |
| The last admin tries to leave, or be demoted, while others remain | Make someone else an admin first, so the ward can still add and remove people |

## Debugging

`toJsonString` stays, with a developer-only "export decrypted JSON" action, off by
default, which warns that it writes unencrypted member data. `parseJson` and
`toJsonString` remain plaintext-only and serve this path.

## Component changes

| Component | Responsibility |
|---|---|
| `KeyStore` | qtkeychain wrapper; store, load, and test for this device's key record |
| `IdentityService` | This device's keypairs and name; the member id and display name it belongs to; creating a person or linking to one |
| `DocumentCrypto` | Header parse and build, AD binding, header signing and verification, seal and unseal the data key, encrypt and decrypt the payload |
| `JsonService` | Encrypted read and write; signatures change (below) |
| `ServerClient` | HTTP, challenge-response authentication, typed results per endpoint |
| `SyncService` | Base version, publish queue, sync state, lease heartbeat, reconnect reconciliation |
| `DocumentManager` | Single write method owning debounce, lease, generation adoption, and conflict handling |

There is deliberately **no storage abstraction with local and remote implementations**.
The local file is always the local file, and the server is a separate concern layered
above it; a polymorphic store would model a choice this design does not make.

`JsonService`'s entry points cannot stay as they are. It is a stateless static class
whose `JsonResult` carries only `success`, `errorMessage`, and `document`. Encryption
needs identity and key material in, and the parsed header, meaning save counter, key
generation, roster, and document id, out, because conflict detection lives in
`DocumentManager`. `saveDocument` needs an expected-counter argument and returns the
new counter. The implementation plan should carry the concrete signatures.

## Testing

**Prerequisite: there is no test harness yet.** `tests/` holds only `CMakeLists.txt`
with every `TEST_SOURCES` entry commented out, so `if(TEST_SOURCES)` is false and no
test executable is built. Standing that up is the first step of implementation, and
there are no existing fixtures to convert.

Headless tests in `tests/services`:

- `DocumentCrypto` round-trip
- two consecutive saves of an unchanged document produce different nonces, and so do
  two saves made at the same save counter, the case a counter-derived nonce would
  break
- associated data built from a header that has been through `toJson`/`fromJson`
  matches the original, so a serialisation round-trip never breaks authentication
- mutating any bound header byte causes decryption to fail, including the document id
- a header with an unknown format version, an unknown recipient `kind`, or an unknown
  payload version is refused whole; the directory is read-only and nothing publishes,
  while emergency response edits save locally; the state clears as soon as the server
  again serves a version the app understands
- a header or payload with a missing or malformed required field is refused, never read
  in part
- a file presenting a key generation or save counter below the local high-water mark is
  rejected
- a header whose key generation skips a number, rather than staying equal or rising by
  one, breaks the chain
- the high-water mark survives clearing application data and reinstalling, and a proven
  removal does not erase it
- a header signature verifies against the writer's roster entry, and fails when any
  bound header byte changes or when the signer is not on the roster; a payload byte
  change fails the payload hash
- a header chain verifies link by link; it breaks on a writer absent from the previous
  header's recipients, a bad signature, a key generation that does not step by exactly
  the rule, or a save counter that does not increase; a chain whose revoker was granted
  after the anchor verifies
- grant and revoke changing the roster; rotation making a removed key fail
- a chain breaks on a device added to a person by any device but that person's own —
  an admin's included — on a device dropped from a person by a device that is neither
  theirs nor an admin's, on a person's last device dropped as an unlink, on a device
  fingerprint listed under two people, and on a device moved between people; a person
  linking and unlinking their own devices holds, and an admin dropping someone's lost
  device holds
- a chain breaks on a person added, a person removed, or the admin list changed by a
  writer who is not an admin in the previous header, and on a header that lists people
  but no admin; an admin granting, removing, promoting, and demoting holds, including an
  admin removing another admin
- a person who left or was removed is granted again under their own member id and
  announced again
- a header whose writer keys do not match its last-writer fingerprint is refused on parse;
  a version written by someone who has just left is accepted on download with and without
  a base copy, and reopens from disk afterwards
- a key generation that rises with no device or person dropped, or stays level when one is
  removed or unlinked, breaks the chain
- the last person's leave publishes a header listing no one, after which the server lists
  the ward for nobody
- a pairing whose reveal does not match its commitment is refused; a relay that
  substitutes either device's keys produces different numbers on the two screens; a
  pairing step cannot be written twice
- a relay that serves different pairing contents to successive reads is refused, because
  each side checks against the message it first read
- a leave — a version written by the person it drops, changing nothing else in the
  membership and keeping the key generation — holds, and every remaining member opens
  it; a chain breaks on a writer absent from its own recipients in anything but a leave,
  on a leave that also drops or adds someone else, and on the last admin leaving while
  others remain
- concurrent grants merge rather than dropping one
- two responders working offline both keep their records on reconnect
- overlapping contact attempts are all kept, and an attempt edited on one side keeps
  the edit; a contact status or task field changed on both sides takes the reconnecting
  device's value; a value changed on one side only takes that side's value regardless
  of reconnect order
- a task or attempt deleted offline stays deleted; one deleted on the server is not
  restored
- a response-only change merges silently and leaves the other member's directory
  changes untouched; a directory change on one side and response changes on the other
  merge silently in both directions; directory changes on both sides raise the conflict
  dialog
- a member without the lease can record and publish response edits while another member
  holds it; a directory edit without the lease is refused before it changes the document
- a base copy whose payload hash does not match the local record is ignored
- a document is located by its id, so moving or renaming its file does not lose it
- a new document has an id from creation, and sharing it leaves the id, file name,
  and local records unchanged
- Save a copy produces a file with a fresh document id and the original as its origin,
  and Open a copy opens it read-only without touching the shared document
- editing a copy's origin id breaks authentication
- a proven removal deletes recorded exported and recovery copies still at their paths,
  leaves a moved copy alone, and Open a copy then refuses any copy of that document
- Keep mine publishes this session's directory on top of theirs with the adopted roster,
  and a person they granted remains

Against a fake server:

- publishing from the current base version succeeds and advances it
- publishing from a stale base version is rejected, adopts their key, generation, and
  roster, and raises the conflict dialog without publishing this session's payload
- a downloaded version that fails validation replaces nothing
- edits made while unreachable are kept locally and published on reconnect
- a server ahead with nothing local unpublished fast-forwards without a prompt
- both sides moved produces the conflict dialog, and overwrite is refused across a
  generation change
- a revocation proven by the header chain deletes the local file and removes the ward
  from the list, both mid-session and on next open, including when the revoker was
  granted after this member's last sync; a forged link, a withheld header, or the
  server refusing the key without a proving chain deletes nothing
- the server refuses an upload that grants, removes, or changes a role when its writer is
  not an admin
- a removal, an unlink, or a grant publishes while another member holds the lease
- the server refuses a header adding a device that has no pending access request for the
  document and no completed pairing with the person it is added under
- the server's allow-list after any accepted upload is exactly that header's
  recipients: a granted device can fetch immediately, a removed person's devices are
  refused immediately
- a version with a header recipient that has no roster device, a roster device with no
  header entry, or a verify key that differs between them is refused
- clearing local application data restores the ward list from the server, and the
  documents on it download again

---

## Deliberate limits

- **Only the emergency response merges.** Ordinary conflicts are reported and resolved
  by choosing a version; roster grants merge; emergency response records merge three
  ways against the base copy under the rules above.
- **Responders cannot see each other's work while the network is down.** Records
  reconcile when it returns. Local-network peer sync would close this and is possible
  future work.
- **Overlapping response edits settle by rule, silently.** Contact attempts are kept
  in full; a single value changed differently by two responders takes the value from
  whichever device reconnects last.
- **Revocation governs future versions and cleans up the app's copy.** The removed
  member's app deletes its local copy when it next connects; copies taken out of the
  app, and devices that never reconnect, are beyond reach.
- **One admin can remove anyone.** A compromised admin device can remove every other
  person, deleting their local copies. Every removal is announced; recovery is a new ward
  built from the directory PDFs, and what they do not recreate is lost.
- **A stolen, unlocked device acts as its owner**, with its owner's role. Unlinking it
  offers to unlink what it added; otherwise any admin removes it.
- **A leaver knows the data key until the next rotation.** Leaving does not rotate, so a
  version written before the next removal or unlink is readable by a leaver who obtains
  it some way other than from the server, which refuses them — and a leaver colluding
  with the operator could forge versions under that key. Collusion with the operator is
  outside what this design defends.
- **Access must be granted before an emergency**, because granting needs the server.
- **Reports stay plaintext.** `ReportGenerator` writes member-data PDFs to a
  user-chosen path. Out of scope here, but it belongs in the confidentiality story.
- **Emergency archives are gone**, along with the record of past emergencies.
- **The server learns the shape of the access list** and who wrote when, though not
  who they are inside the document or what it says.
- **A hostile server could list a look-alike ward** for a device after a reinstall; a
  linked device's identity and ward list come from its pairing, not the server.
- **A removal deletes unpublished work.** Someone removed while holding emergency records
  they had not yet published loses them; they already hold the data, and the case is rare.
- **Remote access requests are not verified.** An admin can grant a request sent by
  whoever holds a forwarded link; in-person grants compare a number.
- **Sharing depends on the server being up.** Editing does not.
- **A hostile server operator could keep members on separate copies** without them
  noticing; nothing is exposed, and each member keeps their own work.
- **There is no folder-based sharing mode.** Continuity if the server stops rests on
  the project being open source, the format being documented, and every member holding
  a complete file.
- **Operating the server is ongoing work** — uptime, backups with a tested restore,
  certificate renewal, and abuse handling — carried by whoever runs it rather than by
  the ward.
## Earlier versions

Ward Planning reads only its own `.wardplanning` documents and its own settings. Files
and settings from earlier releases are not migrated; an `.emergencyplan` file is treated
like any other file the app does not own.

Startup restores the last document from the ward list by id. Today `MainWindow`
silently clears the remembered document when loading it fails, leaving an empty window
and no explanation; a failure is now reported and the record left alone.

## Identity recovery

Device keys live only in each device's OS keychain. They survive an uninstall and
reinstall; a reimaged profile or a replaced device loses them. **A person with another
working device links the new one** — no one else is involved. **A person who has lost
every device** is removed by an admin and granted again, which re-seals the data key to
the new device and adds it to the server allow-list. The single-holder and single-admin
notices, and the suggestion to link a second device, are what keep a ward out of the
state where neither is possible. There is no exported recovery key file.

A lost device is unlinked by its person from another device, without disturbing their
access from anywhere else.

---

## Hosting notes

Preliminary findings from discussion on 2026-09-13, to seed the server's own design.
Figures marked *verified* were checked against Cloudflare's documentation that day;
the rest are recalled or estimated and need confirming.

### Recommendation

**Cloudflare Workers with one SQLite-backed Durable Object per document.** A Durable
Object is a single-threaded unit with its own storage, which maps directly onto what
this design asks of the server: atomic version and key-generation checks, a lease
with a TTL expired by the object's alarm, and the allow-list and pending requests
stored beside the blob. Patching, TLS, uptime, and DDoS protection belong to the
platform, which suits a server operated by one volunteer. It also runs away from any
local disaster the ward is responding to.

The runner-up is a small VPS running a plain server over SQLite, with Caddy for TLS
and Litestream for continuous backup. It is portable and anyone can self-host it, in
exchange for the operator carrying patching, uptime, and restore testing.

**Reconsidered on 2026-09-24 and kept.** A C++/Qt server would let the client and server
share one implementation of the rules. Checked that day: Cloudflare Containers is
generally available (April 2026), but a container's disk is ephemeral, so a C++ server
there could only be a stateless rules sidecar to a TypeScript Durable Object, adding a
1–3 second cold start to most publishes. A single C++ process on a container platform
with a persistent volume would work, at the cost of rebuilding and patching the image.
Cloudflare Workers was kept for its hands-off operation, and the duplicated rules are
covered instead by the conformance suite described under *Companion work*.

### Tailscale

Tailscale serves the operator, and only for development. Members reach the server
over the public internet with no extra client, because a tailnet would reintroduce a
second access list (a Tailscale account and tailnet invite per member) and could not
receive access requests from people holding only an invite link. Tailscale Funnel can
expose a home machine publicly over HTTPS, which is useful for testing against a real
server; production belongs on infrastructure that stays up when the operator's house
does not.

### Cloudflare compared with a VPS

| | Cloudflare Workers + Durable Objects | VPS + SQLite + Caddy + Litestream |
|---|---|---|
| Reliability | Distributed; rare global incidents, which take much of the web down too | One machine; roughly 99.9%, recovered by the operator |
| Cost | Free tier, or about $5/month paid (recalled) | About $4–6/month plus cents for backup storage (recalled) |
| Maintenance | Deploying code | OS updates, monitoring, backup checks, restore tests — about an hour a month plus incidents |
| Backups | Built-in point-in-time recovery, believed 30 days (recalled) | Litestream to object storage, set up and tested by the operator |
| Portability | Tied to Cloudflare's APIs | Runs anywhere |
| Abuse handling | Rate limiting and DDoS protection included | Built by the operator |

Server downtime stops sharing and leaves editing untouched, so reliability is not the
deciding factor. The choice is maintenance against portability.

### Capacity on the free plan

Free-plan limits (*verified*): 100,000 Worker requests and 100,000 Durable Object
requests per day, reset at midnight UTC; 100,000 SQLite rows written and 5 million
read per day; 5 GB storage total, 1 GB per object; 2 MB maximum row or BLOB size.

Estimated requests for one ward of up to 100 users on a **Sunday**, the peak day, with
weekdays well below it:

| Activity | Requests |
|---|---|
| About 40 opens (auth, head, occasional download, lease) | ~200 |
| Editing sessions totalling 3–4 hours at a 1-minute heartbeat | ~200 |
| Debounced publishes and access requests | ~50–100 |
| **Total** | **~500** |

- **Free plan: roughly 200 wards.** Limits are daily, so capacity is set by the peak
  Sunday and quiet weekdays contribute nothing. Rows written track requests closely,
  since each heartbeat updates the lease.
- **The UTC reset helps a little.** Midnight UTC falls on Sunday afternoon or evening
  across US time zones, so late Sunday use counts against Monday's quota.
- **Heartbeat interval is the main lever.** A 5-minute heartbeat with a 15-minute
  lease roughly doubles capacity, with the largest effect on Sunday when leases are
  held longest.
- **Storage is ample.** At an estimated 200 KB compressed per document, 5 GB holds
  about 25,000 current versions; retained prior versions reduce that.
- **Emergencies spike.** Twenty responders publishing often could use thousands of
  requests for one ward, and a regional disaster affects many wards at once. An
  exhausted quota fails requests until midnight UTC while every member keeps editing
  locally.
- **Paid plan** usage is believed to be billed monthly (recalled), which averages
  Sunday peaks against quiet weekdays and supports thousands of wards.
- **The 2 MB BLOB limit** is checked against a real document carrying an emergency
  response. A larger payload is chunked across rows or stored in R2.

### Continuity if the operator dies

Running unattended:

- **Billing** sits on the free tier, or on a payment method that outlives the
  operator.
- **The domain** is registered for the maximum term with auto-renew, so the built-in
  address keeps resolving.
- **No scheduled manual chores**: no secrets or tokens that need hand rotation.

**Trust model: hostname plus TLS.** Whoever controls the domain is the server, which is
exactly what lets a successor take over at the same address. Clients authenticate every
blob themselves, so a lapsed domain re-registered by a stranger — most plausibly an
automated drop-catcher parking ads — yields ciphertext and request metadata, and stops
sharing until a successor acts. The one active attack it enables, injecting spoofed
access requests, already exists against the live server through any forwarded invite
link, and is an accepted risk of the remote request path (see *Granting access*).

Handing over:

- **A second Super Administrator** on the Cloudflare account, so access never runs
  through the provider's support process.
- **Credentials** (registrar, Cloudflare, GitHub) in a password manager with emergency
  access.
- **A second owner** on the GitHub repository.
- **A runbook** in the repository covering deploy, data location, restore, and
  migrating to another host or domain.
- **A scheduled export** of all ciphertext blobs and header chains to a second provider
  the successor controls, which is what a restore at a new host starts from. It is
  safe to hold because it is encrypted, and reveals approximate document sizes.

Keeping members current:

- **The app checks for updates and prompts clearly**, because a move to a new domain
  reaches members through a release.

Surviving with no successor:

- Every member already holds a complete, editable local copy and keeps working;
  sharing is what pauses.

---

## Companion work

**The server has its own design and implementation plan.** Where it runs, how it is
backed up and restored, how long prior versions are retained, and what the user guide
promises about all three are settled there. This document fixes only the interface the
client depends on, and the client is built and tested against a fake server that
presents it.

**The fake and the real server are checked against each other.** The server runs
TypeScript on Cloudflare, so its rules — the chain rule with its roles, invitation-only
joins, member-id reuse, the lease — are written a second time, in another language. One conformance suite, written against the client's server
interface, runs against both: against the fake in the ordinary test run, and against the
real server over HTTP. The server's plan treats that suite as its acceptance gate. A
disagreement is resolved in whichever implementation is wrong, never by loosening a test.

**The real server offers a development clock override.** Session, lease, and pairing
expiry can only be tested by moving time forward. The server therefore accepts a clock
offset in local development only — never in a deployed build — so the conformance suite
covers those rules too rather than leaving them to the fake alone.
