# Server Sync Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** With *Use the Ward Planning server* turned on, the open ward shares itself through the
server: every change is pushed as it is made, every other member's version is taken as it lands,
the window says who else is here, and it says what stops sharing when something does.

**Architecture:** `WardSession` stays the one place that changes a ward; it gains the two calls
every way of sharing uses, `takeVersion` and `holdPushed`, which update files already go through
under other names. `WardSync` sits beside the session and owns the one orchestration, the *look*:
read the server's head, then take it, push over it, create the ward, or end this device's share,
and settle every unanswered write the same way. It holds the ward's live channel, which says who
is here and when to look. `Server` is the wire: it turns each call into a request and reads the
reply, over a `Transport` that is HTTP in the app (`NetworkTransport`) and an in-process
`FakeServer` in the tests.

**Tech Stack:** Qt 6.12 (Core, Gui, Widgets, Network, WebSockets, Test), C++17, MSVC 2022, CMake +
Ninja via `build.bat`, libsodium from vcpkg, Qt Test. The server is `server/` in this repository,
deployed at `https://ward-planning-dev.ward-planning-server.workers.dev`.

**Design authority:** `docs/SHARED_WARD_DESIGN.md`: *What the server is*, *The live channel*, *Who
is here*, *Every edit is a version*, *Which version wins*, *Saving*, *If the server goes away*,
*Failure paths*, *Testing*. Section names in *italics* are its sections. The server as built is
`server/src/ward.ts` and `server/README.md`.

**What a second plan covers, and this one leaves as it is:** joining online with its lost-answer
cases and *The server has no such ward*; Leave and *Remove this device*; the update check from
`/releases/latest`; the server address override; and the switch being on by default. After this
plan, a join works as it does with the server off, and its version travels through the server by
the ordinary path.

---

## Conventions for every task

- **The executor does not build.** The project's `CLAUDE.md` forbids running cmake or the app from
  this shell. The user builds with `build.bat` and runs `build\bin\WardPlanningTests.exe`. Each
  task's tests are written in that task and run when the user builds. The server's own checks
  (`npm test`, `npm run live` in `server/`) the executor runs itself.
- **Core first.** Task 1 is the whole design-bearing core, then a STOP for the user's own review.
  Every later task is a consequence of it. The tree and the app may be red between tasks.
- **Delete with the last caller**, in the same task.
- **Docs change in the task whose code they describe** (`docs/FORMAT.md`,
  `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`, `docs/SHARED_WARD_DESIGN.md`).
- **Style:** `docs/CODING_STYLE.md` and the project `CLAUDE.md`: Allman braces, braces on every
  control statement, only one-liners in headers, `std::optional` for nullable fields, leading
  operators on continuation lines, `tr()` for every user-visible string. Comments say what is true
  now. ASCII only.
- **Commits:** one per task, with the message the task gives, plain, no attribution lines. Before
  each commit, `git branch --show-current` prints `feature/server-sync`. Stage the files the task
  changed by name.
- **Tests** pin observed values, use inputs that give non-trivial results, and anchor every
  absence assertion to a positive signal.

---

## Task 1: The core: the wire, the look, and the two calls every way of sharing uses

Done, in commits `d1afcfa` and the one after it that addresses its review. The code is the
record: `src/services/WardSync.h` states the look in its class comment, `Server.h` the calls,
`Transport.h` what carries them, `SignedLine.h` the signed line, and `WardSession.h` the calls
the sync makes on the session (`nextVersion`, `writer`, `holdPushed`, `takeVersion`,
`sawOnServer`, `becomeNoLongerShared`, `invitationCarrying`, and the `localChange` signal).

What later tasks rely on:

- Every call on a `Transport` or a `Server` returns an object that emits its answer once, after
  the call has returned, and then deletes itself. `TransportChannel::close` may be called on a
  channel that has already closed.
