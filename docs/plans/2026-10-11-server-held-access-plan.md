# Server-Held Access Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Move who-has-access onto the server, keep confidentiality on the data key alone, and finish the encrypted shared document's remaining features on that model.

**Architecture:** The server holds people, devices, roles, memberships, and the access history as ordinary state, changed by authenticated calls. The document holds its data key sealed to each device in a *sealing set* inside the payload; every write prunes the set to the server's allow-list, adds only devices this client approved, and rotates the key when it pruned anyone. The membership chain, header signatures, request signing, pairing, and the local high-water mark all go.

**Tech Stack:** Qt 6.11 (Widgets, Network, Test), C++17, MSVC 2022, Ninja, libsodium.

**Design document:** [2026-08-30-encrypted-shared-document-design.md](2026-08-30-encrypted-shared-document-design.md), revised 2026-10-11. Read it first; this plan implements it and does not restate its rationale.

---

## Before you start

**Where the tree stands.** Branch `docs/encrypted-shared-document`. The encrypted file,
the identity file, the document library, the sync-and-save core (`DocumentSession`,
`ServerVersion`, the roster change queue), the HTTP transport, the sync indicator, the
conflict dialog, the access roster dialog, and the ward notices are built on the old
design: a signed membership chain, roles in the header, the roster in the payload, and
pairing by number comparison. 694 tests pass at `be6cee1`. This plan replaces the access
model under all of it and then builds the features still missing.

**Building and testing.** CLAUDE.md gives the commands; Claude runs both. Write logs to
the scratchpad, summarise the test log with `grep -n "^FAIL\|QFATAL"` and the summed
`Totals` lines, and compare the test binary's timestamp with the last commit before
trusting a run. `build.bat` reconfigures only when the top-level `CMakeLists.txt`
changes; a change to `tests/CMakeLists.txt` alone needs
`build/windows-debug/.cmake_marker` deleted first.

**Standing rules.**

- **Core first.** Task 1 is the whole core as one artifact; Task 2 stops for the user's
  review of it. The tree does not build between Task 1 and Task 5, and that is expected.
- **Delete with the last caller.** Code a change makes dead goes in that change.
- **Every new file** goes in `CMakeLists.txt`; every new test in `tests/CMakeLists.txt`
  and `tests/TestMain.cpp`. Every removed file comes out of the same lists.
- **Style** is [CODING_STYLE.md](../CODING_STYLE.md): Allman braces, braces always,
  one-line functions only in headers, leading operators on continuation lines,
  `std::optional<>` for nullable fields, `json` as the local in `toJson`/`fromJson`,
  `tr()` on every user-visible string. No multi-line lambdas: bind a named member with
  `std::bind`; a lambda is accepted only as the `std::function` edit callback to
  `DocumentSession::edit` and `DocumentManager::editResponse`.
- **Editing.** Files mix CRLF and LF; keep each file's endings. Multi-line edits go most
  reliably through a Python script in the scratchpad, reading and writing with
  `encoding='utf-8'` and asserting each old string occurs once. Keep everything ASCII.
- **Commit after every task**, except Task 1, which is committed at Task 2 once the
  user's review is applied. Plain messages, no attribution lines. After each commit,
  dispatch the `review-readonly` agent on it in the background and relay its report
  verbatim.
- **Server ids name local files.** `DocumentLibrary` and `HttpWire` accept only canonical
  document ids from the server; keep that for every server-supplied id.

**Terminology:**

| Term | Meaning |
|---|---|
| Data key | 32-byte random key encrypting one document's payload |
| Sealed key | The data key encrypted to one device's X25519 public key |
| Sealing set | The devices a document's data key is sealed to, held **inside** the payload: each device's X25519 public key and Ed25519 verify key. The one piece of access state the server never controls |
| Sealing-set rule | Each write prunes devices the allow-list no longer lists, adds devices this client approved that it does list, and rotates the key when it pruned anyone |
| Approved device | A device this client may add: one this device linked, or the device that asked in a request this admin granted |
| Person | A server record shared by all their wards: member id, display name, devices |
| Member | A person in one ward, with a role: **admin** or **member** |
| Allow-list | The devices of a ward's current members; the server permits exactly these to fetch and publish |
| Lost access | The server's answer to a device that had access and no longer does: removed by whom, left, or unlinked by whom |
| Access history | The server's list of grants, removals, leaves, and role changes, read for the notices |
| Link code | A short lookup handle a new device shows so its person can approve it from another device |
| Base version, base copy, publish, lease | Unchanged from the existing code |

---

## Phase 1 - The core

### Task 1: The access model, the server that holds it, and the sealing-set rule

> **Done, with these differences.** Where the code differs from the sections below,
> the code is the reference:
> - **Former keys.** The sealing set also lists a hash of every earlier data key, and
>   `Reconcile::validate` / `validateDownload` take the keys this device holds of the
>   ward (`ServerVersion::trustedKeys`): a version under another key must name one of
>   them, so a server cannot hand a device a ward of its own making. A device's first
>   download is taken as it stands, an accepted limit in the design. A device whose
>   base copy is unreadable while its local file holds an unpublished rotation trusts
>   only the local key, so a server version rotated from the base key is refused until
>   the base is readable again; accepted as rare.
> - A head that leaves this device out is a fault and is never applied
>   (`ServerVersion::listsDevice`); one refused as NotMember or UnknownDocument is a
>   fault, not Unreachable.
> - `ServerVersion::isCurrent(document, access)` takes no header;
>   `ServerVersion::settleGranted` drops granted devices after this device's own
>   accepted upload.
> - `ServerContract.cpp` stays, for the multi-line `operator==` definitions.
> - The session signals every answered head (`headChanged`), and the manager fetches
>   history whenever the head's index is past what it has shown.
> - The last person's leave keeps a tombstone: the id and lost-access records.
> - `ServerConnection` remembers a refused registration and answers every later call
>   with it.

**Everything that decides who may fetch, who can read, and how a write keeps the two in
step, written as one artifact.** The server contract and the fake server that enforces
its rules; the sealing set and the rule over it; the slimmed header, crypto, and file
format; reconciliation; and the orchestration in `DocumentSession`, `ServerVersion`, and
the queue that carries additions to closed wards. Every later task is transport, wiring,
UI, or tests around it.

