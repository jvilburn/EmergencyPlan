# Atomic Grant and Approve Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** A grant or a link approval carries the resealed version with it, so the server
adds the member or device and stores the version they can read in one step, and nothing
is ever owed to anyone afterwards.

**Architecture:** `grant` and `approveLink` gain a body: the current version of each
affected ward, fetched, resealed by the sealing-set rule with the new device among the
approved, and uploaded conditional on the version it was built on. The server applies
the same checks as `PUT /documents/{id}` and commits membership and version together, or
refuses both. With that, `SealingQueue`, the record's `granted` list, and the startup
`sealEverywhere()` have nothing left to do and go. `IncomingWards` polls only while a
request is pending or a ward is waiting, checks waiting wards by `head` rather than by
downloading them, and learns from `me` which held wards this person has lost.

**Tech Stack:** Qt 6 / C++17, libsodium through `DocumentCrypto`, the fake server in
`tests/services/FakeServer`, QtTest.

**Companion server:** outside this repo. `ServerConformanceTest` is the contract; the
server must pass it after this change. Section 1.1 below is what it has to implement.

---

## Why

Today a grant succeeds on the server before any version the new member can read exists.
The admin's device then owes a seal, remembers it in `DocumentRecord::granted`, retries it
through `SealingQueue` on timers, finishes it at the next launch, and settles the record
when the version lands. The new device meanwhile polls the server once a minute,
downloading the full snapshot of every ward it is listed in until one validates, and
shows *Waiting for access*. A link approval works the same way for every ward the
approver holds.

The edit rate is very low, so an upload built on the current version almost never
conflicts. Carrying the version with the call that causes it removes the owed state, the
retry subsystem, and the waiting state for grants. The design's two bindings are
untouched: the header stays the payload's associated data and the sealing set stays
inside the payload, so the server still cannot add a recipient.

What changes for a person: a granted request is readable the moment it is granted. A
linked device gets every ward its approver held at that moment, at that moment. *Waiting
for access* remains only for a ward the approver did not hold at approval time, which
arrives when the approver next writes it.

---

## Phase 1 - The core

### Task 1: Contract, server, builder, and the two callers

**Files:**
- Modify: `src/services/ServerContract.h`
- Modify: `src/services/HttpWire.h`, `src/services/HttpWire.cpp`
- Modify: `src/services/HttpServerApi.h`, `src/services/HttpServerApi.cpp`
- Modify: `src/services/ServerConnection.h`, `src/services/ServerConnection.cpp`
- Modify: `tests/services/FakeServer.h`, `tests/services/FakeServer.cpp`
- Modify: `src/services/ServerVersion.h`, `src/services/ServerVersion.cpp`
- Modify: `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Modify: `src/services/DeviceLinks.h`, `src/services/DeviceLinks.cpp`
- Modify: `src/services/DocumentSession.cpp` (the `accessFor` call)
- Modify: `src/services/DocumentRecords.h`, `src/services/DocumentRecords.cpp`
- Modify: `src/main.cpp`, `CMakeLists.txt`
- Delete: `src/services/SealingQueue.h`, `src/services/SealingQueue.cpp`

This task is the whole core, as one artifact. The tree will not link until Task 3
(tests) catches up; that is expected. Do not build the tests before Task 2.

#### 1.1 The server contract

In `ServerContract.h`, beside `Upload`:

```cpp
/// A version of a ward resealed to admit someone, carried by the call that admits them:
/// grant, for the ward being granted, and approveLink, one per ward the approver holds.
/// Stored as `PUT /documents/{id}` would store it, conditional on baseVersion, in the
/// same step as the membership change, or refused with it.
struct Resealing
{
    QString documentId;
    qint64 baseVersion = 0;
    Upload upload;
};
```

Replace the two virtuals:

```cpp
    /// Adds the link's device to the caller's person and stores each resealing as an
    /// upload by the caller, with the new device on every allow-list. All or nothing:
    /// a resealing that fails any upload check refuses the whole call and the device
    /// is not added. Wards of the person that are not among the resealings are left
    /// as they are.
    virtual void approveLink(const Session& session,
                             const QString& code,
                             const QList<Resealing>& resealings,
                             Reply<Done> reply) = 0;

    /// The request's person becomes a member, and the resealing is stored as an
    /// upload by the caller with that person's devices on the allow-list. All or
    /// nothing: VersionConflict, NotOnAllowList, or an invalid upload refuses the
    /// grant too. The resealing's documentId must be documentId.
    virtual void grant(const Session& session,
                       const QString& documentId,
                       qint64 index,
                       const Resealing& resealing,
                       Reply<Done> reply) = 0;