- `WardSync::fetchInvitation` answers through `invitationFetched` with an invitation carrying
  the server's version, once the look under way has ended, or with no bytes when the server did
  not answer, sharing stopped, or its version does not name the invite.
- This device appears in `WardSync::present()` under `WardSession::writer()`'s fingerprint.

---

## Task 2: The fake server, the test client, and the sync tests

*Testing* (the server interface, a test client, *Against the fake server*, *The server switch*).

**Files:**
- Create: `tests/server/FakeServer.h`, `tests/server/FakeServer.cpp`
- Create: `tests/server/ServerSyncTest.h`, `tests/server/ServerSyncTest.cpp`
- Create: `tests/services/SignedLineTest.h`, `tests/services/SignedLineTest.cpp`
- Modify: `tests/services/TestClient.h`, `tests/services/TestClient.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `server/test/wire.test.ts` (the block's title)

**Step 1: `FakeServer`**

A `Transport` that is the server, in memory, keeping the rules `server/src/ward.ts` keeps: it
reads each request as the Worker and the `Ward` object do, with `WardHeader::parse`,
`Identity::fingerprintOf`, `SignedLine` and `Crypto::verify`, and answers the statuses
`server/README.md` lists. Per ward it holds the current version's bytes and the open channels,
each under its signer's fingerprint.

```cpp
/// The Ward Planning server, in process: a Transport that keeps each ward's current version by
/// the server's own rules and holds its channels, and that a test can make misbehave.
class FakeServer : public Transport
{
public:
    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;

    /// While unreachable, every request gets no answer and stores nothing, and every channel
    /// closes and none opens.
    void setReachable(bool reachable);

    /// The next write is stored, and told to the channels, and gets no answer.
    void dropNextWriteReply();

    /// Hands over the oldest reply that is waiting; false when none is. Every reply waits
    /// until a test hands it over, here or through settle.
    bool deliverNext();

    /// Hands over every waiting reply and channel event, and those they lead to, until none
    /// is left.
    void settle();

    /// The ward's current version becomes bytes, whatever the rules say, as a restored or
    /// hostile server would serve it. Tells no channel.
    void serve(const DocumentId& document, const QByteArray& bytes);

    /// Forgets the ward and closes its channels.
    void forget(const DocumentId& document);

    /// The bytes of the ward's current version; empty when it holds none.
    QByteArray version(const DocumentId& document) const;

    /// How many requests and channel opens it has received.
    int calls() const;

    /// The fingerprint of every device with a channel open to the ward.
    QSet<QByteArray> present(const DocumentId& document) const;
};
```

Nothing is answered inside `send` or `open`: each reply and each channel event (a message, a
close) joins one queue in the order the server produced it, and `deliverNext` or `settle` hands
them over. So a test decides what has arrived when, with no timer and no event loop, and a test
that does not care calls `settle` after each step. An `open` by a signer the current header does
not name, or while unreachable, queues a close. A push that drops a device closes its channels,
and a second open by one device closes its first.

**Step 2: `TestClient`**

The client gains a server and a sync, so a multi-user test is two or three clients on one
`FakeServer`:

```cpp
    /// A client of server, which it shares its wards through once startSync is called.
    TestClient(const QString& deviceName, FakeServer* server);

    WardSync& sync() { return *m_sync; }

    /// Starts sharing the open ward, and settles the server.
    void startSync();
