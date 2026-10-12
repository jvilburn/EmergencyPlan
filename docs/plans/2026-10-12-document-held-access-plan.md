# Document-Held Access Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** The ward is its own access list. The server stores versions and keeps the
lease; who holds the key, their names, roles, and devices live in the encrypted payload;
people join through one-time invite links.

**Architecture:** `SealingSet` becomes a roster: people with names and roles, each with
devices, plus open invites. The server contract shrinks to five calls with signed writes
checked against the current header. `DocumentSession` gains `invite` and a static `join`.
`AccessDialog`, `WardNotices`, and lost-access detection read the roster and its diffs.
Everything that kept a server-side list in step is deleted.

**Tech Stack:** Qt 6 / C++17, libsodium through `DocumentCrypto`, the fake server in
tests, QtTest.

**Design:** `docs/plans/2026-10-12-document-held-access-design.md`. Read it first.

**Companion server:** does not exist yet. `ServerConformanceTest` is what it must pass.

---

## Shape

Core first. Task 1 is the roster, the contract, the fake server, and the join and invite
flow in the session, as one artifact. The tree will not build until Task 3. Then the
user's own review, then the app and the tests on top, then the deletions' last traces and
the docs.

---

## Phase 1 - The core

### Task 1: Roster, signed contract, fake server, invite and join

**Files:**
- Modify: `src/models/SealingSet.h/.cpp`
- Modify: `src/services/CryptoTypes.h` (if a new type is needed), `src/services/DocumentCrypto.h/.cpp`
- Modify: `src/services/ServerContract.h/.cpp`
- Modify: `tests/services/FakeServer.h/.cpp`
- Modify: `src/services/ServerConnection.h/.cpp`
- Modify: `src/services/HttpWire.h/.cpp`, `src/services/HttpServerApi.h/.cpp`
- Modify: `src/services/ServerVersion.h/.cpp`, `src/services/Reconcile.h/.cpp`
- Modify: `src/services/DocumentSession.h/.cpp`
- Modify: `src/services/DeviceIdentity.h/.cpp`, `src/services/DocumentRecords.h/.cpp`
- Modify: `src/services/FileOpenRouter.h/.cpp`
- Delete: `src/services/IncomingWards.h/.cpp`, `src/services/DeviceLinks.h/.cpp`

#### 1.1 The roster

`SealingSet` keeps its name, its place in the payload, its former keys, and its
`next` rule, and becomes a roster:

```cpp
/// One person with access: their devices hold the key.
struct Person
{
    MemberId id;
    QString name;
    Role role = Role::Member;
    QList<NamedDevice> devices;         ///< sorted by fingerprint hex
};

/// A device of a person: its keys and the name it was given.
struct NamedDevice
{
    QString name;
    DeviceKeys keys;
};

/// A one-time invite: a keypair the data key is sealed to until someone joins with it.
struct Invite
{
    enum class For
    {
        Device,     ///< joins person as another device
        User        ///< makes a new person
    };
    For purpose = For::User;
    MemberId person;                    ///< the inviter, for Device; the inviter too for User, as the attribution
    Role role = Role::Member;           ///< the role the joiner gets
    QString label;                      ///< "for Bob", shown in the roster while open
    DeviceKeys keys;
};
```

```cpp
class SealingSet
{
public:
    static SealingSet of(const DeviceIdentity& first);     ///< a new ward: its maker, an admin

    // Reading
    QList<Person> people() const;
    std::optional<Person> person(const MemberId& id) const;
    std::optional<Person> personOf(const Fingerprint& device) const;
    QList<Invite> invites() const;
    bool contains(const Fingerprint& fingerprint) const;   ///< a device or an invite
    bool isEmpty() const;
    QList<DeviceKeys> recipients() const;                  ///< every device and invite, sorted by fingerprint hex: the header's entries
    int adminCount() const;

    // Writing, each refused (false) when it would leave the set inconsistent
    bool addInvite(const Invite& invite);
    bool removeInvite(const Fingerprint& invite);
    /// The joiner's device replaces the invite: under the invite's person for a device
    /// invite, under a new person named by the joiner for a user invite.
    bool join(const Fingerprint& invite, const NamedDevice& device, const MemberId& newPerson, const QString& name);
    bool removePerson(const MemberId& id);
    bool removeDevice(const Fingerprint& device);          ///< never a person's last; removePerson for that
    bool setRole(const MemberId& id, Role role);

    // The rule, as now but with the allow-list being the previous header
    struct Next { SealingSet set; bool rotate = false; };
    /// Keeps what the previous header still names, drops what it does not, and
    /// rotates when anything was dropped. There is no "approved" list any more:
    /// additions are made by addInvite and join on the document itself.
    Next next(const QSet<QString>& previousHeader) const;

    void addFormerKey(const DataKey& key);
    bool descendsFrom(const DataKey& key) const;

    QJsonObject toJson() const;
    static std::optional<SealingSet> fromJson(const QJsonObject& json);
    bool operator==(const SealingSet& other) const;
};
```