```

`ServerConnection` mirrors both signatures; its bodies are one-line `call<Done>`
forwards like the ones they replace.

#### 1.2 The wire

`HttpWire.h` table rows:

```
/// | approveLink | `POST /links/{code}/approve` | `{resealings: [{documentId, baseVersion, header, blob}]}` | - |
/// | grant | `POST /documents/{id}/members` | `{request, baseVersion, header, blob}` | - |
```

`HttpWire.cpp`:

```cpp
const QLatin1String KeyResealings("resealings");
const QLatin1String KeyDocumentId("documentId");   // if not already defined

QJsonObject resealingJson(const Resealing& resealing)
{
    QJsonObject json = uploadJson(resealing.upload);
    json[KeyDocumentId] = resealing.documentId;
    json[KeyBaseVersion] = resealing.baseVersion;
    return json;
}

QJsonObject approveLinkRequest(const QList<Resealing>& resealings)
{
    QJsonArray array;
    for (const Resealing& resealing : resealings)
    {
        array.append(resealingJson(resealing));
    }
    QJsonObject json;
    json[KeyResealings] = array;
    return json;
}

QJsonObject grantRequest(qint64 index, const Resealing& resealing)
{
    QJsonObject json = uploadJson(resealing.upload);
    json[KeyRequest] = index;
    json[KeyBaseVersion] = resealing.baseVersion;
    return json;
}
```

Declare both in `HttpWire.h` in place of `grantRequest(qint64)`. `HttpServerApi` passes
them as the bodies of the two existing calls.

#### 1.3 The fake server

`decideUpload` already holds the PUT checks. Split out the part after the caller is
resolved so grant and approve can run it against an allow-list they supply:

```cpp
/// The upload checks shared by PUT, grant, and approve, against the allow-list the
/// caller supplies - for grant and approve, one that already includes who is being
/// added. On success the version is stored. The caller has already been checked.
ServerContract::ServerError FakeServer::storeUpload(StoredDocument& document,
                                                    const Caller& caller,
                                                    qint64 baseVersion,
                                                    const ServerContract::Upload& upload,
                                                    const QSet<QString>& allowList)
{
    if (!withinLimits(m_uploads, caller.device, m_limits.uploadsPerHour, 0))
    {
        return ServerError::RateLimited;
    }
    if (upload.header.documentId != document.id || upload.blob.size() > m_limits.maxBlobBytes
        || upload.header.recipients.isEmpty())
    {
        return ServerError::InvalidRequest;
    }
    if (document.forceConflict || baseVersion != document.version)
    {
        document.forceConflict = false;
        return ServerError::VersionConflict;
    }
    for (const RecipientEntry& entry : upload.header.recipients)
    {
        if (!allowList.contains(entry.fingerprint.toHex()))
        {
            return ServerError::NotOnAllowList;
        }
    }

    ++document.version;
    document.header = upload.header.toJson();
    document.blob = upload.blob;
    document.published = Publication{caller.person->displayName, caller.fingerprint, m_now};
    m_uploads[caller.device.bytes].append(m_now);
    return ServerError::None;
}
```

`decideUpload` becomes: resolve the caller with `allowListed`, then
`storeUpload(*document, caller, baseVersion, upload, allowListOf(*document))`, answering
the version on `None`. (`StoredDocument` needs its `id` if it does not carry one; check.)

`decideGrant(session, documentId, index, resealing)`: after finding the pending request,
and before changing anything, build the allow-list as `allowListOf(*document)` plus the
fingerprints of `m_people.value(member).devices`, run `storeUpload`, and refuse on any
error. Only then set the status, insert the member, record the history entry. The
resealing's `documentId` must equal `documentId`, else `InvalidRequest`.

`decideApproveLink(session, code, resealings)`: after the link checks pass and before
the device is added, for each resealing: find the document (`UnknownDocument` if none);
the caller must be on its allow-list (`NotMember`); build the allow-list as
`allowListOf(*document)` plus the new device's fingerprint; run the checks of
`storeUpload` **without storing**. If every one passes, store them all and add the
device. The simplest way to get all-or-nothing with one check function: give
`storeUpload` a `bool store` parameter, run every resealing with `false`, then again with
`true`. The double pass costs nothing in a fake.

#### 1.4 The builder, in `ServerVersion`

`accessFor` loses the record parameter: the approved devices are whatever the caller
says they are.

```cpp
/// The allow-list a head names, and the devices this device approved for the write:
/// the ones it linked, and for a grant or an approval the one being added.
Access accessFor(const ServerContract::Head& head, const QList<DeviceKeys>& approved);
```

`DocumentSession::access()` calls it with `m_context.records.linkedDevices()`.

Then the builder:

```cpp
/// How building a resealing ended: Accepted with the resealing, or why not. A fetched
/// version that could not be sealed - which only a crypto failure causes - counts as a
/// ServerFault with its reason.
using ResealingBuilt = std::function<void(const Fetched& theirs, const std::optional<ServerContract::Resealing>& built)>;