```

`reopen()` rebuilds the sync with the session. The existing constructor stays for the tests that
use no server.

**Step 3: `SignedLineTest`**

The known answers `server/test/wire.test.ts` pins, for the same inputs: the document id is the 16
bytes `0x00` to `0x0f`, and the version is the header line `{"format":1}`, a newline, then the
payload.

- `createLine`: payload `abc` gives
  `wardplanning-v1 create AAECAwQFBgcICQoLDA0ODw IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E vd2BPGNCOXIxce8_7phXm5SWTjuxyz5CcmLIwGjVIxk`
- `pushLine`: an empty payload gives
  `wardplanning-v1 push AAECAwQFBgcICQoLDA0ODw IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g`
- `updateLine`: payload `abc` gives the create line with `update` in place of `create`
- `channelLine`: `wardplanning-v1 channel AAECAwQFBgcICQoLDA0ODw - -`

In `server/test/wire.test.ts` the block's title becomes `known answers shared with the app`, and
the fingerprint test's comment names `tests/services/SignedLineTest.cpp` beside
`IdentityTest.cpp`. Run `npm test` in `server/`: 40 pass.

**Step 4: `ServerSyncTest`**

Each test builds its clients on one `FakeServer` and drives them step by step. `alice` creates a
ward of `TestWard`'s content; where a test needs two members, `TestSharing::share` gives both the
ward and both start. Names and what each pins:

*The first look*
- `aNewWardIsCreatedOnTheServer`: after `startSync`, the server's version equals the bytes
  `alice`'s `nextVersion` gave, numbered 2; `isOnServer()` is true; her journal is empty; one
  `POST` and no `PUT` were sent.
- `aCreateWhoseAnswerIsLostIsSettledByTheNextLook`: with `dropNextWriteReply`, the look ends in
  no problem with the ward on the server, the journal emptied by the mark, and exactly one `POST`
  in `calls`.
- `aWardTheServerNoLongerHasIsNoLongerShared`: after `forget`, `retry` makes the ward no longer
  this device's, read-only, its journal dropped, and no `POST` follows.

*Pushing*
- `aCommandIsPushedAsTheNextVersion`: `commitBobsPhone` on `alice` leaves the server one number
  higher, holding that phone when `bob` takes it, and her journal empty.
- `aCommandMadeWhileAPushIsOnItsWayGoesOutWithTheNext`: two commits with nothing delivered between them; after
  `deliverNext` for each reply in turn, the server holds both changes in two versions and the
  journal is empty; the second entry was still in the journal after the first reply.
- `aPushWhoseAnswerIsLostIsNotAppliedTwice`: an attempt command pushed with its reply dropped;
  after the next look the journal is empty by the mark, and the document holds exactly one such
  attempt; repeated with `reopen()` before that look.
- `twoPushesOfOneNumberTheSecondReplaysAndPushesTheNext`: both offline (`setReachable(false)`),
  each edits a different person's phone; back online, `alice` retries then `bob` retries: the
  server's number is two above the start, both phones are in both documents after `alice`
  retries again, and neither replay was abandoned.
- `aConflictAbandonsTheJournalAndTheNextCommandPushes`: both offline edit Bob's phone; `alice`
  lands first; `bob`'s `versionFetched` carries `Abandoned` with the to-redo text naming his
  value; the server keeps `alice`'s; `bob`'s next command pushes.
- `anAutomaticUpdateIsPushedOnce`: a geocode batch for two families pushes one version, and a
  second client that takes it starts no batch and pushes nothing (`calls` unchanged after its
  look but for the head and the fetch).
- `aSaveThatFailsPushesNothing`: with the store failing writes, a commit leaves the server where
  it was and `isSyncing()` false; once writes work, `retry` saves and pushes it.

*Taking*
- `aNoticeMakesTheOtherClientFetch`: `alice` commits; `bob`, connected, holds her change with no
  call from the test but the server's `settle`.
- `aServerAheadWithAnEmptyJournalFastForwards`: `bob` offline while `alice` pushes twice; his
  `retry` takes the head as `FastForwarded`.
- `aHeadThatRanksBelowIsPushedOver`: `serve` an older version of the ward; `alice`'s `retry`
  pushes her next version though her journal is empty, and the server keeps it.
- `aHeadFromBeforeThisDeviceJoinedIsRetriedUntilAMemberPushes`: `serve` a version that lacks
  `bob`; `bob`'s look ends in `PushFault`, the ward still his; after `alice`'s look pushes over
  it, `bob`'s retry ends in no problem.
- `aVersionThatFailsACheckReplacesNothing`: `serve` another ward's version under this id, and
  the same with its header's id rewritten: each look ends in `Unreadable`, `alice`'s document and
  version unchanged, and her next command is not pushed while it lasts; once `serve` puts back a
  version she can read, `retry` clears it and pushes.
- `aNewerFormatOnTheServerStopsPushesAndKeepsEdits`: `serve` a header line with `format` 2: the
  problem is `NewerFormat`, a commit journals and saves and sends no `PUT`; serving a readable
  version again clears it and the journal replays and pushes.
- `aRemovedDeviceIsNoLongerShared`: `alice` removes `bob`'s user and pushes; `bob`'s channel
  closes, his look makes the ward no longer his, his sync stops, and a later `retry` sends
  nothing.

*Unreachable*
- `editsMadeWhileUnreachableGoUpOnReconnect`: unreachable, three commits: the problem is
  `Unreachable`, `isConnected()` false, the journal holds three; reachable and `retry`: one
  version carries all three and the channel is open again.

*The channel*
- `theChannelSaysWhoIsHere`: `alice` alone sees herself; when `bob` starts, both see both; when
  `bob` stops, `alice` sees herself again.
- `aSecondChannelFromOneDeviceKeepsWhoIsHere`: `alice`'s `retry` after her channel is closed
  under her leaves `bob`'s `present()` holding both, with `presenceChanged` not having dropped
  her in between once settled.

*Starting, stopping and the channel's signer*
- `aNewWardIsConnectedOnceItIsCreated`: after `startSync` and `settle` on a ward the server did
  not have, `isConnected()` and `isSyncing()` are true and `present()` holds the device, with no
  `retry`.
- `aJoinersChannelIsReopenedAsItsDevice`: `bob` joins through an invitation and starts; after
  his join is stored, `present()` on `alice` holds his device's fingerprint and not the
  invite's, and `bob` is connected, with no `retry`.
- `aRemovalFoundByALookStopsSharingAtOnce`: after `bob` is removed, his next look leaves
  `isRunning()` false, and `calls()` does not grow when his retry timer's slot is invoked.
- `aSaveThatFailsWhileALookIsOnItsWayIsNotPushed`: with the head's reply held, the store set
  to fail writes, and a commit made; delivering the reply sends no `PUT`; once writes work,
  `retry` pushes it.
- `afterTheServerComesBackTheWriteIsSentAgain`: a push and the look after it both get no
  answer; reachable again, `retry` ends in no problem with the change on the server, and the
  problem was never `PushFault`.

*Invitations*
- `anInvitationCarriesTheServersVersion`: `alice` invites and asks for the invitation while her
  push is on its way; `invitationFetched` arrives only after that push is stored, and the
  version in the invitation is byte for byte the server's.
- `anEditAfterTheInvitationSurvivesTheJoin`: `alice` invites, fetches the invitation, then
  commits Bob's phone; `bob` joins through the invitation and syncs; `alice` holds Bob as a
  member and her phone edit, and so does `bob`.
- `anInvitationIsNotGivenWhileNothingCanBeShared`: unreachable, `fetchInvitation` answers with
  no bytes; and for an invite withdrawn before the fetch, with no bytes.

*The switch*
- `withTheServerOffNothingIsSent`: a client whose sync is never started creates, edits, writes
  an update and joins through an invitation, and `calls()` stays 0.
- `anUpdateFileAboveTheHeadIsPushedOver`: `bob`, syncing, takes `alice`'s update file written
  with her sync stopped: his next look pushes over the head, and the server then holds her
  change.
- `anUpdateFileBelowTheHeadIsReplayedOntoTheHead`: the file is taken, and the next look takes the
  head and replays onto it.
- `membersWhoTradedFilesTurnTheServerOn`: `alice` and `bob` share by files only, the ward never
  on the server; `bob` starts first and creates it there; `alice` starts and takes it or pushes
  over it; both end holding equal documents.

Register `SignedLineTest` and `ServerSyncTest` in `tests/main.cpp` and add the sources to
`tests/CMakeLists.txt`.

**Step 5: Commit**

```bash
git add tests/server tests/services/SignedLineTest.h tests/services/SignedLineTest.cpp tests/services/TestClient.h tests/services/TestClient.cpp tests/CMakeLists.txt tests/main.cpp server/test/wire.test.ts
git commit -m "A fake server keeps the real one's rules in process, and the sync tests run clients against it"
```

---

## Task 3: `NetworkTransport`: HTTP and the WebSocket

*The live channel* (the ping, a closed channel), *Hosting notes*.

**Files:**
- Create: `src/services/NetworkTransport.h`, `src/services/NetworkTransport.cpp`
- Modify: `CMakeLists.txt`

`NetworkTransport` is a `QObject` and a `Transport` over one `QNetworkAccessManager`:

```cpp
/// The app's transport: HTTPS and WebSocket to the Ward Planning server.
class NetworkTransport : public QObject, public Transport
{
    Q_OBJECT

public:
    /// The server the app is built to use.
    static constexpr char BuiltInAddress[] = "https://ward-planning-dev.ward-planning-server.workers.dev";

