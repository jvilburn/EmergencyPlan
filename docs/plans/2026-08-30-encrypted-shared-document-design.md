# Encrypted Shared Document - Design

**Date:** 2026-08-30
**Status:** Design agreed; revised after design review; revised to hold the authoritative
copy on a Ward Planning server rather than in a third-party sync folder; revised on
2026-09-24 to replace two-person removal and the 7-day waiting period with admin and
member roles; revised on 2026-10-11 so the server holds who has access, and encryption
alone keeps it from reading anything.

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

**So the app talks to a Ward Planning server directly.** One access list, held by the
server. Sharing is: share the document, send the link, they install and click Request, an
admin clicks Grant. No provider account, no sync client, no folder path.

**Every member still keeps a complete local file.** It opens, edits, and saves with no
network at all, which is what makes the application dependable during an emergency,
the moment it exists for. The server holds the authoritative copy, in the same
encrypted bytes the local file holds, and can read none of them.

A document that has not been shared is simply a local file. The server enters the
picture when someone shares it.

## Scope

This design covers the encrypted document, who has access to it, safe concurrent use,
and the contract between the app and the server. The server's own implementation,
hosting, and operation are a companion piece of work; what follows fixes the
interface that work has to present.

Two related ideas surfaced during design and are deferred to their own work:

- **Spreadsheet sync** - column-mapped add/remove against an external `.csv`/`.xlsx`
- **Native talks tracker** - speaking history held in the document itself

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
| The split of responsibility | **The server decides who may fetch and publish; encryption decides who can read.** The server holds people, devices, roles, and the access history as ordinary state. The document holds the data key sealed to each device, and only a client adds a device to that list. |
| Key model | Envelope encryption, one data key per document, sealed to each device |
| Key generation | Silent, on first run. No user-visible key material. |
| Server authentication | Ed25519 challenge-response yields a short-lived bearer token |
| Revocation | A server call takes the person or device off the allow-list at once; the next version anyone writes is under a fresh data key sealed only to those remaining |
| What the server holds | Ciphertext, people and their devices' public keys, names, roles, and who published when. No key material. |
| Private key storage | A file in local app data, readable only by its user, holding the secret halves - as SSH keys are kept. Local app data does not roam. |
| Crypto library | libsodium 1.0.22 or later from vcpkg, confirmed to contain commit `ad3004e` |
| Who has access | People. A person is one server record, shared by all their wards, with one or more devices; keys never leave the device that made them. |
| Roles | Each person in a ward is an **admin** or a **member**, held by the server. The ward's creator is its first admin, and a ward with anyone in it has at least one admin. |
| Who may grant | Any admin adds a person; only a person links their own devices |
| Who may remove | Any admin removes a person, alone and at once. A person unlinks their own devices. An admin promotes and demotes, including stepping down while another admin remains. Every change to who has access is announced to all members. |
| Who seals the key to a new device | Only the client that approved it: the admin who granted the request, or the device that linked it |
| Leaving | Any person, at once, by one server call. The last admin promotes someone first. |
| Recovery | Link from another of your devices; with none left, an admin removes the old person and grants you again; persistent notices appear while only one person holds access or only one person is an admin |
| Document storage | One encrypted payload. Merge granularity is a client concern, so storage stays simple. |
| Ordinary concurrency | The lease keeps one writer of the directory and ministering at a time while the server is reachable; access changes never need it |
| Emergency concurrency | Emergency response records need no lease: responders record in parallel, online or offline, and records merge three ways |
| Encryption coverage | Every document; emergency archives are removed |
| File extension | `.wardplanning`, for exported copies and file association |

---

## Threat model

Naming this explicitly, because several design choices only make sense against it.

**What is being protected, and how much.** The document holds a ward's member directory:
names, addresses, phone numbers, birth dates, family relationships, and the ward's own
planning notes. That deserves real protection - it should not sit readable on a server or
in a stray file - but it is not money, credentials, or secrets anyone would mount an
elaborate attack for. Easier routes to the same data already exist: a focused phishing
attack on a ward leader's church website login yields the whole directory with far less
effort than anything this design could be attacked with.

So the defences are proportionate. The design protects against the realistic failures -
a server operator who can read what it stores, a lost or stolen device, a mistaken grant,
a file that leaks - and keeps each defence cheap in code and invisible in use. **Attacks
that need collusion** (two members together, or a member with the server operator) **or a
chain of unlikely steps are accepted, not defended**, and are listed below in a line
each. Where a defence would cost usability or much code to stop such an attack, the
simpler design wins.

**Two kinds of trust, kept apart.** The server is trusted to run the access list: who may
fetch and publish, who is an admin, who did what and when. It is never trusted with the
content. Confidentiality rests on the data key alone, and a client seals that key only to
a device it approved itself or that an earlier version already held. A server that lies
about the access list can therefore deny service, but it cannot read the ward without
someone approving a device it substituted at that moment.

**Defended against.** The server operator reading member data - the operator is the
application's author, and the design keeps that from being a matter of trust. Anyone
who obtains a file without ever having held a key. A removed person fetching anything
newer, which the server refuses at once. A removed person reading a later version
obtained some other way, which is under a key they never held. Silent corruption or
truncation.

**Detected, not prevented.** Tampering with a local file's header, including its
document id - the header is bound as associated data, so a modified local file fails
to decrypt rather than quietly attaching itself to a different document.

**Inside the trust boundary.** Every member can read everything and write whatever they
like into the document. Admins also decide who has access: an admin grants, removes, and
changes roles alone. A ward's admins are its bishopric and clerks - the people who would
hold that authority anyway - and the design trusts them with it.

**Accepted: the server, or a bug in it, can remove anyone.** A removed device deletes its
local copies on the server's word, so a hostile operator or a server bug could tell
members they were removed and their copies would go. The ward's ciphertext and every
copy outside the app survive. This was weighed against keeping a signed chain of every
membership change, which would let a device prove its own removal before deleting
anything, and judged not worth that machinery: a server that wanted to deny service
could already do so by refusing to serve.