/// Fetches the server's head and current version of documentId, validates it against
/// trusted as fetch does, applies the sealing-set rule with approved, encrypts, and hands
/// back what grant or approveLink sends. A head that does not list this device is a
/// ServerFault: applying it would seal this device out. Replies come only while receiver
/// lives.
void buildResealing(const Reconcile::Self& self,
                    const QList<DataKey>& trusted,
                    const QList<DeviceKeys>& approved,
                    ServerConnection& server,
                    const QString& documentId,
                    QObject* receiver,
                    ResealingBuilt done);
```

```cpp
namespace
{

void onResealingFetched(const QString& documentId,
                        const ServerVersion::Access& access,
                        const ServerVersion::ResealingBuilt& done,
                        const Fetched& theirs)
{
    if (theirs.outcome != Fetched::Outcome::Accepted)
    {
        done(theirs, std::nullopt);
        return;
    }

    const ServerVersion::Resealed resealed = ServerVersion::reseal(theirs.document, theirs.dataKey, access);
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(resealed.document, theirs.sealed.header, resealed.dataKey);
    if (!sealed.has_value())
    {
        Fetched failed = outcomeOnly(Fetched::Outcome::ServerFault);
        failed.reason = QObject::tr("The version could not be sealed on this device.");
        done(failed, std::nullopt);
        return;
    }

    done(theirs, Resealing{documentId, theirs.version, Upload{sealed->header, sealed->ciphertext}});
}

void onResealingHead(const Reconcile::Self& self,
                     const QList<DataKey>& trusted,
                     const QList<DeviceKeys>& approved,
                     ServerConnection& server,
                     const QString& documentId,
                     QObject* receiver,
                     const ServerVersion::ResealingBuilt& done,
                     const Result<Head>& head)
{
    if (head.error == ServerError::AccessLost)
    {
        Fetched lost = outcomeOnly(Fetched::Outcome::AccessLost);
        lost.lost = head.lost;
        done(lost, std::nullopt);
        return;
    }
    if (!head.ok())
    {
        done(outcomeOnly(Fetched::Outcome::CallFailed), std::nullopt);
        return;
    }
    if (!ServerVersion::listsDevice(head.value.value(), self.fingerprint()))
    {
        Fetched fault = outcomeOnly(Fetched::Outcome::ServerFault);
        fault.reason = QObject::tr("The server's list of who has access leaves out this device.");
        done(fault, std::nullopt);
        return;
    }

    const ServerVersion::Access access = ServerVersion::accessFor(head.value.value(), approved);
    ServerVersion::fetch(self, trusted, server, documentId, receiver,
                         std::bind(&onResealingFetched, documentId, access, done, _1));
}

} // namespace