    /// Sends to the server at address, such as BuiltInAddress.
    NetworkTransport(const QString& address, QObject* parent);

    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;
};
```

- **`send`** returns a `NetworkCall`, a `TransportCall` that owns its `QNetworkReply`. It builds
  a `QNetworkRequest` for `address + path` with the request's headers and a transfer timeout of
  30 seconds (`setTransferTimeout`), sends it with `sendCustomRequest`, and on `finished` emits
  `answered` with the HTTP status and the body, or `NoAnswer` when the reply has no status, then
  deletes itself. Destroying the call aborts the request.
- **`open`** returns a `NetworkChannel`, a `TransportChannel` over a `QWebSocket` it owns. It
  opens `wss://` (or `ws://` for an `http://` address) with a `QNetworkRequest` carrying the
  request's headers. `textMessageReceived` becomes `messageReceived`; `disconnected` and
  `errorOccurred` become one `closed`, emitted once. Every 30 seconds it sends a ping, and a pong
  that has not arrived when the next ping is due closes the socket, as *The live channel* asks.

Qt 6.12's `QWebSocket` is newer than the executor's training data: read its documentation for
`open(const QNetworkRequest&)`, `ping`, `pong` and `errorOccurred` before writing against them.

No unit test: Task 5's conformance suite runs it against the live server.

