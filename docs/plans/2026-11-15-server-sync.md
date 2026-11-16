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

Done. `tests/server/FakeServer.h` is the server in process, keeping the rules of
`server/src/ward.ts`; every reply and channel event waits in one queue until a test hands it over
with `deliverNext` or `settle`. `TestClient` takes a `FakeServer` and has `sync()` and
`startSync()`. `tests/server/ServerSyncTest.cpp` holds the sync tests, and
`tests/services/SignedLineTest.cpp` the signed lines' known answers, which
`server/test/wire.test.ts` pins for the same inputs.

---

## Task 3: `NetworkTransport`: HTTP and the WebSocket

Done. `src/services/NetworkTransport.h` carries the calls over HTTPS and the channel over a
WebSocket it pings every 30 seconds. The conformance suite runs it against the live server.

---

## Task 4: The window: the switch, the notices, who is here

Done. `DocumentManager` owns the `Server` and the `WardSync` and starts sharing when a ward is
open and the switch is on; `MainWindow` shows the notices and who else is here; `WardSharing`
saves an invitation from the server's version while the ward is syncing. `docs/ARCHITECTURE.md`
has the section *Sharing through the server*, and `docs/USER_GUIDE.md` one of the same name.

---

## Task 5: The conformance suite, against the fake server and the real one

Done, ahead of Task 4, so the window is built on a transport and a fake that the live server has
checked. `tests/server/ConformanceTest.cpp` runs on the fake server in every test run and on the
real one when `WARD_PLANNING_SERVER_URL` is set; `server/README.md` says how.

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