Code the core makes dead is deleted in this task: the chain, header signing, request
signing, pairing, the roster model, roster changes, and access events.

**Files:**

- Rewrite: `src/services/ServerContract.h`; delete `src/services/ServerContract.cpp`
  (its request bodies go with request signing)
- Rewrite: `tests/services/FakeServer.h/.cpp`
- Create: `src/models/SealingSet.h/.cpp`
- Modify: `src/models/Document.h/.cpp` (the roster field becomes the sealing set)
- Modify: `src/services/DocumentHeader.h/.cpp`, `src/services/DocumentCrypto.h/.cpp`,
  `src/services/DocumentFile.h/.cpp`, `src/services/CryptoTypes.h`
- Modify: `src/services/Reconcile.h/.cpp`, `src/services/ServerVersion.h/.cpp`,
  `src/services/DocumentRecords.h/.cpp`, `src/services/DeviceIdentity.h/.cpp`
- Modify: `src/services/DocumentSession.h/.cpp`, `src/services/ServerConnection.h/.cpp`
- Rename and rewrite: `src/services/RosterChangeQueue.h/.cpp` to
  `src/services/SealingQueue.h/.cpp`
- Delete: `src/services/AccessControl.h/.cpp`, `src/services/AccessEvents.h/.cpp`,
  `src/services/Pairing.h/.cpp`, `src/services/RosterChange.h/.cpp`,
  `src/models/AccessRoster.h/.cpp`, `src/models/RosterPerson.h/.cpp`,
  `src/models/RosterDevice.h/.cpp`
- Modify: `CMakeLists.txt` source lists

Write the sections in order; each builds on the ones above it.

#### 1.1 The server contract

`ServerContract.h` keeps its shape - types only, every call answering through a `Reply`
called exactly once and never before the call returns - with the new state and calls.
The namespace comment becomes:

```cpp
/// What the Ward Planning server accepts and returns - types only, no transport.
///
/// The server holds the access list and the ciphertext. People, their devices,
/// each ward's members and roles, and the access history are its ordinary state,
/// changed by the calls below. It holds no key material and never reads a payload;
/// its only cryptography is checking a login signature and computing device
/// fingerprints.
///
/// Confidentiality does not rest on it. A client seals the data key only to devices
/// it approved itself or that the version it builds on already held, so a server
/// that lies about the access list can deny service but cannot read a ward.
///
/// FakeServer (tests) implements this in memory and enforces every rule the real
/// server must, so a client written against it will not meet a stricter server.
```

The types:

```cpp
enum class ServerError
{
    None,
    Unreachable,
    Unauthenticated,    ///< no session, an expired one, or a login that did not verify
    AccessLost,         ///< this device had access and no longer does; Result::lost says how
    NotMember,          ///< the caller's person is not a member of this ward
    NotAdmin,           ///< an admin's call made by a member
    LastAdmin,          ///< would leave a ward with people in it and no admin
    NotOnAllowList,     ///< the uploaded header names a device the allow-list lacks
    UnknownDocument,
    VersionConflict,    ///< the upload was not built on the current version
    LeaseHeld,
    IdTaken,
    NotFound,           ///< an unknown link code, request, or member
    Expired,            ///< a link code past its ten minutes
    RateLimited,
    InvalidRequest      ///< ill-formed, or contradicts the caller's own identity
};

enum class Role
{
    Admin,
    Member
};

/// One of a person's devices, as the server holds it.
struct Device
{
    Fingerprint fingerprint;
    DeviceKeys keys;
    QString name;
    std::optional<Fingerprint> linkedBy;    ///< the device that linked it; none for a person's first
    QDateTime linkedAt;
};

struct Person
{
    MemberId member;
    QString displayName;
    QList<Device> devices;
};

/// A person in one ward.
struct Member
{
    Person person;
    Role role = Role::Member;
};

/// Why a device that had access no longer does - the reason the app acts on.
struct LostAccess
{
    enum class How
    {
        Removed,
        Left,
        Unlinked
    };

    How how = How::Removed;
    QString byWhom;     ///< display name at the time; empty for Left
    QDateTime when;
};

template<typename T>
struct Result
{
    std::optional<T> value;
    ServerError error = ServerError::None;
    std::optional<LostAccess> lost;     ///< set exactly when error is AccessLost

    bool ok() const { return value.has_value(); }
    static Result success(const T& result) { return Result{result, ServerError::None, std::nullopt}; }
    static Result failure(ServerError reason) { return Result{std::nullopt, reason, std::nullopt}; }
    static Result lostAccess(const LostAccess& how) { return Result{std::nullopt, ServerError::AccessLost, how}; }
};

template<typename T>
using Reply = std::function<void(const Result<T>&)>;

struct Done
{
};

/// A bearer token from a signed login challenge, sent with every call.
struct Session
{
    QString token;
};

/// PUT /me, sent by ServerConnection after every sign-in. Registers a new person
/// with this device as its first; for a device already in the person it changes
/// nothing. A device claiming a person it is not in is refused - an unlinked
/// device's claim is refused as AccessLost.
struct Registration
{
    MemberId member;
    QString displayName;
    QString deviceName;
    PublicKey publicKey;    ///< the verify key is the session's own
};

/// GET /me
struct Me
{
    Person person;
    QStringList documents;  ///< the wards this person is a member of
};

/// Who published a version, and when.
struct Publication
{
    QString name;           ///< display name at the time
    Fingerprint device;
    QDateTime when;
};

// Reads carry the header as the server stored it. A reader parses it with
// Reconcile::validateDownload, so a header in a later format reaches the client as
// NeedsNewerVersion rather than being lost in transit.

/// GET /documents/{id}/head
struct Head
{
    qint64 version = 0;
    Publication published;
    std::optional<Fingerprint> leaseHolder;
    QJsonObject header;
    QList<Member> members;
    qint64 history = 0;     ///< the index of the newest access-history entry; 0 for none
};

/// GET /documents/{id} - one consistent state, so a publish landing mid-download
/// can never pair a header with another version's blob.
struct Snapshot
{
    qint64 version = 0;
    Publication published;
    QJsonObject header;
    QByteArray blob;
};

/// POST /documents and PUT /documents/{id}.
struct Upload
{
    DocumentHeader header;
    QByteArray blob;
};

struct Lease
{
    Fingerprint holder;
    QDateTime expires;
};

/// GET /documents/{id}/history?after={n}
struct HistoryEntry
{
    enum class Kind
    {
        Added,
        Removed,
        Left,
        MadeAdmin,
        MadeMember      ///< actor and subject the same: stepped down
    };

    qint64 index = 0;
    Kind kind = Kind::Added;
    QString actor;      ///< display names at the time
    QString subject;
    QDateTime when;
};

/// GET /documents/{id}/requests
struct AccessRequest
{
    qint64 index = 0;
    Person person;      ///< devices: exactly the one that asked
    QDateTime when;
};

enum class RequestStatus
{
    Pending,
    Granted,
    Declined
};

/// GET /links/{code}. The approving device reads the new device's name and keys;
/// the new device reads whether it has been approved, and as whom.
struct LinkState
{
    QString deviceName;
    DeviceKeys keys;
    std::optional<Person> approvedAs;
    QDateTime expires;
};
```