**Commit:**

```bash
git add src/services/NetworkTransport.h src/services/NetworkTransport.cpp CMakeLists.txt
git commit -m "NetworkTransport carries the server's calls over HTTPS and its live channel over a WebSocket"
```

---

## Task 4: The window: the switch, the notices, who is here

*If the server goes away* (the switch), *Who is here*, *Failure paths*, *Saving* (what is pending
is retried).

**Files:**
- Modify: `src/main.cpp`, `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Create: `src/services/Presence.h`, `src/services/Presence.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/widgets/ShareNotice.h`, `src/widgets/ShareNotice.cpp`
- Modify: `src/widgets/WardSharing.h`, `src/widgets/WardSharing.cpp`,
  `src/widgets/WhoHasAccessDialog.cpp`
- Create: `tests/services/PresenceTest.h`, `tests/services/PresenceTest.cpp`
- Modify: `tests/services/WhoHasAccessDialogTest.cpp`, `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`, `docs/SHARED_WARD_DESIGN.md`

**`DocumentManager`** takes a `Transport*` after the store, and owns a `Server` on it and a
`WardSync` on its session and that server.

- `WardSync& sync()` for the window.
- `void setUseServer(bool use)`: starts the sync when a ward is open and `use` is set, stops it
  otherwise. `createWard`, `openWard` and `join` stop the sync before they change the session and
  start it after, when the server is in use and the ward opened.
- `bool retryPending()` replaces `retryPendingSave` for the window's callers: it retries the save
  and, when nothing is left unsaved, calls `sync().retry()`; it answers what `retryPendingSave`
  answered.
- It forwards `WardSync::versionFetched` as its own signal of the same name.

`main` builds a `NetworkTransport` on `NetworkTransport::BuiltInAddress` and hands it to the
manager, then calls `setUseServer(settings.useServer())` once the window has its settings.

**`Presence`** turns the channel's fingerprints into the line above the ward:

```cpp
/// Who else has the ward open, as the window says it.
class Presence
{
    Q_DECLARE_TR_FUNCTIONS(Presence)

public:
    /// One name for each other device in present, as access names it: its user's name; for an
    /// open invite, "someone with Bob's invitation" by its label, or "a device invitation";
    /// and for a fingerprint access does not know, "a device that just joined". A user with
    /// two devices here is named once. own is the fingerprint this device is here under, the
    /// session's writer(), which is left out.
    static QStringList others(const AccessList& access, const QSet<QByteArray>& present, const QByteArray& own);