`Role` and `MemberId` move from `ServerContract` to `CryptoTypes`/`SealingSet` since the
server no longer knows them. `Reconcile::headerMatchesSealingSet` compares the header's
recipients with `recipients()`. The JSON is `{people: [...], invites: [...], formerKeys:
[...]}`, read strictly as now; a format version bump in `DocumentHeader`.

**The rule simplifies.** Today `next(allowList, approved)` adds approved devices the
allow-list names. Now the roster is edited directly (`addInvite`, `join`, `removePerson`,
`setRole`) and `next(previousHeader)` only prunes and decides rotation: a device or
invite in this set that the previous header did not name is one this write adds, which
is what the writer intended; one the previous header named that this set lacks is one
this write removes, so rotate. The "allow-list" is the header of the version the write
builds on, which every writer holds.

#### 1.2 The contract

`ServerContract.h` keeps `ServerError` (trimmed), `Result`, `Reply`, `Done`,
`Publication`, `Head` (without `members` and `history`), `Snapshot`, `Upload`, `Lease`.
It gains:

```cpp
/// Who signs a write: the device's two public keys, whose fingerprint the current
/// header must name, and an Ed25519 signature over what the call changes.
struct Signer
{
    DeviceKeys keys;
    QByteArray signature;
};

/// What a write's signature covers: the call's name, the document id, the base
/// version, and the BLAKE2b hash of the header and blob. One function builds it on
/// both sides.
QByteArray signedBytes(QLatin1String call, const QString& documentId, qint64 baseVersion, const Upload& upload);
QByteArray signedBytes(QLatin1String call, const QString& documentId);     ///< lease calls
```

```cpp
class ServerApi
{
public:
    /// Stores version 1. Anyone; rate-limited by address. The header must name the
    /// signer. Answers the version.
    virtual void registerDocument(const Signer& signer, const Upload& upload, Reply<qint64> reply) = 0;

    virtual void head(const QString& documentId, Reply<Head> reply) = 0;
    virtual void download(const QString& documentId, Reply<Snapshot> reply) = 0;

    /// Conditional on baseVersion. The signer must be named by the current header,
    /// else NotAKeyHolder. Answers the new version.
    virtual void upload(const Signer& signer, const QString& documentId, qint64 baseVersion, const Upload& upload, Reply<qint64> reply) = 0;

    virtual void takeLease(const Signer& signer, const QString& documentId, Reply<Lease> reply) = 0;
    virtual void releaseLease(const Signer& signer, const QString& documentId, Reply<Done> reply) = 0;
};
```

`ServerError` becomes: `None, Unreachable, NotAKeyHolder, UnknownDocument,
VersionConflict, LeaseHeld, IdTaken, RateLimited, InvalidRequest`. `LostAccess` leaves the
contract: the client derives why it lost access from the roster (1.6).

`ServerConnection` loses sign-in, sessions, registration, and the unlinked handler. It
becomes the signer: it holds the identity, builds `Signer` for each write with
`DocumentCrypto::sign`, and forwards reads as they are. Its lifetime guard and
context-bound replies stay.

`HttpWire`: the signer travels as headers `X-Signer-Public`, `X-Signer-Verify`,
`X-Signature` (base64) on `POST /documents`, `PUT /documents/{id}`, and the lease calls;
no `Authorization`. The header in a request body is its compact JSON as a string, and the
signature's hash is over that string as sent. The table in `HttpWire.h` shrinks to the
five calls.

`DocumentCrypto` gains `sign(bytes, SigningKey)` and `verifies(bytes, signature,
VerifyKey)` (rename of the challenge pair) and `keypairFromSeed(seed)` for X25519
(`crypto_box_seed_keypair`), so an invite seed yields both keypairs:
`sealing = keypairFromSeed(hash(InviteSealing, seed))`, `signing =
signingKeypairFromSeed(hash(InviteSigning, seed))`, two new `HashDomain`s.

#### 1.3 The fake server

`FakeServer` keeps `StoredDocument` (version, header, blob, published, lease,
forceConflict, served overrides) keyed by id, and drops people, links, members, requests,
history, sessions, challenges, and unlinking. Each write: parse the current header,
compute the signer's fingerprint, refuse `NotAKeyHolder` unless named, verify the
signature over `signedBytes`, refuse `InvalidRequest` on a bad one, then the checks it
has today (id, blob size, base version, recipients non-empty). `registerDocument` checks
the uploaded header names the signer. Reads need nothing. Rate limits: uploads per
signer per hour as now; registrations and reads per address are the real server's and
are described in the contract comment only.

#### 1.4 Invite and join

In `DocumentSession`:

```cpp
    /// Writes a version sealed to a fresh invite keypair as well, and answers the
    /// link, or why not. Admins only for User, which the caller checks; the session
    /// checks nothing about roles.
    using Invited = std::function<void(const std::optional<QString>& link, ServerContract::ServerError error)>;
    void invite(Invite::For purpose, const QString& label, QObject* receiver, Invited done);

    /// Joins the ward an invite link names: fetches the current version, unseals the
    /// data key with the invite's keys, validates as a download is validated, writes
    /// the next version with this device in the invite's place, signed by the invite,
    /// and writes the local file and base. Then the caller opens it as any ward.
    enum class Join { Done, Unreachable, NeedsNewerVersion, InviteUsed, UnknownWard, ServerFault, WriteFailed };
    using Joined = std::function<void(Join outcome, const QString& reason)>;
    static void join(const Context& context, const InviteLink& link, const DocumentPaths& paths, QObject* receiver, Joined done);
```

`InviteLink{documentId, seed}` and its text form live in `FileOpenRouter`:
`wardplanning:join/<id>/<seed base64url>`; `inviteMessage(name, wardName, link)` builds
the text the person copies, with `UpdateChecker::releasePage()`.

`invite` is a publish with one more recipient: apply `addInvite` to the document's
roster, save, publish; on success answer the link. It needs the lease like any
directory write? No: the roster is not directory content and its merge is by key; treat
roster edits like emergency responses, allowed without the lease, and have
`ResponseMerge` merge rosters by person id (last writer wins per person; a removal wins
over an edit). Document this in the merge.

`join` runs on a device that has no record of the ward: it uses `ServerVersion::fetch`
with the invite's `Reconcile::Self`, then `SealingSet::join`, `DocumentFile::seal` with a
rotated key (a join is an addition, no rotation, but the invite entry is removed, so the
rule says rotate; accept that: one rotation per join is cheap and means the used link
unseals nothing afterwards), uploads signed by the invite's signing key, and on success
writes file, base, and record exactly as `download` does.

#### 1.5 Share

`share()` registers version 1 signed by this device; the roster is `SealingSet::of(this
device)` already, so nothing changes but the signer.

#### 1.6 Lost access, from the roster

`DocumentSession::onHead`: a header that does not name this device means access is
lost. Why: compare the roster this session last held with `head.published`: if this
person is absent from the previous roster's view of the publisher's own removal... the
session cannot read the new roster (it lacks the key), so it reads the *previous* one it
holds: the publisher's fingerprint maps to a person there; if the publisher is this
person, it is a leave or an unlink by one's own other device (`Left` if this person's
every device is gone, which the session cannot know; say "removed from your other
device"); otherwise `Removed` by that person's name. `LostAccess{how, byWhom}` stays a
client type in `DocumentSession.h`.

The Open Ward list and `DocumentLibrary` lose `restore`; a closed ward that was taken
away is found when it is opened (its head lacks this device). The design accepts that.

#### 1.7 Identity and records

`DeviceIdentity` keeps `sealing`, `signing`, `member`, `displayName`, `deviceName`; loses
`registration()`. `DocumentRecord` loses `historySeen`; gains `rosterShown`, the
version whose roster the notices last showed. `DocumentRecords` loses `linkedDevices`.
`AppSettings` loses `requestedWards`, `linkedDevices`.

#### 1.8 Deletions in this task

`IncomingWards`, `DeviceLinks`, `ServerVersion::buildResealing` and `Resealing`,
`ServerVersion::Access` and `accessFor` (the rule takes the previous header),
`FileOpenRouter::Action::RequestAccess` and `routeInvite` (replaced by a `Join` action
carrying the link), the people/link/request/history/me types and calls in
`ServerContract`, `HttpWire`, `HttpServerApi`, `ServerConnection`, `FakeServer`.
`CMakeLists.txt` follows.

Commit as `feat: the ward is its own access list`. The library target need not link
until Task 3.

### Task 2: STOP - user review of the core

Hand over the diff. No test build, no reviewer, no Task 3 until the user has read it.

---

## Phase 2 - The app on the core

### Task 3: Window, dialogs, notices

**Files:** `src/widgets/MainWindow.h/.cpp`, `AccessDialog.h/.cpp`, `WardNotices.h/.cpp`,
`FirstRunDialog.h/.cpp`, `OpenWardDialog.h/.cpp`, `SyncStatusWidget.cpp`,
`src/services/DocumentManager.h/.cpp`, `src/main.cpp`

- **AccessDialog** reads `session->document().sealingSet()`: people with role and
  devices, open invites with their labels. Buttons: *Add a Device...* (everyone),
  *Add a User...* (admins), *Remove* (admins, a person; everyone, their own device),
  *Make Admin / Make Member* (admins), *Revoke Invite*, *Leave...*, *Details*. Signals
  carry what the manager needs; the manager edits the roster through
  `DocumentSession::edit(Edit::Roster, ...)` for removals and roles, and `invite` for
  invites. The last-admin rule is checked here and in the manager, as now.