`ServerApi`, one method per endpoint, each with a `Reply`:

| Method | Endpoint | Who |
|---|---|---|
| `challenge(reply)` | `POST /auth/challenge` | anyone |
| `authenticate(verifyKey, challenge, signature, reply<Session>)` | `POST /auth/token` | anyone |
| `registerPerson(session, Registration, reply<Done>)` | `PUT /me` | signed in |
| `me(session, reply<Me>)` | `GET /me` | signed in |
| `unlinkDevice(session, Fingerprint, reply<Done>)` | `DELETE /me/devices/{fp}` | signed in; never the caller's person's last device |
| `startLink(session, PublicKey, deviceName, reply<QString>)` | `POST /links` | signed in, no person yet; answers the code |
| `link(session, code, reply<LinkState>)` | `GET /links/{code}` | the new device, or a signed-in device with a person |
| `approveLink(session, code, reply<Done>)` | `POST /links/{code}/approve` | a device with a person; adds the new device to it |
| `registerDocument(session, Upload, reply<qint64>)` | `POST /documents` | signed in; caller becomes first admin |
| `head(session, id, reply<Head>)` | `GET /documents/{id}/head` | allow-listed |
| `download(session, id, reply<Snapshot>)` | `GET /documents/{id}` | allow-listed |
| `upload(session, id, baseVersion, Upload, reply<qint64>)` | `PUT /documents/{id}` | allow-listed |
| `takeLease`, `releaseLease` | `POST`/`DELETE /documents/{id}/lease` | allow-listed |
| `history(session, id, after, reply<QList<HistoryEntry>>)` | `GET /documents/{id}/history` | allow-listed |
| `requestAccess(session, id, reply<Done>)` | `POST /documents/{id}/requests` | signed in; the request is the caller's person and device |
| `accessRequests(session, id, reply<QList<AccessRequest>>)` | `GET /documents/{id}/requests` | allow-listed |
| `declineRequest(session, id, index, reply<Done>)` | `DELETE /documents/{id}/requests/{n}` | admin |
| `myRequestStatus(session, id, reply<RequestStatus>)` | `GET /documents/{id}/requests/mine` | the requester |
| `grant(session, id, index, reply<Done>)` | `POST /documents/{id}/members` | admin |
| `removeMember(session, id, MemberId, reply<Done>)` | `DELETE /documents/{id}/members/{m}` | admin, or the person themselves to leave |
| `setRole(session, id, MemberId, Role, reply<Done>)` | `PUT /documents/{id}/members/{m}/role` | admin |
| `advanceClock(seconds, reply<Done>)` | development only | - |

There are no request signatures and no canonical bodies; the bearer token travels over
TLS.

#### 1.2 The fake server

`FakeServer` implements `ServerApi` in memory and enforces every rule the real server
must. Keep its existing machinery - replies queued and delivered on a later turn, the
`Limits` struct, going offline, holding and losing replies, forcing a conflict, holding
the lease as someone else - and drop the chain controls (`withholdHeader`,
`substituteHeader`) and the pairing script. Its state:

- **People**, by member id hex: display name and devices (fingerprint computed with
  `DocumentCrypto::fingerprintOf`, keys, name, linked by, linked at).
- **Devices**, by verify key, mapping to their person - a session knows its device by
  the verify key it signed in with.
- **Documents**, by id: version, header JSON, blob, publication, members (member id hex
  to role), access history, pending requests, lease.
- **Lost access**, by document id and member id, and by unlinked device fingerprint.
- **Link codes**: keys, device name, expiry, approved-as.

The rules, each a refusal with its `ServerError`:

- A document call from a device that is not allow-listed: `AccessLost` with the record
  when one exists for that device's person in that document, or for the device itself
  when it was unlinked; otherwise `NotMember`.
- `upload`: `VersionConflict` unless `baseVersion` is the current version.
  `NotOnAllowList` when the header lists any fingerprint outside the current members'
  devices. The version only rises. Blob size capped; uploads rate-limited per device.
- `registerDocument`: `IdTaken` for a known id; `NotOnAllowList` when the header lists
  any device not the caller's person's. The caller becomes the only member, an admin;
  rate-limited per device.
- `grant`, `removeMember` of another, `setRole`, `declineRequest`: `NotAdmin` from a
  member. `removeMember`, `setRole` to Member, and a leave: `LastAdmin` when the ward
  would keep people and no admin. A leave by the last person drops the document's
  content and keeps its id and lost-access records, so its other devices hear they
  left; a request for it is `UnknownDocument`, and a stranger's read `NotMember`.
- `grant` makes the request's person a member with role Member and appends `Added`;
  removal appends `Removed` and records lost access; a leave appends `Left` and records
  it; `setRole` appends `MadeAdmin` or `MadeMember`.
- `registerPerson`: a new member id creates the person with this device; a device
  already in that person changes nothing; anything else `InvalidRequest`, or
  `AccessLost` for a device that was unlinked from that person.
- `approveLink`: `NotFound` for an unknown code, `Expired` past ten minutes; adds the
  device under the caller's person with `linkedBy` the caller's device, once.
- `unlinkDevice`: only the caller's person's own device, never its last; records lost
  access for that device, and its person's memberships stop covering it at once.
- `requestAccess`: the caller must have a registered person; one pending request per
  person per document; rate-limited. `myRequestStatus` answers Granted once the person
  is a member, Declined once declined.