**Accepted: one compromised admin device can remove everyone.** An admin device in
hostile hands could remove every other person, and their apps would delete their local
copies. This was weighed against requiring a second admin's approval and judged the
better trade for a ward: the realistic threat is a lost device, not a hostile one, and
what a lost device needs is someone able to remove it quickly and alone. A hostile use
is visible at once - every access change is announced to every member. **Recovery is a
new ward:** someone creates a fresh document, imports the ward directory and ministering
PDFs again, and grants people again; the old ward is abandoned to whoever took it. What
the PDFs do not recreate - tags, teams, emergency assets, special needs, notes, and past
emergency records - is lost. Content vandalism, already possible for any member, is
recovered the same way.

**Accepted: a stolen, unlocked device acts as its owner.** It can read everything, write
content, unlink its owner's other devices, and link devices of the thief's own - and,
on an admin's device, anything an admin can do. Its owner unlinks it from another
device, or any admin removes the owner, who is then granted again. Planted devices are
shown for what they are: the access list says which device linked each one, and
unlinking a device offers to unlink every device it linked.

**Accepted: an approved request or link can carry the operator's key.** The server relays
the new device's public key to the admin who grants it or the device that links it. A
hostile operator could substitute its own key at that moment and receive the data key
sealed to it. Comparing a code between the two screens would close this, and was judged
too much friction for the primary goal; it needs the operator to act at the moment of an
approval, ward by ward, and the substituted device shows in the access list.

**Accepted: a remote access request is not verified.** Whoever holds a forwarded invite
link can put a request in front of an admin under any name; an admin who grants it gives
full access. Name-match flags, grant announcements, and removal limit the damage.

**Accepted: a hostile operator can split members onto separate copies, or serve an older
version.** Every sync indicator would read *Up to date* while members saw different or
stale work. Nothing is exposed and every member keeps their own edits; the gain to an
attacker is disruption only.

**Cleaned up, best-effort.** A removed member's local copy, and every copy the app wrote
from it - exported copies and recovery copies still where the app put them. Their app
deletes them the next time it connects and learns of the removal, and refuses to open a
copy of that document afterwards, which covers the ordinary case of a released leader
who would never think to remove them. A copy moved or sent elsewhere, or a device that
never connects again, keeps what it had.

**Outside the app's reach.** Reports exported by `ReportGenerator`, which remain
plaintext PDFs.

**Known to the server, and accepted.** Per document: the ciphertext and its size, which
leaks approximate ward size; the people with access, by display name, with their roles,
their devices and device names; every grant, removal, role change, and leave; which
device published when, and from what address. The server knows who the bishopric is and
how they use the app, without being able to read the directory. The user guide says so
plainly.

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
- save counter
- payload nonce
- recipient entries, one per device: `{device fingerprint, data key sealed to that
  device's X25519 public key}`

The whole header is the associated data of the payload's AEAD, so anyone holding the
data key detects any change to it on decrypt.

**Parsing is strict; a newer version is refused, never guessed at.** One format
version, in the header, covers the whole file: the header's fields, how the data key is
sealed to each device, and the payload inside. A reader accepts a file only when it
knows that version and finds every required field present and well formed. Tolerant
parsing, which skips what it does not recognise, mostly produces data read wrongly, so
anything else is refused whole. Any change to the header, the sealing, or the payload,
such as a hybrid post-quantum entry, is a new format version, and a reader refuses it
before decrypting anything.

**When a newer app wrote the version on the server**, this app keeps the local copy it
has, with the **directory read-only** and **nothing published**, and a persistent notice:
*This ward was saved by a newer version of Ward Planning. Update to see the latest
changes and share your work.* **Emergency response edits stay available** and are saved
locally, so responders keep working through an update; they merge after the app is
updated. The update notice offers the download. The state is **checked again on every
server check**, so it lasts only while the server keeps presenting a version this app
cannot read. The server supports a format before any app writes it.

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
is shared. Whether a document is shared is a property of the local record - it has a
base version - not of the id.

**Copies get a fresh id and a fresh key, and remember their origin.** An exported copy or
recovery copy is encrypted under a new data key - so nobody who once held the ward's key,
such as someone who has since left, can read it - and carries a new random id the server
has never seen, so a copy can never publish itself into the ward's document, and its
local records never collide with the original's. It also carries the original's id as
its origin, bound in the associated data like every other header field, so the origin
cannot be edited out without breaking the file.

**Copies the app writes are cleaned up with the document.** The app records the path of
every copy it writes, per original document. When a removal is reported, it deletes
each recorded copy still at its path along with the document. A copy the user moved,
emailed, or backed up elsewhere is out of the app's reach, as is anyone decrypting a
copy with their own keys outside the app.

**Payload**

The document's JSON, compressed, encrypted with XChaCha20-Poly1305 under the data key,
as **one payload**. Splitting the document into separately encrypted sections was
considered and set aside: reconciling two versions happens on the model, in a client
that holds the data key, so merge granularity owes nothing to storage granularity. One
payload means one nonce, one version, and one write.

**Nonce.** 192 bits from `randombytes_buf`, generated fresh on **every write**, never
derived from the save counter and never carried over from the loaded file.
XChaCha20-Poly1305's extended nonce exists precisely so random nonces are safe
without counters or stored state. This matters more than it looks: the data key is
long-lived, so a repeated nonce would XOR two versions of the ward directory against
each other and expose the Poly1305 one-time key. Two people saving at the same save
counter is exactly the case a counter-derived nonce would break.

**The sealing set.** Inside the payload, the document lists the devices its data key is
sealed to: each device's X25519 public key and Ed25519 verify key. This list is what
makes re-sealing possible - a fingerprint is one-way, so sealing to the remaining
devices needs their actual public keys - and it is the one piece of access state the
server never controls. **The header and the sealing set must match exactly**: every
recipient entry's fingerprint is computed from exactly one device in the set, and every
device in the set has exactly one entry. A version that fails this is refused like any
other invalid version - only a bug or a tampered app could write one, and an
unattributable key must never sit in the header.

The sealing set carries no names, member ids, or roles; those belong to the server. Its
devices are the only thing a client seals to.

**How the sealing set changes.** Each write starts from the sealing set of the version it
is built on, then:

1. **Prunes** every device the server's current allow-list no longer lists - a removed
   person's devices, an unlinked device, a leaver's. Pruning only ever shrinks the set,
   so a server that lies here can deny someone access but can never gain it.
2. **Adds** the devices this client approved and the server still lists: a person this
   admin granted, by the device that asked, and a device this device linked.
3. **Rotates the data key** when the prune removed anyone. The new key is sealed only to
   the devices that remain.