    /// "Alice Smith is also here", "Alice Smith and Bob Jones are also here", or with three
    /// or more, "Alice Smith, Bob Jones and Carol Lee are also here"; empty for no one.
    static QString line(const QStringList& others);
};
```

`PresenceTest`: `othersLeavesOutThisDeviceAndNamesAUserOnce` (own device, a user with two
devices here, a user invite labelled Bob, a device invite, an unknown fingerprint: exactly four
names, in sorted order), and `lineJoinsNames` (the three wordings and the empty one).

**`ShareNotice`** gains `void showMessage(const QString& message)`, a notice with no button, and
`MainWindow` a presence label above the map, hidden while it is empty.

**`MainWindow`**
- `updateSharing` also runs on `WardSync::stateChanged`, and with the server on shows the first
  that applies, after *no longer shared*:

  | State | Notice |
  |---|---|
  | `SyncProblem::NewerFormat` | *This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work.* |
  | `SyncProblem::Unreadable` | *The ward on the server could not be read. Your copy is safe and you can keep working; sharing resumes when the server has a readable copy.* |
  | `SyncProblem::PushFault` | *Your changes could not be shared right now. They are saved here.* |
  | not connected, or `SyncProblem::Unreachable` | *Working offline. Your changes are saved here and will go up when the connection returns.* |
  | otherwise | no notice |

- `WardSync::presenceChanged` sets the presence label from `Presence`.
- `WardSync::versionFetched` runs what `openUpdateFile` runs after a take, without its message
  boxes for a version that was taken: `followUserDetails()` when `Taken`, and
  `showChangesToRedo` when the replay was `Abandoned`. The two share one private function,
  `afterTake(const TakeResult&)`.
- `onSettings` calls `DocumentManager::setUseServer` with the new value.
- `onApplicationStateChanged` and `unsavedChangesMayBeLost` call `retryPending()`.
- `executeCommand` needs no change: the session's `localChange` asks for the look.

**`WardSharing`** and **`WhoHasAccessDialog`**: with the server on, *Add a User*, *Add a
Device* and *Save invitation* are offered while the ward is syncing; otherwise they show the
notice of what stops it, from one function that `updateSharing`'s table and these share,
`WardSharing::whatStopsSharing(const WardSync&)`, which answers *Connect to the internet to
invite someone.* only when the cause is no connection. With the server on, the invitation they
save comes from `WardSync::fetchInvitation`, so it carries the version the server holds: the
action makes the invite, asks for the invitation, and shows a small *Creating the invitation...*
dialog until `invitationFetched` answers, then the save dialog as today. An answer with no
bytes shows *Your invite could not be created, because the ward changed first. Try again.* for
an invite the access list no longer holds, and otherwise the notice of what stops sharing. With
the server off they write the invitation at once, as today. Leave stays as it is. Both take the
`WardSync` beside the settings; `WhoHasAccessDialogTest` builds one on a `FakeServer`.

**Docs:** `docs/ARCHITECTURE.md` gains `Transport`, `Server`, `WardSync`, `NetworkTransport`,
`Presence` and the manager's part in them. `docs/USER_GUIDE.md` gains what the switch does when
on, the four notices and the presence line. In `docs/SHARED_WARD_DESIGN.md`, *Testing*'s
server-interface bullet says what was built (`Server` is one class over a `Transport`; the
app's is `NetworkTransport`, the tests' `FakeServer`), and its test-client bullet says sync
lives in `WardSync` beside `WardSession`.

**Commit:**

```bash
git commit -m "With the server on, the open ward syncs: the window says who else is here and what stops sharing"
```

(after staging, by name, each file this task changed)

---

## Task 5: The conformance suite, against the fake server and the real one

*Testing* (the fake and the real server are checked against each other).

**Files:**
- Create: `tests/server/ConformanceTest.h`, `tests/server/ConformanceTest.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `server/README.md`, `docs/BUILDING.md`