void buildResealing(const Reconcile::Self& self,
                    const QList<DataKey>& trusted,
                    const QList<DeviceKeys>& approved,
                    ServerConnection& server,
                    const QString& documentId,
                    QObject* receiver,
                    ResealingBuilt done)
{
    server.head(documentId, receiver,
                std::bind(&onResealingHead, self, trusted, approved, std::ref(server), documentId, receiver, done, _1));
}
```

Delete `settleGranted`. `trustedKeys`, `reseal`, `isCurrent`, `listsDevice`, `fetch` stay.

#### 1.5 `DocumentManager::grant`

```cpp
// DocumentManager.h, replacing onGranted's declaration
    void startGrant(const ServerContract::AccessRequest& request, bool mayRetry);
    void onGrantBuilt(const ServerContract::AccessRequest& request,
                      bool mayRetry,
                      const ServerVersion::Fetched& theirs,
                      const std::optional<ServerContract::Resealing>& built);
    void onGranted(const ServerContract::AccessRequest& request,
                   bool mayRetry,
                   const ServerContract::Result<ServerContract::Done>& result);

signals:
    /// A grant could not be prepared on this device: the ward's current version could
    /// not be read here. reason says why, in the person's words.
    void grantBlocked(const QString& reason);
```

Update the `grant` doc comment: "An admin's grant: the open ward's current version,
resealed to the request's device, goes up with the grant, so the person can read the
ward the moment they are a member. A conflict with a publish that landed meanwhile is
built again once."

```cpp
void DocumentManager::grant(const ServerContract::AccessRequest& request)
{
    if (!m_session || m_server == nullptr || !m_session->isShared() || request.person.devices.isEmpty())
    {
        return;
    }
    startGrant(request, true);
}

void DocumentManager::startGrant(const ServerContract::AccessRequest& request, bool mayRetry)
{
    const QString documentId = m_session->documentId();
    const QList<DeviceKeys> approved = m_records->linkedDevices() + QList<DeviceKeys>{request.person.devices.first().keys};
    ServerVersion::buildResealing(m_identity.self(),
                                  ServerVersion::trustedKeys(documentId, m_library->pathsFor(documentId), *m_records, m_identity.self()),
                                  approved,
                                  *m_server,
                                  documentId,
                                  this,
                                  std::bind(&DocumentManager::onGrantBuilt, this, request, mayRetry, _1, _2));
}

void DocumentManager::onGrantBuilt(const ServerContract::AccessRequest& request,
                                   bool mayRetry,
                                   const ServerVersion::Fetched& theirs,
                                   const std::optional<ServerContract::Resealing>& built)
{
    switch (theirs.outcome)
    {
    case ServerVersion::Fetched::Outcome::Accepted:
        break;
    case ServerVersion::Fetched::Outcome::AccessLost:
        emit accessCallFailed(ServerContract::ServerError::AccessLost);
        return;
    case ServerVersion::Fetched::Outcome::CallFailed:
        emit accessCallFailed(ServerContract::ServerError::Unreachable);
        return;
    case ServerVersion::Fetched::Outcome::NeedsNewerVersion:
        emit grantBlocked(tr("This ward's current version was written by a newer Ward Planning."));
        return;
    case ServerVersion::Fetched::Outcome::ServerFault:
        emit grantBlocked(theirs.reason);
        return;
    }

    if (!m_session || m_session->documentId() != built->documentId)
    {
        return;
    }
    m_server->grant(built->documentId,
                    request.index,
                    built.value(),
                    this,
                    std::bind(&DocumentManager::onGranted, this, request, mayRetry, _1));
}