Nothing else adds a device. In particular a client never seals to a device because the
server lists it - which is what keeps a hostile server from planting one.

**Rotation lands with the next version.** The server cuts a removed device off the
moment it is removed, and the first version anyone writes afterwards prunes it and
rotates. Until that write there is no newer content for the removed person to miss, so
deferring the rotation to it costs nothing. A leave rotates the same way, so a leaver
never holds the key of anything written after they left.

**Every write re-seals.** A save rebuilds every recipient entry from the sealing set
rather than carrying the old ones forward, so a sealed key that was written wrong - by a
bug or a tampered app - stops locking its owner out of every later version. Sealing a
32-byte key to a handful of devices costs nothing worth saving.

**Fingerprints are at least 128 bits:** 16-byte BLAKE2b over the device's X25519 public key
followed by its Ed25519 verify key, personalised `wp-fingerprint` (the string,
zero-padded to 16 bytes, in BLAKE2b's parameter block, with a zero salt). The companion
server reproduces it, and the known-answer vectors pin it. A fingerprint is an identity
handle, not a shortened secret. Because it covers both keys, a device cannot keep its
identity while either key is swapped: a changed key is a different device.

**Post-quantum.** X25519 is exposed to harvest-now-decrypt-later, and hybrid
X25519 + ML-KEM-768 is now in production elsewhere. The exposure needs an attacker
holding the file today and a quantum computer later, against data with a
few-year sensitivity horizon, so this design stays classical. A hybrid entry can be
added later as a new format version, which every member's app picks up by updating.

**Compression before encryption** is safe here - there is no adaptive
chosen-plaintext channel, so CRIME-style attacks do not apply. It does leak
approximate ward size to the server, which is accepted.

**Primitives.** `crypto_box_seal` wraps the data key to an X25519 public key.
Anonymous sealed boxes mean granting access needs nothing from the granter's own key.
Note that sealed boxes use XSalsa20-Poly1305 internally, not the XChaCha20-Poly1305
of the payload; for a 32-byte data key this is immaterial, but it is worth not
mis-stating. `crypto_sign` (Ed25519) signs the server's login challenge.

**Library pin.** CVE-2025-69277 (`ge25519_is_on_main_subgroup` missing a `Y == Z`
check) does not affect any call this design makes. The build requires libsodium 1.0.22
or later regardless - the vcpkg baseline this project started from builds 1.0.20,
which predates the fix - and integration confirms `ad3004e` is an ancestor of the
release used.

---

## Identity

**An identity is a person with devices.** On first run a device generates **two
keypairs**: an X25519 pair that documents are sealed to, and an Ed25519 pair that signs
in to the server. They are created together, stored together as one record in the
identity file, and loaded together. libsodium can derive one from the other, and this
design does not: two independent keys cost nothing and keep the sealing key and the
signing key each doing one job.

**A person is a server record**, shared by all the wards they belong to: a random member
id, a display name, and their devices, each with its public keys, device name, and the
device that linked it. Granting a person a ward gives all their devices access; linking
a device gives it access to all their wards.

The first screen asks one question: **"Do you already use Ward Planning on another
device?"**

- **No** - the device creates a new person: a random member id and a display name,
  editable later. It works offline at once, and registers the person with the server the
  first time it shares a ward or requests access.
- **Yes** - the device joins an existing person by linking.

Asking this first is what keeps one person from becoming two by accident: a second
"Bob" granted from a phone would be a separate person, with none of the first Bob's
devices and a role of its own.

**Linking a device.**

1. The new device posts its public keys and device name, and shows a short link code -
   a lookup handle, not a secret. The code expires after ten minutes.
2. The person enters that code on a device they already use, which shows the new
   device's name - *Link "Pixel 8" to your account?* - and the person confirms.
3. The server adds the new device to that person. The new device learns its member id
   and display name from the server, and the person's wards appear in its list.
4. The approving device remembers the device it linked, and seals the data key to it in
   every ward it holds, each through the background queue: applied on top of the
   server's version and published, with no lease, retrying until every ward has it.
   Until a ward is sealed to it, the new device shows that ward as *Waiting for access
   from your other device*.

The approving device keeps doing this for every ward it later holds, so a ward granted to
Bob after he linked his phone reaches the phone through Bob's laptop. A device sealed
into a ward by another route - a grant that named it - needs nothing more.

**Unlinking a device** - a lost phone, a retired laptop - is done by the same person from
any other of their devices. One server call takes the device off the person, and so off
the allow-list of every ward at once; the next version written in each ward prunes it
and rotates the key. The unlinked device deletes its local copies when it next connects,
and returns to the first-run question. A person's last device cannot be unlinked; that
is leaving. **Unlinking a device also lists every device it linked**, from the server's
record of who linked each - *This phone also linked "Dell Laptop" on 3 September. Unlink
it too?* - with **Unlink both** as the default, so a device planted from a stolen phone
does not outlive the phone.

**Identity outlives a reinstall.** The keypairs live in a file in local app data, which
the uninstaller leaves in place, so a reinstall finds the same device identity and the
same access. Clearing local app data makes the machine a new device, which links again;
the ward list is fetched from the server, so it comes back with it.

There is no passphrase-protected identity export: linking covers new devices, and
an admin's grant covers losing all of them.

`IdentityFile` is a narrow pair - `load` and `store` - with `load` reporting a missing
file distinctly from one it cannot read, so a damaged file never looks like a first run.

**Identity resolves before any document exists.** `main.cpp` constructs
`DocumentManager` before `MainWindow`, and `autoSave()` can fire from a network callback
with no user present. So identity resolution is a startup precondition: until the
keypairs are loaded or created, no document is created and no save is attempted.

**A failed encrypted save is a modal error.** A transient status-bar message would be
overwritten by the next one, and a user would not notice their work had stopped being
saved. A failed *upload* is not an error and does not interrupt: it is a sync state,
described below.

An identity file that cannot be read stops startup with a message naming the file,
rather than minting a second identity over it.

---

## The server

The server holds the access list and the ciphertext. It holds no key material and never
reads the payload; its only cryptography is verifying the Ed25519 signature on a login
challenge, and computing device fingerprints.

**It stores:**

- **People**: member id, display name, and devices, each with its fingerprint, X25519
  public key, verify key, device name, and the device that linked it and when.
- **Per document**: the current encrypted blob and its header; a monotonic version
  number; who published the current version and when; the members, each with a role;
  the access history; pending access requests; and the current lease.
- **Former access**: for each person removed from or leaving a document, and each
  unlinked device, who did it and when - so a device that comes back learns why it was
  cut off.
- **Link codes**: short-lived, belonging to no document.

**The allow-list is the current members' devices.** A device may fetch and publish a
document when it belongs to a person who is a member. A grant, removal, leave, or unlink
takes effect on the server in the call that makes it.

**The interface it must present** - shape rather than a wire specification, which the
implementation plan carries:

| Endpoint | Purpose |
|---|---|
| `POST /auth/challenge`, `POST /auth/token` | Challenge-response sign-in; the token is a bearer credential for every other call |
| `PUT /me` | Register this device's person on first use, or change its display name |
| `GET /me` | This device's person and devices, and the documents they are a member of, so a reinstall restores the ward list |
| `DELETE /me/devices/{fingerprint}` | Unlink one of this person's devices; never the last |
| `POST /links` | A new device posts its public keys and device name; returns a link code; rate-limited, expires in 10 minutes |
| `GET /links/{code}` | For the approving device: the new device's name and keys. For the new device: whether it has been approved, and as whom |
| `POST /links/{code}/approve` | Adds the new device to the caller's person |
| `POST /documents` | Register a newly shared document under its client-generated id, with its header and blob; the caller becomes its first admin |
| `GET /documents/{id}/head` | Version, who published it and when, lease holder, the current header, and the members with their names, roles, and devices |
| `GET /documents/{id}` | The current version number, its header, its blob, and who published it, in one response |
| `PUT /documents/{id}` | Upload, conditional on the version the client started from |
| `POST` and `DELETE /documents/{id}/lease` | Take, refresh, release the edit lease |
| `GET /documents/{id}/history?after={n}` | The access changes after entry `n`, for the notices |
| `POST /documents/{id}/requests` | Ask for access; open to any signed-in device, the only document write open to a non-member |
| `GET /documents/{id}/requests` | List pending requests, for members |
| `DELETE /documents/{id}/requests/{n}` | Decline a request, for admins |
| `GET /documents/{id}/requests/mine` | The requesting device's own request status - pending, granted, or declined |
| `POST /documents/{id}/members` | Grant a pending request, for admins: its person becomes a member |
| `DELETE /documents/{id}/members/{member}` | Remove a person, for admins; or leave, for the person themselves |
| `PUT /documents/{id}/members/{member}/role` | Promote or demote, for admins |

**A download is one consistent snapshot.** The version, its header, and its blob come
back in a single response, so a publish landing mid-download can never pair a header with
a different version's blob.

**A device that has lost access is told why.** Any document call from a device that is
no longer on the allow-list is refused with the reason - removed by whom, left, or
unlinked by whom - and when. That reason is what the app acts on.

**Authentication.** The client fetches a challenge, signs it with its Ed25519 key, and
receives a short-lived bearer token, sent with every call over TLS. A device on a
document's allow-list is a *member* of it; a signed-in device that is not may still read
its own person, start a link, post an access request, and check its status.

**Rules the server enforces.**

- Writes are conditional on the client's starting version, and the version never
  decreases.
- Only allow-listed devices read the blob or write.
- **An uploaded header lists only allow-listed devices.** It may list fewer - a device
  just linked or granted is sealed by the next version its approver writes - but a
  header naming a removed or unlinked device is refused, which is what makes every client
  prune before it publishes.
- Only an admin grants, removes, or changes a role. A ward with anyone in it keeps at
  least one admin: the last admin cannot leave, step down, or be removed while others
  remain.
- A person becomes a member only by an admin granting their request. A device joins a
  person only by a link that person approved, or as the first device of a new person.
- When the last person leaves, the document is deleted.

Blob size is capped and requests are rate-limited.

**What the server cannot check.** It cannot decrypt, so it cannot verify that the header
matches the sealing set inside the payload. Clients check it on every open and download
and refuse a version where they differ.

---

## Local-first sync

**Saving and publishing are separate operations on separate clocks.** This is the
central mechanism, and most of the behaviour below follows from it.

- **Save** writes the local file, debounced. It never touches the network and never
  fails for a network reason.
- **Publish** uploads the file conditional on the version this session started from.
  It may fail, retry, or sit queued for hours without blocking anyone's work.

**The local record**, kept per document id, holds the server version this file was last
reconciled with. The base version tells "the server moved"; a local file whose save
counter is above the base copy's tells "I moved" - it has unpublished changes. Both are
exact, neither relies on timestamps, and because they are stored, a pending upload
survives closing the app. The local record also caches the members, roles, and names
from the last server check, so the access list and notices read correctly offline.

**The base copy.** Beside the local file, the app keeps the file exactly as it stood
at the base version - written whenever a download replaces the local file or a publish
succeeds. Its payload hash is recorded in the local record, and a base copy that does
not match is ignored. The base copy is what lets a merge tell *what changed here* from
*what changed there*, including deletions, without timestamps on any record.

**Sync state is a quiet, persistent indicator**, not a dialog: *Up to date*,
*Changes not yet uploaded*, *Server unreachable*, *Read-only, another member is
editing*, *Waiting for access*.

**On open.** Sign in, fetch the head. If the server is ahead and nothing local is
unpublished, download it, validate it completely, and only then replace the local
file. If nothing has moved, open the local file. If both have moved, the same route as a
refused publish: validate, adopt, and merge, with the conflict dialog only where the
merge cannot decide. If the server is unreachable, open the local file, say so in the
indicator, and reconcile when it returns.

**Every publish applies the sealing-set rule** (see *File format*) against the
allow-list from the latest head: prune, add what this client approved, rotate if anyone
was pruned. An upload refused because its header names a device the server no longer
lists fetches the head again and retries.

**The lease keeps one writer of the directory at a time while the server is
reachable.** A member opening for edit takes a server lease with a TTL, refreshed by
heartbeat, released on a clean close; a stale lease can be taken over. The lease covers
families, ministering, and ward details. **Access changes need no lease**: they are
server calls, and a sealing addition is applied on top of the server's current version
and published, so no one can hold off a removal, an unlink, or a grant by keeping the
lease. This is what makes reconciliation unnecessary in ordinary use, which is nearly
all of the application's life: one person edits, everyone else reads, and no version of
them ever has to be merged with another.

**Emergency response records need no lease.** Contact status, contact attempts, tasks,
assignments, and notifications can be recorded by any member at any time, online or
offline. An emergency is when several responders work at once, and the response merge
below is built for exactly that, so the lease would only force all but one of them
into read-only - or teach them to disconnect in order to record a contact. A response
edit publishes like any other; a refusal takes the refused-publish route and merges.

**While an emergency is active, apps check for newer versions often** - every 30 seconds
rather than the ordinary slow interval - and fetch or merge what they find, so connected
responders see each other's records within about half a minute.

Offline, a member edits anyway, because that is the whole point of the local file. The
lease is a server construct and cannot speak for a disconnected device, so the promise
it makes is the one it can keep: while the server is reachable, one writer of the
directory.

**Read-only is enforced in `DocumentSession`, before anything changes**, not by
disabling widgets. Failing to hold the lease is the only source of read-only, and it
never blocks a response edit. A directory edit, undo, or redo in a read-only session is
refused at its entry point, before the document or undo stack moves, and the user is
told another member is editing the directory; the single write method checks again as a
backstop. Refusing only at the write would let the screen show an edit that is never
saved - or saved later over a newer version once the lease frees.

**A read-only session stays current.** It checks for newer versions on the ordinary
interval - every 30 seconds during an emergency - and fast-forwards to what the lease
holder publishes; it has no directory changes of its own, and its response changes
merge. **Replacing the in-memory document clears the undo stack**, so an undo is never
applied to a version it was not recorded against.

**Automatic updates are directory edits, and follow the lease.** Geocoding and ward and
stake lookup change families and ward details on their own, and they run the moment the
network returns - exactly when a responder reconnects. So they start only on a device
that may write the directory: an unshared document, or a shared one whose lease this
device holds. Results arriving after that stops being true are discarded, and families
still missing coordinates are picked up by the next batch on a device that holds the
lease. A responder's device therefore carries only response changes, and its merges stay
silent.

**A download replaces nothing until it validates.** Wherever the local file would be
replaced by a server version: the header still includes this device, the key unseals,
the payload decrypts, and the header matches the sealing set. Any failure keeps the
local file untouched and is reported as a server fault - the local file is the one copy
that has to survive a server serving something wrong.

**A refused publish always takes the same route.** The server refuses only when its
version has moved past this file's base version, so someone else's content is newer.
The app:

1. Downloads their version and validates it completely before anything is replaced.
2. Takes their data key and sealing set exactly. A session's local file never holds an
   unpublished access change of its own - prunes and additions are recomputed on every
   publish - so there is nothing of ours to carry over.
3. Carries this session's content changes through the ordinary paths: the silent
   merge, or the conflict dialog when both sides changed the directory. Nothing
   publishes this session's payload over theirs directly.
4. Publishes the result under the sealing-set rule.

**If the server says this device lost access, the local copy is deleted.** The app
deletes the local file and every recorded copy of it still at its path, discards the
in-memory session, removes the ward from the list, and tells the user why, naming who
removed them. An unlinked device does this for every ward and returns to the first-run
question. The server's word is enough; see the threat model.

This is cleanup rather than enforcement, and the user guide says so: a copy taken out
of the app beforehand, or a machine that never connects again, keeps what it had.

**One write method.** Both write paths run through it:

| Path | Reached from |
|---|---|
| `saveDocument` and `saveDocumentAs` | `autoSave()` |
| `saveDocumentOnly()` | `setEmergencyResponse`, called by every `EmergencyManager` mutation |

`saveDocumentOnly()` deliberately skips dirty-state tracking; that semantic is
preserved, but the debounce, lease check, and publish scheduling move above it.

**Debounced saving.** The real write churn is `EmergencyManager::persistResponseData()`,
called from every contact-status change, contact attempt, task
add/update/remove/resolve/reopen, assignment, and notification. During a live
emergency, the highest-concurrency moment this feature exists for, every button press
would rewrite and re-encrypt the whole file. `autoSave()` additionally fires from
`onWardLookupComplete` and `onStakeLookupComplete`. (Geocoding is already fine:
`onFamilyGeocoded` is a silent update and only `onGeocodingFinished` saves.) The
debounce writes after a few seconds of idle, plus an immediate save on close.

**Ctrl+S forces an immediate flush** - a local save and a publish attempt.

**All writes go through `QSaveFile`**, so a crash mid-write leaves the previous file
intact.

### On conflict

Both sides changed the directory - the one case the merge cannot decide. By the time the
dialog appears their key and sealing set are already adopted and the emergency response
already merged; the choice is only whose directory to keep. The app names who published
and when, from the server's record of the current version, then offers:

1. **Save mine as a copy** (default - destroys nothing): writes this session's version
   as a local copy, then takes theirs. The copy is never uploaded.
2. **Keep theirs**: takes their directory.
3. **Keep mine**: publishes a new version on top of theirs with this session's
   directory, the merged response, and their key and sealing set - so it passes every
   server rule and never restores anyone they removed.

The internet is up whenever this dialog appears, so the two people can talk and settle
anything finer by hand.

**Recovery copies** are written to the user's Documents folder, named as artifacts,
`Tanglewood Ward (unsaved changes 2026-08-30 1441).wardplanning`, with a fresh document id.
They land somewhere visible precisely because ordinary documents do
not: a recovery copy is an artifact to be dealt with, not a document the app manages.
They are sealed to the **full current sealing set** - after a refused publish, the one
adopted from the server - since they are never published, so re-keying them to one
device would buy nothing and would make the copy unreadable if that machine is the
thing that failed.

**Merging a recovery copy is manual**, and the wording says so rather than implying
the files can be rejoined. Because documents are opened from the ward list rather than
a file browser, taking one in is a deliberate act: **Open a copy** reads the file and
opens it read-only. Writing a copy back over a shared document is not an available move.

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
device to its assigned area - that would be UI and explanation spent on a rule people
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
| A single value changed differently on both sides - a family's contact status, a task field, the response's name or start or end time | The reconnecting device's value |
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

- **At most one side changed it** - typically the lease holder edited a family while a
  responder recorded contacts - so that side's version of it is taken, and the response
  records merge as above. Silent.
- **Both sides changed it** - two people edited the directory, one of them while
  disconnected - so the ordinary conflict dialog runs. Nobody imports a ward directory
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

The app owns the file. Documents are stored in the local app data directory, named
by document id, and **the user never chooses or sees a path in the normal flow.**
"Open" is a list of the wards this identity has access to, not a file browser. This is
the largest of the ease-of-setup simplifications, and it is available only because the
server gives a document an identity independent of where its bytes sit.

**The list comes from the server**, cached locally so it is there offline. A member
who reinstalls signs nothing and configures nothing: the same device identity asks the
server which documents its person is a member of, and the app downloads them again. One
whose local app data is cleared is a new device, which links to their other devices
first. Re-downloading is the ordinary cost of a document having changed, which happens
anyway.

A new document is created in app data under a fresh id, and naming it is a display
concern.

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

What the design does keep are the things that make such an adaptation possible, each of
which costs nothing to hold on to:

1. **The document is self-sufficient for reading and editing.** Its sealing set and data
   key live in the file, so every member can open and edit it with no server. Who may
   change access is server state, and a successor restores it from the server's export.
2. **The format is documented publicly** in the repository, as a specification rather
   than only an implementation. This is what makes "someone could adapt it" true
   rather than aspirational.
3. **Every member holds a complete, readable copy**, and Save a copy gets it out of
   the app.

**A successor restores the service rather than members detaching from it.** Because
documents carry only their id and the app supplies the server address, continuity is
an operator task that members never see:

- **Same domain:** the successor deploys the open-source server at the existing
  address from the export of blobs, people, and memberships, and apps reconnect
  unchanged.
- **New domain:** the successor also ships an app release pointing at the new
  address, and every existing document follows it.

Members keep editing their local copies throughout, and sharing resumes once the
service returns.

---

## Sharing a document

A new document is local and private, sealed only to the device that made it. **Share**
registers it with the server: the app posts the blob under the document's existing id,
and the server makes this person its first admin. The person's other devices reach it
through the devices that linked them (see *Identity*). What the user gets back is an
invite link carrying the id, which is not secret, since it grants no access on its own.

## Granting access

**By request.** Someone opens an invite link, or opens a `.wardplanning` file that
carries a document id. The app offers "Request access", and one click posts a request
carrying this device's public keys, the person's member id and display name, and the
device name. The next admin to open the document sees the request and grants it: the
server makes that person a member, and the admin's app seals the data key to the device
that asked, through the background queue. The person's other devices are sealed by the
devices that linked them, with no further grant.

**Every change to who has access is announced.** Each time a member's app checks the
server, it shows **one** dismissible notice for the changes it has not yet shown - *3
changes to who has access* - expanding to the list: *Bob Wilson added Tom Hale*, *Bob
Wilson removed Tom Hale*, *Tom Hale left*, *Bob Wilson made Tom Hale an admin*. The list
comes from the server's access history, read after the last entry this device showed.
Only changes to people are announced; linking and unlinking a person's own devices shows
in the access list instead. This is what makes a hostile admin's actions visible within
minutes rather than silently effective.

**A request whose display name matches a current member** is flagged in the grant
dialog - *Bob Wilson already has access. If this is Bob's new phone, Bob should link it
from his other device instead.* - because granting it would make a second Bob.

The requester's app shows a **pending state** - the request is with the server,
waiting on an admin to open the document - and notices when access is granted.
Without it, a volunteer told "install this and you'll see the ward list" is left
staring at a failure.

**Remote requests are not verified, by choice.** Anyone holding an invite link can post a
request bearing any display name and device name, and links get forwarded more broadly
than intended. Verifying a remote request would mean comparing a code over a separate
channel, and that friction is judged too high for the primary goal of sharing in a
couple of minutes. What remains in its favour: only admins grant, the grant dialog flags
a name that matches a current member, every grant is announced to all members, and a
wrongly granted person is removed by any admin at once. The server rate-limits requests
so the list cannot be flooded.

**Access has to be granted before it is needed.** Granting runs through the server, so a
bishopric that has not onboarded everyone cannot add anyone once the network is down -
which is exactly when the document matters most. The user guide says this directly, and
the single key holder notice is the standing nudge toward it.

## Removing access

**Any admin removes a person, alone and at once.** Remove on a person in the access list
is one server call: their devices are refused from that moment, and the next version
anyone writes prunes them from the sealing set and rotates the key. No one else
approves, and nothing waits. The removal is announced to every member.

**A person unlinks their own lost device** from any other of their devices (see
*Identity*). An admin removes people, not single devices: a lost phone whose owner
cannot be reached is handled by removing that person and granting them again.

**Admins manage roles.** Any admin promotes a member to admin or demotes an admin,
including another admin and themselves. A ward with anyone in it keeps at least one
admin, so the last admin cannot step down while anyone else remains.

**Leaving needs nobody.** Confirming Leave is one server call; once it succeeds, this
device deletes its copies the same way a removal does, and the leaver's other devices
delete theirs when they next connect. The next version anyone writes prunes the leaver
and rotates the key. Leaving needs the server, like every access change; offline, the
app says to connect first.

**When the last person leaves**, the server deletes the document: nobody can read or
write it again, and it does not come back after a reinstall.

**The last admin names a successor before leaving.** A ward with members and no admin
could never grant or remove anyone again, so when the last admin presses Leave while
others remain, the app asks *Before you leave, choose who will be admin*, and one
confirmation promotes that person and then leaves.

**The dialog says what removal does and does not do.** The person is cut off the moment
the server accepts it, and their app deletes its local copy the next time it connects. A
copy they took out of the app beforehand, or a device that never connects again, keeps
what it had.

## Single key holder notice

While the ward has exactly one person, the app shows a **persistent, dismissible
notice**, not a per-save interruption: *Only you can open this document. If you lose
your devices, it cannot be recovered. Add someone now.* A person with a single device
also sees a gentler suggestion to link a second one.

**While the ward has exactly one admin** and anyone else, admins see a second
persistent, dismissible notice: *You are the only admin. If you lose your devices, nobody
can add or remove people. Make someone else an admin.*

Every ward starts in these states and some will stay there, so a warning that fires on
every save would be trained away precisely where it matters most.

---

## Emergency archives are removed

Archiving an ended emergency as a snapshot of the whole document is removed as part of
this work. Each archive would be a complete copy of the ward directory, sealed to the
devices as they stood when it was written, so removing someone would mean re-keying
every archive. The feature's value, a record of a past emergency, does not carry that
weight.

Ending an emergency clears the response data. Read-only is purely a matter of holding
the lease, enforced in one place.

**The summary report becomes the record of an emergency.** With archives gone the report
is the only thing that outlives the response, so ending an emergency is a confirmation
whose report option is offered clearly rather than as an afterthought. The report is a
plaintext PDF written wherever the user chooses, which is worth saying in the same
breath.

---

## Product rename

The application is **Ward Planning**, and its documents are `.wardplanning`. Encryption
changes the format already, so identity and format change together and compatibility
breaks once. With documents kept in app data, the extension governs exported copies,
recovery copies, and file association rather than an everyday Open dialog. The product
identity - application and organisation names, the macOS bundle identifier, the CMake
project and target names, the vcpkg port name, and the installer's product strings -
changes with it. The organisation name keys per-user storage locations, so the
per-document records land somewhere stable.

The ward name is a label carried by the document and shown in the ward list; the bytes
stay where they are, named by id.

---

## Failure paths

Each gets its own message:

| Condition | Message |
|---|---|
| No identity on this device | Ask whether they already use Ward Planning elsewhere; link, or request access |
| Link code unknown or expired | The code was not found; start again on the new device |
| No entry for this device in a version it should hold | Waiting for access from your other device, or from the admin who granted you |
| Header unreadable | File is damaged |
| Payload authentication fails | File was altered or truncated |
| Identity file unreadable | Name the file; Ward Planning will not start a new identity over it |
| Server unreachable | Working from your local copy; changes upload when it returns |
| Removed by an admin | Your access to the ward was removed by *name*; the local copy has been deleted |
| Left, seen on another device | You left this ward; its copy on this device has been deleted |
| This device unlinked | This device was unlinked by *name*; its wards have been deleted from it |
| Header and sealing set disagree | Refused like any invalid version: the server's copy could not be verified; your local copy is unchanged |
| Upload rejected on version | Someone else saved first, so the merge or the conflict dialog |
| Lease held by another member | Directory read-only, naming who holds it; emergency response still editable |
| Invite link unknown to the server | The document was deleted, or the link is wrong |
| Server version written by a newer app | This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work. Directory read-only; emergency response still editable locally. |
| Document file missing from app data | Name the ward and offer to fetch it from the server |
| A member tries to grant, remove, or change a role | Only an admin can change who has access; name the ward's admins |
| The last admin tries to leave, or be demoted, while others remain | Make someone else an admin first, so the ward can still add and remove people |
| An access change while offline | Connect to the internet to change who has access |

## Debugging

The document's plain JSON (`Document::toJson`) serves a developer-only "export
decrypted JSON" action, off by default, which warns that it writes unencrypted member
data.

## Component changes

| Component | Responsibility |
|---|---|
| `IdentityFile` | This device's key record as a local file; load tells a first run from a damaged file |
| `IdentityService` | This device's keypairs and name; the member id and display name it belongs to; the devices it has linked |
| `DocumentCrypto` | Header parse and build, AD binding, seal and unseal the data key, encrypt and decrypt the payload |
| `DocumentFile` | The encrypted file format: header line and ciphertext, sealing each new version under the sealing-set rule |
| `HttpServerApi` | HTTP transport for the server contract; every call answers through a callback |
| `ServerConnection` | Challenge-response sign-in and the bearer token, over any `ServerApi` |
| `DocumentSession` | One open ward: the single write path, debounce, base version, publish queue, lease, adoption, conflict routing, lost access |
| `RosterChangeQueue` | Sealing additions for every ward - a device this one linked, a person this admin granted - each built on the server's version and published on its own |
| `DocumentManager` | Commands, undo, and geocoding over the open `DocumentSession` |

There is deliberately **no storage abstraction with local and remote implementations**.
The local file is always the local file, and the server is a separate concern layered
above it; a polymorphic store would model a choice this design does not make.

## Testing

Headless tests in `tests/services`:

- `DocumentCrypto` round-trip
- two consecutive saves of an unchanged document produce different nonces, and so do
  two saves made at the same save counter
- associated data built from a header that has been through `toJson`/`fromJson`
  matches the original, so a serialisation round-trip never breaks authentication
- mutating any header byte causes decryption to fail, including the document id
- a header with an unknown format version is refused whole; the directory is read-only
  and nothing publishes, while emergency response edits save locally; the state clears
  as soon as the server again serves a version the app understands
- a header or payload with a missing or malformed required field is refused, never read
  in part
- a version whose header lists a device the sealing set lacks, or whose sealing set has
  a device with no header entry, is refused
- the sealing-set rule: a publish prunes every device the allow-list no longer lists and
  rotates the key, after which a pruned device cannot unseal; a publish that prunes
  nobody keeps the key; a device the server lists but this client did not approve is
  never added; a device this device linked, and a person this admin granted, are added
- a removed person's devices cannot unseal the first version written after the removal
- a leaver's devices cannot unseal the first version written after the leave
- concurrent grants by two admins both end up sealed
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
- a reported removal deletes recorded exported and recovery copies still at their paths,
  and leaves a moved copy alone
- Keep mine publishes this session's directory on top of theirs with their sealing set,
  and a person they granted remains

Against a fake server:

- publishing from the current base version succeeds and advances it
- publishing from a stale base version is refused, adopts their key and sealing set, and
  raises the conflict dialog without publishing this session's payload
- an upload whose header names a removed device is refused, and the retry prunes it
- a downloaded version that fails validation replaces nothing
- edits made while unreachable are kept locally and published on reconnect
- a server ahead with nothing local unpublished fast-forwards without a prompt
- a device told it was removed deletes the local file and removes the ward from the
  list, both mid-session and on next open; an unlinked device does so for every ward
- the server refuses a grant, removal, or role change from a member who is not an admin,
  and refuses to leave a ward with members but no admin
- a removal, an unlink, a leave, or a grant takes effect while another member holds the
  lease
- a person joins only through a granted request, and a device only through an approved
  link
- a link: the approving device's queue seals the new device into every ward it holds,
  and into a ward granted afterwards; the new device shows *Waiting for access* until
  then
- the access history lists each grant, removal, leave, and role change once, after the
  last entry a device read
- a reinstall restores the ward list from the server, and the documents on it download
  again
- the last person leaving deletes the document

---

## Deliberate limits

- **Only the emergency response merges.** Ordinary conflicts are reported and resolved
  by choosing a version; sealing additions merge; emergency response records merge three
  ways against the base copy under the rules above.
- **Responders cannot see each other's work while the network is down.** Records
  reconcile when it returns. Local-network peer sync would close this and is possible
  future work.
- **Overlapping response edits settle by rule, silently.** Contact attempts are kept
  in full; a single value changed differently by two responders takes the value from
  whichever device reconnects last.
- **Removal cuts off future access and cleans up the app's copy.** The removed
  member's app deletes its local copy when it next connects; copies taken out of the
  app, and devices that never reconnect, are beyond reach.
- **The server is trusted with the access list.** A hostile operator or a server bug can
  remove anyone, and their apps delete their local copies; it cannot read the ward.
- **An approved request or link can carry a substituted key.** The two screens compare
  nothing, so an operator acting at the moment of an approval could receive the key.
- **One admin can remove anyone.** A compromised admin device can remove every other
  person, deleting their local copies. Every removal is announced; recovery is a new ward
  built from the directory PDFs, and what they do not recreate is lost.
- **A stolen, unlocked device acts as its owner**, with its owner's role. Unlinking it
  offers to unlink what it linked; otherwise any admin removes its owner.
- **A device linked by a device that is later lost** can be left waiting for access in
  wards the lost device never sealed it into; unlinking it and linking it again from a
  device that holds those wards fixes it.
- **Access must be granted before an emergency**, because granting needs the server.
- **Reports stay plaintext.** `ReportGenerator` writes member-data PDFs to a
  user-chosen path. Out of scope here, but it belongs in the confidentiality story.
- **Emergency archives are gone**, along with the record of past emergencies.
- **The server knows who has access**, by name and role, and who published when, though
  not what the document says.
- **A removal deletes unpublished work.** Someone removed while holding emergency records
  they had not yet published loses them; they already hold the data, and the case is rare.
- **Remote access requests are not verified.** An admin can grant a request sent by
  whoever holds a forwarded link.
- **Sharing and every access change depend on the server being up.** Editing does not.
- **A hostile server operator could keep members on separate or stale copies** without
  them noticing; nothing is exposed, and each member keeps their own work.
- **There is no folder-based sharing mode.** Continuity if the server stops rests on
  the project being open source, the format being documented, and every member holding
  a complete file.
- **Operating the server is ongoing work** - uptime, backups with a tested restore,
  certificate renewal, and abuse handling - carried by whoever runs it rather than by
  the ward.

## Earlier versions

Ward Planning reads only its own `.wardplanning` documents and its own settings. Files
and settings from earlier releases are not migrated; an `.emergencyplan` file is treated
like any other file the app does not own.

Startup restores the last document from the ward list by id. A failure is reported and
the record left alone.

## Identity recovery

Device keys live only in each device's local app data. They survive an uninstall and
reinstall; clearing local app data, a reimaged profile, or a replaced device loses them.
**A person with another working device links the new one** - no one else is involved.
**A person who has lost every device** is removed by an admin and granted again, which
seals the data key to the new device. The single-holder and single-admin notices, and the
suggestion to link a second device, are what keep a ward out of the state where neither
is possible. There is no exported recovery key file.

---

## Hosting notes

Preliminary findings from discussion on 2026-09-13, to seed the server's own design.
Figures marked *verified* were checked against Cloudflare's documentation that day;
the rest are recalled or estimated and need confirming.

### Recommendation

**Cloudflare Workers with one SQLite-backed Durable Object per document.** A Durable
Object is a single-threaded unit with its own storage, which maps directly onto what
this design asks of the server: atomic version checks, a lease with a TTL expired by the
object's alarm, and the members, history, and pending requests stored beside the blob.
People and link codes, which span documents, live in their own objects. Patching, TLS,
uptime, and DDoS protection belong to the platform, which suits a server operated by one
volunteer. It also runs away from any local disaster the ward is responding to.

The runner-up is a small VPS running a plain server over SQLite, with Caddy for TLS
and Litestream for continuous backup. It is portable and anyone can self-host it, in
exchange for the operator carrying patching, uptime, and restore testing.

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
| Cost | Free tier, or about $5/month paid (recalled) | About $4-6/month plus cents for backup storage (recalled) |
| Maintenance | Deploying code | OS updates, monitoring, backup checks, restore tests - about an hour a month plus incidents |
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
| Editing sessions totalling 3-4 hours at a 1-minute heartbeat | ~200 |
| Debounced publishes and access requests | ~50-100 |
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
exactly what lets a successor take over at the same address. Clients decrypt every blob
themselves and seal only to devices they approved, so a lapsed domain re-registered by a
stranger yields ciphertext, names, and request metadata, and stops sharing until a
successor acts. Such a stranger holds the access list, with what the threat model
accepts of a hostile server: it can remove members, whose apps then delete their local
copies, and it can substitute a key at the moment of an approval.

Handing over:

- **A second Super Administrator** on the Cloudflare account, so access never runs
  through the provider's support process.
- **Credentials** (registrar, Cloudflare, GitHub) in a password manager with emergency
  access.
- **A second owner** on the GitHub repository.
- **A runbook** in the repository covering deploy, data location, restore, and
  migrating to another host or domain.
- **A scheduled export** of all ciphertext blobs, people, and memberships to a second
  provider the successor controls, which is what a restore at a new host starts from. The
  blobs are encrypted; the people and memberships name the bishopric, so the export is
  held as carefully as the live server.

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

**The fake and the real server are checked against each other.** The server's rules -
roles and the last admin, joins only through a granted request or an approved link, the
header listing only allow-listed devices, the lease - are written in TypeScript for the
real server and in C++ for the fake. One conformance suite, written against the client's
server interface, runs against both: against the fake in the ordinary test run, and
against the real server over HTTP. The server's plan treats that suite as its acceptance
gate. A disagreement is resolved in whichever implementation is wrong, never by loosening
a test.

**The real server offers a development clock override.** Session, lease, and link-code
expiry can only be tested by moving time forward. The server therefore accepts a clock
offset in local development only - never in a deployed build - so the conformance suite
covers those rules too rather than leaving them to the fake alone.