One suite written against `Server`, run twice by `tests/main.cpp`: on a `FakeServer`, always,
and on a `NetworkTransport` at `WARD_PLANNING_SERVER_URL` when that is set. Each case waits for its
answers through one helper, which settles the fake server and, for the real one, runs the
event loop until the answer arrives or ten seconds pass. The versions are real ones, written by `WardFile::write`
for identities the test makes.

The suite makes three wards in all, so a run stays inside the server's limit of 10 creates a
minute from one address:

- `createAndRead`: a create is stored; a second create of the id is not; `head` is the header
  line and `fetch` the exact bytes; a `head` and a `fetch` of an unknown id are `Missing`.
- `whoMayWrite`: a push by a device the header does not name is not stored; one signed by a
  named device that is not its `writer` is not stored; one whose header names another ward is
  not stored; after each, `fetch` gives the bytes from before; the named writer's push is stored.
- `onlyAHigherNumberIsKept`: a push of the current number by another named device is not stored
  and the first stays; a lower number is not stored; a number two above is stored.
- `removalAndDeletion`: a push that drops a device is stored, and that device's next push is
  not; a final version with no recipients is stored, after which a push and a create of the id
  are not.
- `theChannel`: an open by a named device is told who is here and the number; a second device's
  open is told to both; a stored push is told to both with its number; a push that drops a
  device closes its channel; an open by a device the header does not name closes without a
  message.

Run it against the live server:

```
set WARD_PLANNING_SERVER_URL=https://ward-planning-dev.ward-planning-server.workers.dev
build\bin\WardPlanningTests.exe
```

A disagreement is fixed in whichever of `FakeServer` and `server/` is wrong.

`server/README.md` and `docs/BUILDING.md` each gain the two lines above, and what the suite
leaves on the server: three wards, one of them deleted.

**Commit:**

```bash
git add tests/server/ConformanceTest.h tests/server/ConformanceTest.cpp tests/CMakeLists.txt tests/main.cpp server/README.md docs/BUILDING.md
git commit -m "One conformance suite runs against the fake server and, when its address is set, the real one"
```

---

## Open questions for the user

Each states the choice this plan makes.

1. **A stored push empties the journal of what it carried.** With one editor as the usual case,
   the journal then stays small. An invitation made with the server on carries the server's own
   version, so no copy of the ward exists with a pushed version's number and other content.
2. **A look reads the head before every push**, so each edit is two small requests. It keeps one
   path for a first push, a retry and a lost answer.
3. **A fingerprint on the channel that the access list does not know** shows as *a device that
   just joined*, the design's "a device that joined since this copy was last current".
4. **The channel is signed as the invite** while the version this device holds does not name the
   device, as its pushes are.
5. **The conformance suite's wards stay on the development server**: three per run, since the
   server has no call that removes a ward.