Every limit stays in `Limits` (`linkSeconds` replaces `pairingSeconds`).

#### 1.3 The sealing set

`src/models/SealingSet.h` replaces `AccessRoster`, `RosterPerson`, and `RosterDevice`:

```cpp
/// The devices this document's data key is sealed to, carried inside the payload.
///
/// This is the one piece of access state the server never controls: a client seals
/// only to devices listed here, or to devices it approved itself, so a server that
/// lies about who has access can deny service but never gain it. A header entry
/// carries only a fingerprint, and a key cannot be sealed to a fingerprint, so the
/// public keys live here.
///
/// It carries no names, member ids, or roles; those belong to the server.
class SealingSet
{
public:
    /// What the next write seals to, and whether it needs a fresh data key.
    struct Next
    {
        SealingSet devices;
        bool rotate = false;
    };

    SealingSet() = default;

    /// A new ward's set: the device that made it.
    static SealingSet of(const DeviceKeys& firstDevice);

    /// Refused for invalid keys, and for a device sharing either key with another
    /// one here. Adding a device already here changes nothing.
    bool add(const DeviceKeys& device);

    bool contains(const Fingerprint& fingerprint) const;
    int size() const { return int(m_devices.size()); }
    bool isEmpty() const { return m_devices.isEmpty(); }

    /// Sorted by fingerprint hex, so every device seals in the same order.
    QList<DeviceKeys> devices() const;

    /// The sealing-set rule. Keeps the devices allowList still lists; adds each
    /// approved device allowList lists; rotates when anything was dropped.
    ///
    /// Only approved devices are ever added - never a device because the server
    /// lists it - which is what keeps a hostile server from planting one.
    /// allowList holds fingerprint hex.
    Next next(const QSet<QString>& allowList, const QList<DeviceKeys>& approved) const;

    QJsonObject toJson() const;

    /// nullopt for an empty set, or any device missing, ill-formed, or sharing a key.
    static std::optional<SealingSet> fromJson(const QJsonObject& json);

    bool operator==(const SealingSet& other) const;
    bool operator!=(const SealingSet& other) const { return !(*this == other); }

private:
    /// Keyed by fingerprint hex.
    QHash<QString, DeviceKeys> m_devices;
};
```

`Document`: `accessRoster()`/`setAccessRoster()` become `sealingSet()`/`setSealingSet()`,
the JSON key `accessRoster` becomes `sealingSet`, and `parse` refuses an empty set as it
refused an empty roster.

#### 1.4 The header, crypto, and file

**`DocumentHeader`**, format version 3:

```cpp
/// One device's claim on a document: its fingerprint, and the data key sealed to
/// its X25519 public key, which the sealing set holds.
struct RecipientEntry
{
    Fingerprint fingerprint;
    QByteArray sealedKey;
    ...
};

struct DocumentHeader
{
    static constexpr int CurrentFormatVersion = 3;
    static constexpr int DocumentIdLength = 32;
    static constexpr qint64 MaxSaveCounter = (qint64(1) << 53) - 1;

    int formatVersion = CurrentFormatVersion;
    QString documentId;
    QString originDocumentId;
    qint64 saveCounter = 0;
    QByteArray nonce;
    QList<RecipientEntry> recipients;   ///< never empty

    QJsonObject toJson() const;
    static HeaderParse parse(const QJsonObject& json, DocumentHeader& out);

    /// The whole header, canonical, built from the strings toJson writes.
    QByteArray associatedData() const;

    std::optional<RecipientEntry> entryFor(const Fingerprint& fingerprint) const;
    ...
};
```

`keyGeneration`, `lastWriter`, the writer keys, `lastWriteTime`, `admins`,
`payloadHash`, `writerSignature`, `signedBytes()`, `isAdmin()`, and
`MaxSaveCounterStep` go. `parse` stays strict: every field present and well formed,
nothing else present, recipients non-empty with distinct fingerprints and sealed keys of
`DocumentCrypto::SealedDataKeySize`.