void DocumentManager::onGranted(const ServerContract::AccessRequest& request,
                                bool mayRetry,
                                const ServerContract::Result<ServerContract::Done>& result)
{
    // Someone published between the fetch and the grant: once more on their version.
    if (result.error == ServerContract::ServerError::VersionConflict && mayRetry && m_session)
    {
        startGrant(request, false);
        return;
    }
    onAccessCall(result);
}
```

The version the grant stored is this device's own publish of the server's version. The
open session's next head check adopts it: `adopt` three-way-merges against the base, the
content is unchanged, so local edits survive and the new sealing set is taken, as with any
other device's publish. Nothing in `DocumentSession` changes for this.

Remove from `DocumentManager`: the `SealingQueue&` constructor parameter and `m_queue`,
every `setOpenSession` call, `onQueueLost`'s connection (the function itself is rewired in
Task 4; keep it), and the old `onGranted`. `MainWindow` connects `grantBlocked` to a
`QMessageBox::warning(this, tr("Who Has Access"), reason)`.

#### 1.6 `DeviceLinks::approve`

The constructor becomes
`DeviceLinks(const DeviceIdentity& identity, DocumentRecords& records, ServerConnection& server, std::function<DocumentPaths(const QString&)> pathsFor, QObject* parent)`;
the `SealingQueue&` goes. Class comment:

```cpp
/// This person's devices, from a device they already use: approving a new one's link
/// code, and unlinking one that is lost or retired.
///
/// Approving sends, with the approval, every ward this device holds resealed to the new
/// device, so the server adds the device and stores those versions together. The device
/// is then remembered in linkedDevices, so every later write by this device seals to it
/// too: that is how it reaches a ward this device did not hold at approval time. No other
/// device seals it, so a device the server merely lists under this person never receives
/// a key.
```

```cpp
// DeviceLinks.h, private
    /// One approval in progress: the wards still being built, and those built.
    struct Approval
    {
        QString code;
        DeviceKeys device;
        bool mayRetry = true;
        int building = 0;
        QList<ServerContract::Resealing> built;
    };

    void startApproval(const QString& code, const DeviceKeys& device, bool mayRetry);
    void onBuilt(const ServerVersion::Fetched& theirs, const std::optional<ServerContract::Resealing>& built);
    void sendApproval();
    void onApproved(const ServerContract::Result<ServerContract::Done>& result);

    DeviceIdentity m_identity;
    std::function<DocumentPaths(const QString&)> m_pathsFor;
    std::optional<Approval> m_approval;
```

```cpp
void DeviceLinks::approve(const QString& code, const DeviceKeys& device)
{
    if (m_approval.has_value())
    {
        return;
    }
    startApproval(code, device, true);
}

void DeviceLinks::startApproval(const QString& code, const DeviceKeys& device, bool mayRetry)
{
    m_approval = Approval{code, device, mayRetry, 0, {}};
    const QList<DeviceKeys> approved = m_records.linkedDevices() + QList<DeviceKeys>{device};
    for (const QString& documentId : m_records.documentIds())
    {
        if (!m_records.record(documentId).baseVersion.has_value())
        {
            continue;
        }
        ++m_approval->building;
        ServerVersion::buildResealing(m_identity.self(),
                                      ServerVersion::trustedKeys(documentId, m_pathsFor(documentId), m_records, m_identity.self()),
                                      approved,
                                      m_server,
                                      documentId,
                                      this,
                                      std::bind(&DeviceLinks::onBuilt, this, _1, _2));
    }
    if (m_approval->building == 0)
    {
        sendApproval();
    }
}

void DeviceLinks::onBuilt(const ServerVersion::Fetched& theirs, const std::optional<ServerContract::Resealing>& built)
{
    // A ward whose version could not be built here is left out; this device seals it
    // on its next write of that ward, and the new device waits for access until then.
    Q_UNUSED(theirs);
    if (built.has_value())
    {
        m_approval->built.append(built.value());
    }
    if (--m_approval->building == 0)
    {
        sendApproval();
    }
}

void DeviceLinks::sendApproval()
{
    m_server.approveLink(m_approval->code, m_approval->built, this, std::bind(&DeviceLinks::onApproved, this, _1));
}

