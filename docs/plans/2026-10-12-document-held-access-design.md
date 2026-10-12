# Document-Held Access

The ward is its own access list. Supersedes the server-held access parts of
`2026-08-30-encrypted-shared-document-design.md` and both plans of 2026-10-11; the
format, crypto, save and publish, merge, lease, and local library parts of that design
stand.

## Why

The header of every version already names each device that holds the key, and the
payload's sealing set carries their public keys. The server kept a second list - people,
members, roles, devices, requests, history - and most of the machinery built since
August existed to keep the two lists in step: grants that must carry a version, links
that must reseal every ward, a queue, polling for what is on its way, lost-access
discovery. With one list there is nothing to keep in step.

The threat model is a lost phone and a mistaken share, for a roster of names, addresses,
and birth dates. The server is trusted to store and to serve; it is not trusted with the
content, and it never was.

## What the server is

A versioned store of encrypted documents, with a lease. Nothing about people.

| Call | Who may | What |
|---|---|---|
| `POST /documents` | anyone | Stores version 1 of a new document under its client-made id. Rate-limited by address. |
| `GET /documents/{id}/head` | anyone with the id | Version, publisher fingerprint and time, lease holder, the header. |
| `GET /documents/{id}` | anyone with the id | The current version: header and blob. Ciphertext; unreadable without a key. |
| `PUT /documents/{id}` | a key holder | Conditional on `baseVersion`. Signed; see below. |
| `POST`, `DELETE /documents/{id}/lease` | a key holder | Take, refresh, release the edit lease. Signed. |

**Signed requests.** A write carries the signer's two public keys and an Ed25519
signature over the request's document id, base version, and the payload's hash. The
server computes the signer's fingerprint from the keys exactly as clients do and accepts
the write only if the *current* header names that fingerprint. There are no sessions, no
challenge, no registration, no token. The first version of a document is the exception:
anyone may create one, and its header names its creator.

Reads are open to anyone who knows a document id, because the design never relied on the
server to protect content and the id is 128 random bits. A read limit by address stops
scraping.

Removing a person is therefore one write without their devices: the new header lacks
them, so the server refuses their next upload, and their next head shows a header they
are not in.

## What the document carries

The sealing set inside the payload grows from a list of device keys to a small roster:

```
sealingSet:
  people:
    - person: <person id>
      name: "Alice Smith"
      role: admin | member
      devices:
        - { name: "Laptop", publicKey, verifyKey }
        - { name: "Pixel 8", publicKey, verifyKey }
  invites:
    - { for: device | user, person: <person id or absent>, role, label: "for Bob", publicKey, verifyKey }
  formerKeys: [...]
```

The header still names one recipient per key: every device, and every open invite.
Header and sealing set must agree exactly, as now. The server sees fingerprints only.

**Person ids** are the client-generated member ids devices already carry. A device
joining through an *Add a device* invite takes the inviter's person id, name, and role;
one joining through an *Add a user* invite makes a new person with the name it gives.

**Roles are a convention among honest clients.** The server cannot read the sealing set,
so it cannot tell an admin from a member. The rule - admins add users and remove people;
everyone adds and removes their own devices - is what the app offers, and it exists to
keep well-meaning people from over-sharing. A member with a modified client could add a
user, but that member already holds the key and could hand the data over directly. The
design and the user guide say this plainly and never call the role a security measure.

## Invites

An invite is a one-time bearer key.

1. **Add a device** or **Add a user** (admins only for the latter) generates an invite
   keypair from a random 32-byte seed, writes a new version whose header seals the data
   key to it, with an `invites` entry saying what it is for, and shows the link:
   `wardplanning:join/<document id>/<seed, base64url>`. What the person copies is a short
   message, not the bare link, because the recipient may not have the app yet:

   > Alice Smith has invited you to Tanglewood Ward in Ward Planning. Install it from
   > https://github.com/jvilburn/EmergencyPlan/releases/latest, then open this link:
   > wardplanning:join/...

   The download address is the release page `UpdateChecker` already knows. The person
   sends the message by email, text, or any channel they trust with it.