**`DocumentCrypto`** keeps key generation, `fingerprintOf`, seal and unseal, the nonce,
encrypt and decrypt, and `payloadHash` (the local record's check on the base copy).
`HashDomain` keeps `Fingerprint` and `Payload`. Signing becomes one purpose-named pair:

```cpp
/// The one thing a device signs: the server's login challenge.
static QByteArray signChallenge(const QByteArray& challenge, const SigningKey& key);
static bool challengeVerifies(const QByteArray& challenge, const QByteArray& signature, const VerifyKey& key);
```

`SignatureTag`, `ChainResult`, `signHeader`, `verifyHeader`, `writerIsAccountedFor`,
`isCoherent`, `verifyLink`, and `verifyChain` go. In `CryptoTypes.h`, `VerifyKey`'s
comment says it checks a login signature.

**`DocumentFile::seal(document, previous, dataKey)`** seals `dataKey` to every device in
`document.sealingSet()`, takes a fresh nonce, advances the save counter, and encrypts. It
applies no rule; the caller decides the set and the key (1.6). nullopt when sealing or
encrypting fails or the counter is at its cap. The writer parameter goes.

#### 1.5 Reconciliation and the local record

**`Reconcile`** shrinks to what a download needs:

```cpp
namespace Reconcile
{

enum class Verdict
{
    Accept,
    NeedsNewerVersion,
    ServerFault         ///< anything that does not check out; the local file is untouched
};

struct DownloadCheck { ... };   // verdict, reason, header, dataKey, document - as now

struct Self
{
    Keypair sealing;
    VerifyKey verifyKey;

    Fingerprint fingerprint() const;
};

/// Whether a server version may replace the local file: the header parses, lists
/// this device, its key unseals, the payload decrypts and parses, and the header
/// matches the payload's sealing set. documentId is the ward being reconciled; a
/// version of any other document is ServerFault.
DownloadCheck validateDownload(const QString& documentId,
                               const QJsonObject& header,
                               const QByteArray& ciphertext,
                               const Self& self);

/// Every recipient entry's fingerprint is one device in the set, and every device
/// in the set has exactly one entry. The agreement the server cannot check.
bool headerMatchesSealingSet(const DocumentHeader& header, const SealingSet& set);

} // namespace Reconcile
```

`HighWaterMark`, `readHeaders`, and `checkRemoval` go. Opening a local file uses the same
checks as a download.

**`DocumentRecord`**:

```cpp
struct DocumentRecord
{
    /// The server version the base copy holds. Set once the ward is shared, and
    /// what makes it shared.
    std::optional<qint64> baseVersion;

    /// The base copy's payload hash, so a damaged or replaced base copy is not
    /// taken for the version the server and this device agreed on.
    QByteArray baseHash;

    /// The base copy's save counter. A local file above it has unpublished changes.
    qint64 baseSaveCounter = 0;

    /// The devices of people this admin granted, each the device that asked, until a
    /// version seals to it or the server stops listing it.
    QList<DeviceKeys> granted;
    ...
};
```

`DocumentRecords` gains the one device-level list the rule needs:

```cpp
    /// The devices this device linked, which it seals into every ward it holds.
    virtual QList<DeviceKeys> linkedDevices() const = 0;
    virtual void setLinkedDevices(const QList<DeviceKeys>& devices) = 0;
```

`DeviceIdentity` loses `asRosterDevice` and `asNewPerson` and gains
`DeviceKeys keys() const` and `ServerContract::Registration registration() const`.

#### 1.6 Taking in and writing versions: `ServerVersion`

`ServerVersion` is what the session and the queue share:

```cpp
/// What one write may seal to: the allow-list from the latest head, and the devices
/// this client approved for this ward.
struct Access
{
    QSet<QString> allowList;        ///< fingerprint hex
    QList<DeviceKeys> approved;
};

/// The allow-list a head names, and this ward's approved devices: its record's
/// granted devices and this device's linked ones.
Access accessFor(const ServerContract::Head& head, const DocumentRecord& record, const QList<DeviceKeys>& linked);

/// The sealing-set rule applied to a document about to be written: its sealing set
/// becomes SealingSet::next, and the data key is fresh when that rotates.
struct Resealed
{
    Document document;
    DataKey dataKey;
};
Resealed reseal(const Document& document, const DataKey& dataKey, const Access& access);

/// Whether a header already seals to exactly what the rule would give.
bool isCurrent(const DocumentHeader& header, const Document& document, const Access& access);
```

`BaseCopy`, `readBase`, `writeBase` (now recording `baseSaveCounter`), and `forget` stay.
`Fetched` loses `laterHeaders`, `remover`, and `left`, and gains:

```cpp
    enum class Outcome { Accepted, AccessLost, NeedsNewerVersion, CallFailed, ServerFault };
    ServerContract::Publication published;          // Accepted
    std::optional<ServerContract::LostAccess> lost; // AccessLost
```

`fetch(identity, server, documentId, receiver, done)` downloads the snapshot and runs
`validateDownload`; it needs no base copy and no records. `proveRemoval` goes.

#### 1.7 The open ward: `DocumentSession`

The state machine keeps its shape - one operation at a time, saving and publishing on
separate clocks, the lease beside them, the refused-publish route - with these changes:

- **The latest head is held** (`std::optional<ServerContract::Head> m_head`), and with it
  the ward's `Access`. `lastHead()` exposes it; `headChanged()` fires when a head check
  answers with different members, roles, lease holder, or history index.
- **Every save of a shared ward applies the rule** through `ServerVersion::reseal`
  against the held access. Before the first head of a session there is no allow-list,
  and a save seals to the current set unchanged.
- **Publish re-seals first when needed.** If `ServerVersion::isCurrent` is false for the
  local file against the held access, publish writes a save (which reseals) and uploads
  that. A ward's own content is never published under a set the rule would change.
- **`NotOnAllowList` from an upload** checks the head and publishes again; the fresh
  allow-list prunes the device.
- **`AccessLost` from any call** finishes the session: `finishLost(const LostAccess&)`
  forgets the files and record, stops every timer, and emits
  `accessLost(const ServerContract::LostAccess&)`, replacing `removed(byWhom, left)`.
- **The refused-publish route** takes their data key and sealing set exactly, merges as
  now, and publishes under the rule. `raiseConflict` puts their sealing set into this
  session's document in place of their roster.
- **The conflict names its writer from the snapshot**: `Conflict{published.name,
  published.when}`. `nameOf` and `baseRoster` go.
- **`create`** seals a new ward to `SealingSet::of(identity.keys())`.
- **`share()`** takes no devices. It registers the local file as it stands, sealed to
  this device alone; the next publish adds this device's linked devices once the head
  lists them.
- **`versionAdopted`** goes; the notices read the server's history (Task 6).
- `finishRemoved`, `proveRemoval`, `onRemovalChecked` go.

#### 1.8 Carrying additions to every ward: `SealingQueue`

`RosterChangeQueue` becomes `SealingQueue`. Removals, role changes, and leaves are server
calls now, so the only thing left to carry to wards is an addition, and an addition is
not a change to queue but a condition to reach: every ward sealed to every approved
device the server lists.

```cpp
/// Seals each ward's data key to the devices this device approved - a device it
/// linked, a person it granted - that the server lists and the ward's current
/// version does not yet seal to.
///
/// Additions only: removals take effect on the server at once, and the next write in
/// each ward prunes them. Nothing here needs the lease, so no one holding it can hold
/// off a link or a grant.
///
/// For the ward open in the window it asks the session to publish, which reseals; for
/// any other it fetches the server's version, reseals it, and uploads it conditional
/// on that version, leaving the local file to catch up when the ward next opens.
class SealingQueue : public QObject
{
    Q_OBJECT

public:
    SealingQueue(const DeviceIdentity& identity,
                 DocumentRecords& records,
                 ServerConnection& server,
                 std::unique_ptr<SessionTimers> timers,
                 std::function<DocumentPaths(const QString&)> pathsFor,
                 QObject* parent);

    void setOpenSession(DocumentSession* session);

    /// Checks every shared ward and seals where needed. Called at start, after a link
    /// or a grant, and on Retry until every ward is done.
    void sealEverywhere();

    /// Wards still waiting for a seal this device owes them.
    int pendingCount() const;

signals:
    void pendingChanged();

    /// The server told this device it lost access to a ward; its files and record
    /// are gone.
    void accessLost(const QString& documentId, const ServerContract::LostAccess& lost);
};
```

Per ward, one at a time: head; if the header lacks an approved device the allow-list
lists, seal (open ward: `publishNow`; closed ward: fetch, `reseal`, upload).
`VersionConflict` and `NotOnAllowList` start that ward again; `Unreachable` defers it to
the next Retry while the others go ahead; `AccessLost` forgets the ward. A granted device
leaves the record's `granted` list once a version this device published seals it, or
once the allow-list no longer lists it.

#### 1.9 `ServerConnection`

Signing in becomes: challenge, `DocumentCrypto::signChallenge`, `authenticate`, then
`registerPerson(identity.registration())` when the identity has a person. Request
signing, `signature()`, and every `send...` helper go; each public method is a plain
`call` with the session. Add a method per new endpoint in 1.1, each with the `context`
parameter the existing ones take, and drop `headers` and `myDocuments` (replaced by
`me`). The re-sign-in on `Unauthenticated` and the waiting list of calls stay.

---

### Task 2: STOP - user review of the core

Hand the user the diff of Task 1 and say plainly what has and has not happened: nothing
has been built or tested, nothing is committed, no agent has reviewed it. Wait for their
review and apply what they ask. Then commit as
`refactor: hold access on the server and seal by the sealing-set rule`, and dispatch the
read-only review on that commit.

---

## Phase 2 - Make it build and pass

### Task 3: The app on the new core

**Files:** `src/services/AppSettings.h/.cpp`, `src/services/DocumentLibrary.h/.cpp`,
`src/services/DocumentManager.h/.cpp`, `src/services/IdentityService.h/.cpp`,
`src/services/IdentityFile.h`, `src/widgets/AccessDialog.h/.cpp`,
`src/widgets/WardNotices.h/.cpp`, `src/widgets/MainWindow.h/.cpp`,
`src/widgets/FirstRunDialog.h/.cpp`, `src/services/FileOpenRouter.h/.cpp`, `src/main.cpp`

Make the application compile and behave on Task 1's types, with no new features:

- `AppSettings` stores the new `DocumentRecord` fields and the linked-device list.
- `DocumentLibrary`: copies seal to the document's sealing set; `restore` lists wards
  through `ServerConnection::me`; `forgetRemoved` is driven by lost access.
- `DocumentManager`: `accessRemoved(wardName, byWhom, left)` becomes
  `accessLost(wardName, const LostAccess&)` from both the session and the queue;
  `accessChanged(events)` and `onVersionAdopted` go (Task 6 replaces them);
  `fetchDeviceOrigins` and `onChainFetched` go, since the head's devices carry
  `linkedBy`. Expose the open session's `lastHead()` for the dialog.
- `AccessDialog` shows the head's members - name, role, devices with their names and who
  linked each - in place of the roster and origins. Its action signals take member ids
  and fingerprints as now; they stay unconnected until their tasks.
- `IdentityService`: the comment that names cannot change because no chain rule carries a
  rename goes; `joinPerson` stays, fed by a link (Task 9).
- `FileOpenRouter`: a header no longer names people, so `LinkDevice` goes. A file routes
  to `OpenWard` (in the library), `OpenCopy` (it has an origin), `AddWard` (its header
  lists this device), or `RequestAccess` (anything else).
- `MainWindow`: the request-access message stays as a placeholder for Task 8; the
  link-device message goes with its route.

Commit as `refactor: run the app on server-held access`.

### Task 4: Transport

**Files:** `src/services/HttpWire.h/.cpp`, `src/services/HttpServerApi.h/.cpp`

The wire format for Task 1's contract: the routes in 1.1, `Authorization: Bearer
<token>` on every call after sign-in, no `X-Request-Signature`. Bodies and answers follow
the existing conventions (base64 bytes, lowercase hex ids, ISO 8601 UTC with
milliseconds, a header as its JSON object). `AccessLost` travels as status 403 with
`{error: "accessLost", how, byWhom, when}`; the other errors keep the existing
status-and-name table, with `notMember`, `notAdmin`, `lastAdmin`, `notOnAllowList`,
`notFound`, `expired`, and `invalidRequest` added and `chainRefused`, `payloadMismatch`,
`counterJump`, `deviceNotInvited`, `alreadyWritten`, `outOfOrder` removed. Update the
routes table in `HttpWire.h`, which is the specification the companion server follows.

Commit as `feat: wire format for server-held access`.

### Task 5: Tests

**Files:** under `tests/`; `tests/CMakeLists.txt`, `tests/TestMain.cpp`

Delete: `DocumentChainTest`, `ChainTestSupport`, `AccessControlTest`, `AccessEventsTest`,
`PairingTest`, `AccessRosterTest`. Rename `RosterChangeQueueTest` to `SealingQueueTest`.
Add `SealingSetTest`. Rewrite the rest against the new types. The cases, each with the
observed values pinned and every absence anchored to a positive signal:

**`SealingSetTest`:** the rule keeps every listed device and rotates on none dropped; a
dropped device rotates; an approved device the allow-list lists is added, one it does
not list is not; a device the allow-list lists but nobody approved is never added; a
device sharing a key with another is refused; JSON round trip; an empty set is refused
on read.

**`DocumentHeaderTest`, `DocumentCryptoTest`, `DocumentFileTest`, `FormatVectorsTest`:**
strict parsing of format 3, every field required and nothing extra; mutating any header
byte fails decryption, the document id and origin included; fresh nonces on consecutive
saves and on two saves at one counter; associated data survives a JSON round trip;
known-answer vectors for the fingerprint and for a format-3 header's associated data;
`signChallenge` verifies and fails on one changed byte.

**`ReconcileTest`:** a valid download is accepted; a header listing a device the set
lacks, or a set device with no entry, is ServerFault; a version of another document is
ServerFault; a later format is NeedsNewerVersion; a version not sealed to this device is
ServerFault and replaces nothing.

**`FakeServerTest` and `ServerConformanceTest`** (the conformance suite runs against the
fake here and against the real server when `WARD_PLANNING_SERVER_URL` is set): each rule
in 1.2, refused and allowed, including: a member's grant, removal, and role change are
`NotAdmin`; the last admin's leave or demotion with others present is `LastAdmin`; an
upload naming a removed device is `NotOnAllowList` and one naming fewer devices than the
allow-list is accepted; a removed person's devices get `AccessLost` with the remover's
name; an unlinked device gets `AccessLost` Unlinked on every ward and on `registerPerson`;
a person joins only through a granted request and a device only through an approved link;
the last person's leave ends the ward; the history lists each change once after a
given index; session, lease, and link-code expiry through `advanceClock`.

**`DocumentSessionTest`:** publishing from the base succeeds; a stale base adopts their
key and set and merges or raises the conflict; after a removal, the first publish prunes
the removed device, rotates, and the removed device cannot unseal the result while the
others can; a publish that prunes nobody keeps the key; an upload refused
`NotOnAllowList` re-heads and succeeds; `AccessLost` mid-session and on open deletes the
files and record and emits the reason; a device linked by this one is added at the next
publish after the head lists it, and a device the head lists that nobody here approved is
not; a member without the lease records and publishes response edits; offline edits
publish on reconnect; the conflict names the publisher from the snapshot.

**`SealingQueueTest`:** a link seals the new device into every shared ward, the open one
through its session and closed ones by upload; a ward granted afterwards is sealed on the
next `sealEverywhere`; a grant seals the requesting device and then drops it from the
record; an unreachable ward is retried while the others finish; `AccessLost` forgets the
ward.

**`DocumentLibraryTest`, `AppSettingsTest`, `ResponseMergeTest`, `HttpServerApiTest`:**
updated for the new types and routes; a reported loss of access deletes recorded copies
still at their paths and leaves a moved copy.

Build, run, and fix until green. Commit as `test: server-held access`.

---

## Phase 3 - Features

### Task 6: Ward notices from the server's history

**Files:** `src/widgets/WardNotices.h/.cpp`, `src/services/DocumentManager.h/.cpp`,
`src/services/DocumentRecords.h/.cpp`, `src/services/AppSettings.cpp`, the stylesheet,
tests

The access-change notice reads the server's history. When the open session's head
reports a `history` index above the record's `historySeen` (a new `DocumentRecord`
field), `DocumentManager` fetches `history(after: historySeen)` and emits
`accessChanged(const QList<ServerContract::HistoryEntry>&)`; dismissing the notice
advances `historySeen` to the newest entry shown. Unread changes therefore survive a
second sync, a ward switch, and a restart, and the notice never shows another ward's
entries. This replaces the Task 19 review's findings 1 and 2 at their source: the
per-ward event lists, the access-change dismissed key, and the clearing on
`documentOpened` go.

The single key holder and single admin notices count the head's members and admins.

Also, from the Task 19 review of `eb13f22`:

- 3: notices get the coloured treatment of `EmergencyBanner`: a `QFrame` named
  `wardNotice` with a stylesheet rule.
- 4: rebuild the column only when the set of notices differs.
- 5: the RequiresUpdate notice is persistent, not dismissible, as the design says.
- 7: the access list shows once, in the label; the tooltip goes.
- 8: the dismissed set is `QHash<QString, QSet<Kind>>` by document id.
- 9: `refresh()` becomes private; `linkRequested` gets a comment; the class comment
  names the copy banner; include `<functional>`.

Finding 6 is Tasks 7 and 9's; finding 10 is the old plan's and goes with it.

Commit as `feat: access notices from the server's history`.

### Task 7: Share a document

**Files:** `src/widgets/MainWindow.cpp`, `src/widgets/AccessDialog.cpp`,
`src/services/DocumentManager.h/.cpp`, `src/services/FileOpenRouter.h/.cpp`,
`src/main.cpp`, `installer.iss`, `packaging/macos/Info.plist`, tests

`AccessDialog::shareRequested` calls `DocumentManager::share()`, which calls
`DocumentSession::share()`. On `shared`, show the invite link with a Copy button. The
link is `wardplanning:open/<document id>`: register the `wardplanning` URL scheme in
`installer.iss` and `packaging/macos/Info.plist`, and route a launch carrying one through
the single-instance handoff to the same place a `.wardplanning` file for that ward goes.
On `shareFailed`, a message per error (`IdTaken` is a plain failure: 128-bit ids do not
collide). Sharing needs the server; offline says to connect first.

Commit as `feat: share a ward`.

### Task 8: Request and grant access

**Files:** `src/services/DocumentManager.h/.cpp`, `src/widgets/MainWindow.cpp`,
`src/widgets/AccessDialog.h/.cpp`, a grant dialog, tests

**Request.** Opening an invite link or a `.wardplanning` file for a ward this device
lacks offers *Request access*, replacing the `RequestAccess` placeholder in
`MainWindow::openPath`. One click calls `requestAccess`; the ward list shows the ward as
*Requested* and polls `myRequestStatus` on the ordinary head interval; on Granted it
fetches through `DocumentLibrary::fetchWard` and the ward shows *Waiting for access*
until a version seals this device; on Declined it says so and drops the entry.

**Grant.** For an admin, the access dialog lists pending requests from
`accessRequests`. Grant calls `grant`; on success the request's device keys join the
record's `granted` list and `SealingQueue::sealEverywhere()` runs. A request whose
display name matches a current member is flagged: *Bob Wilson already has access. If
this is Bob's new phone, Bob should link it from his other device instead.* Decline
calls `declineRequest`. A member sees requests but no Grant.

Commit as `feat: request and grant access`.

### Task 9: Linking and unlinking devices

**Files:** `src/widgets/FirstRunDialog.h/.cpp`, a link dialog,
`src/widgets/AccessDialog.cpp`, `src/widgets/WardNotices.cpp`,
`src/services/IdentityService.h/.cpp`, `src/services/DocumentManager.h/.cpp`, tests

**The new device.** First run's **Yes** calls `startLink` and shows the code with the
instruction *On a device you already use, choose Link a device and enter this code.* It
polls `link(code)`; once `approvedAs` arrives it calls
`IdentityService::joinPerson(member, displayName, deviceName)`, then restores the ward
list. Wards not yet sealed to it show *Waiting for access from your other device*. An
expired code offers to start again. This replaces the **Yes** placeholder in
`FirstRunDialog::onLinkDevice`.

**The approving device.** *Link a device* (the access dialog's `linkRequested` and the
notice's `WardNotices::linkRequested`) asks for the code, reads `link(code)`, shows
*Link "Pixel 8" to your account?*, and on confirmation calls `approveLink`, appends the
device's keys to `linkedDevices`, and runs `sealEverywhere()`.

**Unlink.** On one of your own devices in the access dialog (`unlinkRequested`), confirm,
listing every device the server records as linked by it - *This phone also linked "Dell
Laptop" on 3 September. Unlink it too?* with **Unlink both** the default - then call
`unlinkDevice` for each and drop each from `linkedDevices`. Your last device has no
Unlink. An unlinked device, told so on its next call, deletes every ward and returns to
the first-run question.

The in-person invite (`AccessDialog::inviteRequested`) goes with pairing: remove the
signal and its button.

Commit as `feat: link and unlink devices`.

### Task 10: Remove, roles, and leave

**Files:** `src/widgets/AccessDialog.h/.cpp`, `src/services/DocumentManager.h/.cpp`,
`src/widgets/MainWindow.cpp`, tests

Each is one server call from the access dialog, admins only where the design says, with
the dialog's explanation of what removal does and does not do:

- **Remove** (`removeRequested`): `removeMember`. The member disappears from the list at
  the next head; the next publish prunes and rotates.
- **Make admin / make member** (`roleRequested`): `setRole`. `LastAdmin` says to make
  someone else an admin first.
- **Leave** (`leaveRequested`): confirm; the last admin with others present first picks
  a successor (*Before you leave, choose who will be admin*), which calls `setRole` then
  `removeMember` on itself. On success the ward's files and recorded copies go at once.
  Offline says to connect first.
- A member pressing an admin action, which the dialog does not offer, would get
  `NotAdmin`; map it to *Only an admin can change who has access* with the admins' names.

Commit as `feat: remove people, change roles, and leave`.

---

## Phase 4 - Messages, update notice, and documentation

### Task 11: Failure path messages

**Files:** `src/widgets/MainWindow.cpp`

One message per `LoadFailure`, per `ServerError`, and per `LostAccess::How`, following
the design's *Failure paths* table. Load failures and a failed local save are modal; sync
states are never modal. Startup restores the last ward by id and reports a failure,
leaving the record alone.

Commit as `fix: explain load, save, and access failures`.

### Task 12: Developer JSON export

**Files:** `src/widgets/MainWindow.cpp`

A menu action behind `AppSettings::developerExport`, off by default, writing
`Document::toJson` indented, with a warning that the file holds unencrypted member data.

Commit as `feat: developer decrypted JSON export`.

### Task 13: Update notice

**Files:** `src/services/UpdateChecker.h/.cpp`, `src/main.cpp`, `CMakeLists.txt`,
`src/widgets/MainWindow.h/.cpp`, `src/widgets/WardNotices.cpp`,
`tests/services/UpdateCheckerTest.h/.cpp`

The check reads GitHub Releases, independent of the Ward Planning server:
`GET https://api.github.com/repos/<owner>/<repo>/releases/latest`, comparing `tag_name`
(leading `v` stripped) with the running version (`WARD_PLANNING_VERSION`) through
`QVersionNumber`. On startup and at most once a day, the last check kept in
`AppSettings`; silent on any failure. The notice sits in `WardNotices` below the
emergency banner - *A new version of Ward Planning is available.* - with **Download**
(`QDesktopServices::openUrl` on the release page) and **Not now**, hidden until a newer
version. When the server has been unreachable a day or more and a newer version is known,
the sync indicator's *Server unreachable* adds *a newer version is available*. It also
runs when `DocumentManager::requiresUpdateChanged` fires. `UpdateChecker` takes an
injectable fetch so its tests need no network.

**Tests:** a newer tag raises the notice; equal or older does not; `v1.10.0` is newer
than `v1.9.0`; malformed JSON and a network failure are silent; a check within a day is
skipped; Not now on 1.2.0 stays hidden for 1.2.0 and reappears for 1.3.0.

Commit as `feat: notify when a newer version is available`.

### Task 14: Documentation

**Files:** `docs/USER_GUIDE.md`, `docs/ARCHITECTURE.md`, `docs/BUILDING.md`,
`README.md`, `CLAUDE.md`

- **USER_GUIDE**: sharing and the invite link; requesting, granting, and removing access;
  admins and members; linking a phone or computer with a code, and unlinking a lost one;
  why every access change is announced; the single-holder and single-admin notices; the
  sync indicator's states, *Waiting for access* included; working offline; that
  responders cannot see each other's work during an outage; and prominently, **that
  access must be granted before an emergency, because granting needs the server**. What
  the server knows - names, roles, devices, and who published when, never the content.
  That reports are plaintext PDFs, and that emergency archives are gone.
- **ARCHITECTURE**: the services as they now stand, the single write path, save versus
  publish, the sealing-set rule, and the split between server-held access and
  client-held keys.
- **README**: product name, extension, dependencies.
- **CLAUDE.md**: remove the Active Tasks row if one was added.

Delete this plan once verification passes.

Commit as `docs: encrypted shared documents`.

---

## Verification before calling it done

`FakeServer` exists only inside the test executable. Items 1, 8, and 11 need no server;
the rest need the companion server, and their behaviour is already covered against the
fake by the automated tests.

Ask the user to confirm, in the running app:

1. A new ward is created, saved, and reopened from the ward list.
2. Sharing a ward produces an invite link.
3. A second identity (a second Windows account) opens the link, requests access, and is
   granted it; it shows *Waiting for access* until sealed, then both can edit.
4. An admin removes a person; the removed person's app deletes its copy with a message
   naming the admin the next time it connects, and the next publish leaves the removed
   device unable to read. A member has no Remove. An admin makes a member an admin, who
   can then grant. Every member sees the changes in one notice.
5. A member leaves; their copy disappears at once, the others still open the ward, and
   the leaver's second device says they left. The last admin leaving is asked for a
   successor. The single-admin notice shows while one admin remains.
6. A person links a second device with a code; both open the ward; unlinking the second
   from the first deletes its copies on its next connection and returns it to the first
   screen, and the first keeps working.
7. Two identities editing at once produce the conflict dialog, naming the other, and
   *Save mine as a copy* leaves both sets of work intact.
8. Starting an emergency, ending it, and generating the summary PDF work with no archive
   prompt.
9. With the network off, two identities record contact results during an emergency;
   reconnecting keeps every attempt from both.
10. Clearing application data and reopening restores the ward list from the server once
    the device is linked again, and the wards download.
11. A build below the latest GitHub release shows the update notice, and Download opens
    the release page.

Do not claim the feature works until the user reports these results.