void DeviceLinks::onApproved(const Result<Done>& result)
{
    const Approval approval = m_approval.value();
    m_approval.reset();

    // Someone published to one of the wards meanwhile: once more on their versions.
    if (result.error == ServerError::VersionConflict && approval.mayRetry)
    {
        startApproval(approval.code, approval.device, false);
        return;
    }
    if (!result.ok())
    {
        emit failed(result.error);
        return;
    }

    QList<DeviceKeys> linked = m_records.linkedDevices();
    if (!linked.contains(approval.device))
    {
        linked.append(approval.device);
        m_records.setLinkedDevices(linked);
    }
    emit approved(m_names.value(approval.code));
}
```

`unlink` and `onUnlinked` are unchanged apart from the lost `m_queue`.

#### 1.7 Deletions

- `SealingQueue.h/.cpp`, their lines in `CMakeLists.txt`.
- `DocumentRecord::granted`: the field, `KeyGranted`, its `toJson`/`fromJson`/`operator==`
  lines. Records written by the current build carry the key; `fromJson` is strict, so
  make it ignore `granted` when present rather than fail - one line, with the comment
  "Written by builds before atomic grants; nothing reads it."
- `main.cpp`: the `SealingQueue` include and object, `queue.sealEverywhere()`, the
  queue arguments to `DocumentManager` and `DeviceLinks`; `DeviceLinks` takes
  `identity` and `std::bind(&DocumentLibrary::pathsFor, &library, _1)`.
- `DocumentRecords::documentIds()` comment: "so an approval can reseal every ward".

Commit as `feat: grant and approve carry the resealed version`. The build target
`WardPlanningLib` and the app must compile; the tests will not until Task 3.

### Task 2: STOP - user review of the core

Hand over the diff of Task 1. Do not run the test build, do not dispatch a reviewer, do
not start Task 3. The agent review follows the user's own pass.

---

## Phase 2 - Tests

### Task 3: Tests on the new contract

**Files:**
- Modify: `tests/services/SessionTestSupport.h/.cpp`
- Modify: `tests/services/ServerConformanceTest.h/.cpp`
- Modify: `tests/services/DocumentManagerTest.cpp`, `tests/services/DeviceLinksTest.cpp`,
  `tests/services/IncomingWardsTest.cpp`, `tests/services/DocumentSessionTest.cpp`,
  `tests/services/AppSettingsTest.cpp`
- Delete: `tests/services/SealingQueueTest.h/.cpp`, their lines in `tests/CMakeLists.txt`
  and `tests/TestMain.cpp`

**Test support.** `Device::queue()` and `queueTimers` go. `TestWard::seal` goes.
`TestWard::grant(admin, newcomer)` builds the resealing through
`ServerVersion::buildResealing` on `admin` (approved = `admin.records.linkedDevices()` plus
`newcomer.identity.keys()`), sends `grant` with it on the admin's session, then downloads
to `newcomer` as today. `TestWard::link(newDevice, existing)` does the same for every
ward `existing` holds and sends `approveLink` with them, then appends to
`existing.records.linkedDevices()`. Their doc comments say what the app does.

**Conformance** (runs against the fake always, the real server when
`WARD_PLANNING_SERVER_URL` is set):

- `aGrantStoresTheResealedVersionWithTheMembership`: after the grant, `head.version`
  is the previous plus one, `download` returns the sent header and blob, and the
  requester is a member. Pin the version number.
- `aGrantOnAStaleVersionChangesNothing`: a grant whose `baseVersion` is one behind gets
  `VersionConflict`; afterwards the request is still pending, the member list is
  unchanged, and `head.version` is unchanged.
- `aGrantSealingToAStrangerIsRefused`: a resealing whose header names a device neither
  on the allow-list nor in the request gets `NotOnAllowList`; nothing changes.
- `anApprovalStoresEveryResealingOrNone`: two wards; approve with both resealings lands
  both (versions pinned); approve with one stale and one current gets `VersionConflict`,
  neither version moved, and `link(code)` still shows the device unapproved.
- `anApprovalWithNoResealingsAddsTheDevice`: the empty list is accepted.

**`DocumentManagerTest`:**

- `aGrantSealsTheDeviceThatAskedAndADeclineSaysSo`: after `grant`, Bob's device opens the
  ward directly through `bob.open(ward.documentId)` with no sealing step in between, and
  `app.manager.session()->header()` does not yet carry Bob (the open session takes the
  new version in at its next head check). Assert `membersAt` lists Bob, and that
  `bob.records.record(...).baseVersion` is the version the grant stored. Remove the
  `granted.isEmpty()` assertion.
- `aGrantRetriesOnceOnAConflict`: `ward.server.forceConflictOnce(documentId)` (add the
  helper if the fake lacks it: sets `forceConflict`), then `grant`; `accessCallDone`
  fires once and Bob opens the ward.
- `aGrantOnAnUnreadableVersionIsBlocked`: corrupt the fake's stored blob for the ward
  (`ward.server.replaceBlob(documentId, "garbage")`, add if missing); `grant` emits
  `grantBlocked` with the reason text from `Reconcile` ("The document could not be
  decrypted.") and `accessCallDone` does not fire; the request stays pending.

**`DeviceLinksTest`:**

- `approvingALinkRemembersTheDeviceAndSealsItEverywhere`: two shared wards on the
  laptop (create a second with `ward.createSharedAnother(laptop)` or a second
  `TestWard`-style helper; add what is missing). After `approve`, both wards'
  server versions seal to the phone, `linkedDevices` holds the phone, and
  `phone.open(id)` succeeds for both with no further step.
- `aWardTheApproverCouldNotReadIsLeftOutAndArrivesWithItsNextWrite`: two wards; corrupt
  the fake's blob for the second; approve; the first is sealed to the phone, the second
  is not, the device is linked. Restore the blob; the laptop opens the second ward and
  edits it; after its publish the version seals to the phone.
- `anApprovalRetriesOnceOnAConflict`: `forceConflictOnce` on one ward; approve lands.

**`IncomingWardsTest`:** `aGrantedRequestArrivesOnceTheWardIsSealedToThisDevice` becomes
`aGrantedRequestArrivesAtOnce`: after `grantOnServer` (now with a resealing built on
Alice), one `checkNow` drops the request and fetches the ward; `waiting()` is never
non-empty. `aLinkedDevicesWardsArriveOnceItsLinkerSealsThem` becomes
`aLinkedDevicesWardsArriveAtOnce` for the ward the linker held, plus
`aWardTheLinkerLackedWaitsForItsNextWrite`: a third person Carol shares a ward and grants
Alice's laptop after the phone was linked; the phone lists it, `waiting()` names it; the
laptop opens and edits it; after the publish the phone's `checkNow` fetches it.

**`DocumentSessionTest`:** drop the `granted.isEmpty()` line in
`aSecondDeviceGrantedAccessOpensIt`. **`AppSettingsTest`:** drop the `granted` lines.

Every test uses the Task 1 code as it stands; where a test exposes a defect, fix the
defect in the same commit and say so in the report.

Commit as `test: atomic grant and approve`.

---

## Phase 3 - Incoming wards without downloads

### Task 4: `IncomingWards` polls only while waiting, checks by head, and learns losses from `me`

**Files:**
- Modify: `src/services/IncomingWards.h/.cpp`
- Modify: `src/services/DocumentLibrary.h/.cpp`
- Modify: `src/services/DocumentSession.h` (one enum member)
- Modify: `src/services/DocumentManager.h/.cpp`, `src/widgets/MainWindow.cpp`, `src/main.cpp`
- Modify: `tests/services/IncomingWardsTest.h/.cpp`

**The poll runs only while there is something to wait for.** `IncomingWards` starts its
timer when `pending()` or `waiting()` becomes non-empty and stops it when both are
empty. `checkNow()` still runs at launch from `main.cpp`. A device with nothing pending
makes no periodic calls at all. (Today every device calls `me` once a minute for its
whole life, which alone is 1,440 requests a day against a budget of a few hundred per
ward per peak day.)

**Waiting is detected by `head`, not by download.** `DocumentLibrary::restore` becomes:
`me`, then for each listed ward the library lacks, `head`; if the header seals to this
device's fingerprint, `fetchWard`; otherwise report `Download::NotYetSealed`, a new
member of `DocumentSession::Download` with the comment "listed for this person, but no
version is sealed to this device yet". `IncomingWards::onFetched` puts `NotYetSealed` in
`m_waiting`, removes the ward on `Done`/`NeedsNewerVersion`/`WriteFailed`/`AccessLost`,
and leaves it on `Unreachable`/`ServerFault`. `MainWindow::onWardRestored` lists the new
member with the others `IncomingWards` keeps to itself.

**A held ward missing from `me` has been lost.** In `DocumentLibrary::onListed`, for
each shared ward in the library that `me.documents` does not list, call `head`; its
`AccessLost` carries how and by whom; report through a new `restore` callback parameter
`WardLost(documentId, LostAccess)`. `IncomingWards` forwards it as a signal
`lostAccess(documentId, lost)`, which `main.cpp`/`DocumentManager` connects to the
existing `onQueueLost`, renamed `onWardLostElsewhere`. This replaces what the queue's
startup pass did, with no extra calls in the common case.

**Tests:** `aDeviceWithNothingToWaitForDoesNotPoll` (no timer active after a `checkNow`
with an empty library; `ward.server.requestCount()` or equivalent unchanged after
advancing the clock - add a counter to the fake if missing), `aWaitingWardIsCheckedByHeadNotDownloaded`
(assert the fake's download count stays 0 while waiting and becomes 1 when sealed),
`aWardRemovedWhileClosedIsForgottenAtTheNextCheck` (Alice removes Bob while Bob's ward is
closed; Bob's `checkNow` emits `lostAccess` with `How::Removed` and `byWhom` "Alice";
the library no longer contains it).

Commit as `feat: incoming wards poll only while waiting and check by head`.

---

## Phase 4 - Documentation

### Task 5: Design doc, wire table, and the superseded plan text

**Files:**
- Modify: `docs/plans/2026-08-30-encrypted-shared-document-design.md`
- Modify: `docs/plans/2026-10-11-server-held-access-plan.md`
- Modify: `src/services/HttpWire.h` (done in Task 1; verify)

Design doc:

- API table rows for `POST /links/{code}/approve` and `POST /documents/{id}/members`:
  describe the body and the all-or-nothing commit.
- *Identity*, linking step 4: the approving device sends every ward it holds, resealed
  to the new device, with the approval; the server adds the device and stores them
  together. A ward it did not hold reaches the new device when the approver next writes
  it; until then that ward shows *Waiting for access*.
- *Granting access*, "By request": the admin's app resends the ward's current version
  resealed to the device that asked, with the grant; the person can read it the moment
  they are a member.
- Component table: drop the `RosterChangeQueue` row; add to `ServerVersion`'s row (or add
  one): "building the resealed version a grant or an approval carries".
- The sync-state list and failure-path row for *Waiting for access*: now only a ward
  the approver did not hold at approval time.
- The invariants list bullet on links; the "device linked by a device that is later
  lost" bullet: such a device waits only in wards the lost device never wrote after
  linking it.
- Capacity: note that a device polls only while a request or a ward is waiting.

Server-held access plan: section 1.8 and the grant text in Task 8 describe the queue.
Replace each with one line pointing at this plan, so the document stays an accurate
record of what was built and what superseded it. Do not rewrite the rest.

Commit as `docs: atomic grant and approve`.

---

## Verification before calling it done

- Full test suite green (`build.bat debug`, then `WardPlanningTests.exe` as CLAUDE.md
  describes). Pin the totals.
- `grep -rn "SealingQueue\|sealEverywhere\|settleGranted\|\.granted\b" src tests docs`
  returns only the superseded-plan pointers.
- Run the conformance suite against the companion server once it implements 1.1; until
  then, say plainly that it has not run against the real server.