2. **Opening the link** on a device: derive the invite keypair from the seed, fetch the
   current version, unseal the data key with the invite's private key, validate as any
   download is validated, then write the next version: the invite entry becomes a device
   entry - under the inviter's person for a device invite, under a new person for a user
   invite, named by the joiner - signed by the invite's verify key, which the current
   header names. From then on the device signs as itself. The ward opens.
3. **Withdrawing** an unused invite is a write without its entry, by whoever made it or
   by an admin. Removing a person withdraws the invites they made.

Two people changing who has access at the same moment is the ordinary conflict, shown
in the conflict dialog like two edits to the directory; the roster is directory content
for merging. The last person cannot leave: a ward with nobody to read it is not a state.
Leaving needs the server, since the version without you must be held there before your
copy goes.

The link is the gate. Whoever opens it first gets in, visibly, and can be removed.
Interception of a device invite puts the interceptor in as one of the inviter's devices,
under the inviter's name; the inviter sees "you added a device" they did not add and
removes it, which rotates the key. That is the accepted risk at this threat level.

An invite link is about 90 characters: fine to paste, long to type. A QR code can show it
for a camera later if a phone client ever wants one. If a web page is ever hosted for the
app, the link can become `https://.../join#<id>/<seed>`, one address that offers the
download when the app is missing and opens the app when it is there; the seed stays in
the fragment, which browsers do not send to the server.

## Everything else, from the document

- **Who has access**: the roster, read from the open ward. Add a device, Add a user,
  Remove, Make admin / Make member, Leave, Unlink (own devices), Details.
- **Access notices**: the client diffs the roster between the version it last showed and
  the current one: joined (as whom, via whose invite), removed, left, role changed.
  Attribution is the publisher of the version that made the change, whose fingerprint
  the head gives and whose name the roster gives.
- **Lost access**: a head whose header lacks this device. The reason is read off the
  previous roster and the publisher: removed by *name*, or left, or this device unlinked.
  The copy is deleted and the person told, as now.
- **Last admin**: a client rule, as now: an admin cannot step down or leave while others
  remain and no other admin exists. The single-admin and single-holder notices stay.
- **A second device of your own**: *Add a device*, send yourself the link, open it there.
  The first-run screen asks for a name and a device name, nothing else; a new device
  that is really a second device gets its wards by opening links.
- **Reinstall or cleared app data** gives a fresh identity; wards come back by invite
  from another holder (or from your other device). The server keeps no index of who
  holds what, so there is no ward list to restore from it.
- **Lease, merge, conflict, offline, save and publish**: unchanged.

## Failure paths that change

| Condition | Message |
|---|---|
| Invite link already used or revoked | This invite has been used. Ask for another. |
| Invite link for a ward the server does not have | The ward was deleted, or the link is wrong. |
| Opening a link while offline | Connect to the internet to join a ward. |
| Not an admin, trying to add a user | Only an admin can add people. Ask *names*. |
| Head lacks this device | Removed by *name* / You left / This device was unlinked by *name*, as now |

## What is deleted

Server contract and fake: people, registration, sign-in and sessions, link codes,
members and roles, requests and grants, history, unlink, `me`. Client: `ServerConnection`
sign-in (replaced by request signing), `IncomingWards`, `DeviceLinks`,
`FileOpenRouter`'s request route, the request and grant parts of `AccessDialog`, the
link-code page of `FirstRunDialog`, `WardNotices`' history fetch (replaced by the roster
diff), `OpenWardDialog`'s Requested and Waiting rows, the `requestedWards` and
`linkedDevices` settings, `DocumentRecord::historySeen` (replaced by the roster last
shown), `ServerVersion::buildResealing` and the atomic grant and approve. What the
companion server would have had to implement shrinks to the five calls above.

## What is kept

The file format and crypto, `Reconcile`, the sealing-set rule (prune what the header
lacks, add what this write admits, rotate on any removal), `DocumentSession` and its
save, publish, base copy, merge, conflict, and lease handling, `DocumentLibrary`,
`UpdateChecker`, the identity file.

## Open points

- **Reads are open.** Anyone with a document id can download ciphertext. If that ever
  matters, the same signature can gate reads; it costs nothing in the format.
- **Creating documents is open.** Someone could fill the store with junk documents. A
  per-address limit on `POST /documents` is the first answer; a shared secret in the app
  is the second. Not needed to start.
- **Roster size in the payload** is a few hundred bytes per person and does not matter.