- **Add a Device / Add a User** ask for a label (prefilled "for ..."), call `invite`,
  and show the message with a Copy button, as the invite-link dialog does today.
- **Opening a join link** (`handleArguments` through `FileOpenRouter`) calls
  `DocumentSession::join` through the manager, adds the ward to the library on `Done`,
  and opens it; each `Join` outcome has its message from the design's table.
- **WardNotices**: the access-changed notice comes from
  `RosterDiff::describe(previous, current)`, a small pure function in
  `src/models/RosterDiff.h/.cpp`: joined (as whom, by whose invite), removed, left,
  role changed, device added or removed, invite made or revoked. The previous roster is
  the one at `record.rosterShown`, kept as the base copy's roster at that version; the
  simplest store is the roster JSON in the record. Dismiss sets `rosterShown` to the
  current version. The single-holder, single-admin, one-device, requires-update, and
  update-available notices stay.
- **FirstRunDialog**: one page, name and device name. The link page goes.
- **OpenWardDialog**: library entries only.
- **MainWindow**: the Ward menu keeps *Who Has Access*; messages for `Join`, for a
  refused invite (`NotAKeyHolder` means this device lost access meanwhile), for the
  roster edits' publish failures (they are ordinary publishes, so the sync indicator
  carries them). `onAccessCallFailed`, `onLinkFailed`, `onGrantBlocked`, request and
  link handlers, `unlinkedHere` through `setUnlinkedHandler` go; lost access on an
  unlink arrives through the session's `accessLost` as `Unlinked`.
- **main.cpp**: no `IncomingWards`, `DeviceLinks`, or unlinked handler.

Commit as `feat: who has access, from the ward itself`.

### Task 4: Tests

**Files:** `tests/services/*` as listed.

- Delete `IncomingWardsTest`, `DeviceLinksTest`, `ServerConnectionTest`'s sign-in
  cases (keep the context-lifetime one), `FakeServerTest`'s people and link cases.
- `SealingSetTest`: roster round trip; `join` for both invite kinds; `removeDevice`
  refuses a last device; `next` rotates on a removal and not on an addition; a set with
  two people sharing a device key is refused.
- `ServerConformanceTest`: an upload by a key the header names lands (version pinned);
  by one it does not name is `NotAKeyHolder`; a bad signature is `InvalidRequest`; a
  register whose header lacks the signer is refused; reads need no signer; lease by a
  non-holder refused.
- `DocumentSessionTest`: `invite` adds a recipient and the roster entry (version
  pinned); `join` as a user and as a device, each opening on the joiner and showing in
  the inviter's roster at the next head; a used link gives `InviteUsed`; a removal
  rotates and the removed device's next head reports `Removed` by name; a leave reports
  `Left`; a second device joining and then being removed by its owner reports
  `Unlinked`.
- `RosterDiffTest`: each kind of change, pinned text.
- `DocumentManagerTest`: the roster edits through the manager; the last-admin refusal.
- `FileOpenRouterTest`: join links parse, bad seeds and ids do not, the message text.

Every test pins observed values; every absence is anchored. Commit as
`test: document-held access`.

---

## Phase 3 - Docs and the last traces

### Task 5: Documentation

- `docs/plans/2026-08-30-encrypted-shared-document-design.md`: the server section,
  identity and linking, granting, removing, failure paths, component table, invariants,
  capacity: rewrite against the new design note, or replace those sections with a
  pointer to it and keep the format and sync sections.
- Delete `2026-10-11-server-held-access-plan.md` and `2026-10-11-atomic-grant-plan.md`:
  superseded before verification.
- `USER_GUIDE.md`: *Sharing a Ward* rewritten: Add a User and Add a Device, the invite
  message, what the role means (a convention, not a lock), removing, leaving, notices.
- `ARCHITECTURE.md`: the Shared Wards section and service table.
- `README.md`: the server's five calls in a sentence; what the server knows (nothing
  about who).
- `HttpWire.h` table verified.

Commit as `docs: document-held access`.

---

## Verification before calling it done

- Full suite green; totals pinned in the report.
- `grep -rn "IncomingWards\|DeviceLinks\|Resealing\|accessRequests\|registerPerson\|approveLink\|LostAccess::How::Unlinked" src tests` finds only what Task 1 kept on purpose.
- The companion server, once written, passes `ServerConformanceTest` with
  `WARD_PLANNING_SERVER_URL` set.
- In the running app, with two identities: share; Add a User, send the message by text,
  open it on the other device, see the ward; Add a Device to yourself; Remove; Leave;
  the notices; offline editing and merge. Do not claim it works until the user reports.
