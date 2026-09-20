# Plan Review — Encrypted Shared Document

**Date:** 2026-09-13
**Reviewed:** [2026-08-30-encrypted-shared-document-plan.md](2026-08-30-encrypted-shared-document-plan.md)
**Reviewers:** two fresh read-only agents, each given only the plan, design, source, and
project conventions. Round 2 ran after the core-first restructure, with this document
moved out of the repository so the reviewer could not see Round 1.

## Round 2 tracker — current plan numbering

Status: `open`, `discussing`, `fix`, `fixed`, `skip`. **R1** column links to the Round 1
item it repeats or extends; Round 1 task numbers refer to the plan before restructure.

| # | Item | R1 | Status | Decision |
|---|---|---|---|---|
| R2-1 | One Ed25519 key signs challenges, headers, and proposals with no context separation — server can forge a removal | new | fixed | **Tagged signatures**: `sign`/`verify` take a `SignatureTag` and prepend a short ASCII tag + one 0x00 byte — `auth`, `request`, `header`, `removal`; tag never stored; no untagged path. Plus **header chain keeps only membership-changing headers** (plus current), no age cap — sound because `verifyLink` is unchanged across a header with identical recipients. Edited design (primitives, chain, server storage/endpoint, authentication) + plan Tasks 1.2, 1.5, 1.8, 16. |
| R2-2 | `validateDownload`/`verifyChain` anchored on the local header, not the base header — every conflict after local saves reads as ServerFault | new | fixed | **Anchor on the base copy's header** (`std::optional`). No usable base: a decryptable newer version is still accepted (unseal, hash, decrypt, signature, generation), chain skipped; removal is never concluded → no deletion. Test: local save counter ahead of other writer still Accepts. Edited design (download validation, removal) + plan Tasks 1.5, 1.9, 17. |
| R2-3 | `leave` writes a header whose writer is absent from the new roster — nobody can open it | new | fixed | **A remaining member completes a leave.** Leave posts a signed leave request (proposal with proposer == target) and deletes the leaver's local copy; the next remaining member's app runs `completeLeave` automatically — rotates key, reseals, publishes with the request as approval, updates allow-list. Invariant: **a version's writer is always among its recipients.** Chain/server rule: a dropped recipient is covered by a second member's approval or their own leave request. Last member can't leave; "Remove from this computer" instead. Edited design (decisions, chain rule, server rules, revoking, tests) + plan Tasks 1.5, 1.7, 1.8, 2, 33. |
| R2-4 | Tasks 3–4 name `WardPlanningLib`, which exists only after Task 6 | new | fixed | **Rename moved to Phase 0 as Task 0** (user decision), ahead of the core; later tasks renumbered (old 7–37 → 6–36). Task 3 no longer renames the test target; Task 35's version text now relies on Task 0's 1.1.0. **Task numbers in rows above this one refer to the numbering before this change.** |
| R2-5 | One person can approve their own removal proposal with a second self-granted machine key | new | fixed | **Roster lists people, each with linked devices** (user: "add users, not machines"). Person links/unlinks own devices; chain rules 1-8 (writer member, device under one person, device changes by own person, person removal by third person or own leave request, rotation on any drop). Approvals count people. **Option 1** for invented persons: every grant announced; person granted <7 days can't approve (server clock + app; not chain-verifiable). **S1** one-time code = lookup + secret with keyed BLAKE2b tag (also fixes in-person invite / I12). **S2** accepted: stolen device can unlink owner's devices; recovery via two others. **S3** first screen asks "already use Ward Planning on another device?"; grant dialog flags matching display name. Edited design (decisions, threat model, header, chain, roster, identity, server, granting, revoking, notice, failures, components, tests, limits, recovery) + plan (terminology, 1.1, 1.4-1.9, 7, 9, 12, 24, 27, 28, 30-33, docs, verification, limits). |
| R2-6 | Unknown-kind entries preserved through rotation | I7 | fixed *(superseded by I13: unknown kinds are now refused)* | Preserve unknown-kind entries when the key is unchanged; **drop them on every rotation**; ignore them in `verifyLink`'s membership comparison. New kinds are added beside a recognised entry, so newer clients keep access and re-add. Edited design (file format, tests) + plan Tasks 1.4, 1.7. |
| R2-7 | Allow-list removal after rotation carries an approval for the previous generation | new | fixed | **The server's allow-list is the current accepted header's recipients**; no separate update. `PUT /members`, `AllowListEntry`/`allowList`, "revocation incomplete" state, fetch window after removal, and allow-list-vs-roster mismatch all removed. Server's one extra check: approval from a person first seen <7 days ago refused. Clients report header-vs-roster disagreement (`unattributableEntries`, `unsealedDevices`). Edited design (decisions, server, sharing, granting, revoking, failures, tests, continuity) + plan (terminology, 1.7, 1.8, 26, 27, 29, 31, 32, 33). |
| R2-8 | Merge routing not in the core API; optional response and unlisted fields; open-time "both moved" skips merge | I4, I5 | fixed | Core routing is `mergeDocuments` (from R2-11). `merge` takes optional responses: started one side kept; started both → one response, theirs' name/start; ended one side while other recorded → kept; name/start/end single-value; taskCategories as a set. Open-time "both moved" → Task 24 route. **I4:** `AppSettings` stores published save counter per doc at fetch/publish; unpublished = header counter above it; survives restart. Edited design (local record, on open, merge table) + plan Tasks 1.10, 7, 17. |
| R2-9 | Automatic geocode/lookup writes on reconnect defeat response-only routing | new | fixed | **Automatic updates follow the lease**: `mayWriteDirectory()` (unshared, or lease held) gates starting geocoding and ward/stake lookup; late results discarded; lease acquisition starts a batch for missing coordinates. Responder devices carry only response changes. Edited design (local-first sync) + plan Task 25. |
| R2-10 | QSettings uses the registry on Windows; tests not isolated; Task 7 migration finds nothing | I10 | fixed | `AppSettings` = **JSON file at explicit path** `<AppDataLocation>/settings.json` (user chose JSON over INI after discussion), loaded once, rewritten via `QSaveFile`; `setFilePath` set once at startup and to a fresh `QTemporaryDir` file by the test runner. **No migration** from earlier versions (user decision); old cache-folder settings and `MainWindow` helpers (`:607-668`) deleted; Task 6 warning reworded. *Note:* INI was first applied without approval and then reverted. **Single running instance** (user decision): `QLockFile` in AppDataLocation at the top of `main()`; a second launch forwards its arguments over `QLocalSocket` to the running window and exits; stale locks taken over. Added to plan Task 7 and design decisions. Edited plan Tasks 3, 6, 7. |
| R2-11 | The lease makes a connected emergency single-writer | new | fixed | **Emergency response records need no lease**, online or offline; lease covers directory/ministering/ward details/roster. Head checked every 30 s during an active emergency. Routing generalised so it works online: `ResponseMerge::mergeDocuments(base, mine, theirs)` — silent when at most one side changed the directory (take that side's directory, merge response), `nullopt` → conflict dialog only when both changed it. Edited design (decisions, local-first sync, emergency section, failures, tests) + plan Tasks 1.10, 2, 19, 25. Partly addresses R2-8 (core routing function). |
| R2-12 | Read-only only at `writeToDisk` leaves phantom edits | I6 | fixed | `executeCommand`/`undo`/`redo` check `isDirectoryReadOnly()` **before mutating** and refuse with a message; `writeToDisk` backstop; response edits never refused. Read-only session fast-forwards on head checks. **Replacing the in-memory document clears the undo stack.** Edited design (local-first sync) + plan Task 25. |
| R2-13 | Task 26 roster merge contradicts Task 24 adoption | I8 | fixed | Deleted Task 26's roster-merge step and `AccessControl::mergeRosters` + its test; `Reconcile::adopt` is the only roster reconciliation. Design's "Grants merge" folded into the adoption step. Edited design (refused publish, On conflict) + plan Tasks 1.7, 26. |
| R2-14 | `adopt` undefined with no usable base; carried grants need allow-list update | new | fixed | No usable base → **take their roster exactly**; `Adoption::notCarriedOver` lists people/devices in mine but not the result, shown as a notice ("Grant again if needed"). Allow-list half moot after R2-7. Edited design (refused-publish route) + plan Tasks 1.9, 24. |
| R2-15 | "Overwrite theirs" cannot pass the server's link rule | new | fixed | Conflict dialog appears only when both sides changed the directory, after adoption and response merge. Options: **Save mine as a copy** (default, local only), **Keep theirs**, **Keep mine** (my directory + merged response + adopted roster/key, published on top of theirs). Edited design "On conflict" + plan Task 26. |
| D3 | *(raised during R2-15)* App-written copies survive revocation | new | fixed | Copies carry **`originDocumentId`** (AD-bound) beside their fresh id; app **records copy paths** per original; proven removal **deletes recorded copies still at their path** and **records the removed document id**; `openCopy` refuses copies of a removed document. Moved/emailed copies and out-of-app decryption remain out of reach. Edited design (threat model, header, copies, removal, failures, tests) + plan Tasks 1.4, 7, 11, 24, 26. |
| R2-16 | Removal path after allow-list refusal cannot call `validateDownload` (no blob) | new | fixed | Split **`Reconcile::checkRemoval(baseHeader, laterHeaders, self)`** — headers only. `validateDownload` step 1 delegates to it; the allow-list-refusal path fetches the chain and calls it directly. One place concludes removal. Edited plan Tasks 1.9, 2, 24. |
| R2-17 | Ordering and tests: Task 13 before GenerationGuard; `Q_ASSERT` test; Task 21 test vs debounce; save counter incremented twice | I2, I9 | fixed | `OlderGeneration` test moved to Task 14; `keypair()`/`signingKeypair()` return `std::optional`; Task 22 updates Task 21's test to `flush()`; **only `saveDocument` increments** — `SaveResult {success, errorMessage, header}` defined, `expectedSaveCounter` removed, `writeToDisk` keeps returned header; **`IdentityService` test constructor** with no keychain, used by identity tests and the DocumentManager fixture. Edited plan Tasks 9, 10, 13, 14, 20, 21, 22. |
| R2-18 | Verification split mislabels what needs the real server | I15 | fixed | Relabelled: no server — items 1, 8, 11; companion server — 2–7, 4a, 9, 10; noted FakeServer lives only in the test executable. |
| R2-19 | *(merged into R2-10)* | I10 | fixed | No migration needed. |
| R2-20 | File-association open passes a path to an id-based API | I3, I14 | fixed | **Ward list UI (I3):** Task 10 builds the File menu — New ward, Open ward… (`WardListDialog`), Save a copy…, Open a copy…, Show in folder; `MainWindow::openFile(path)` removed. **Restore:** Task 16 restores the ward list from `/me/documents` and fetches missing files through `validateDownload`. **File routing:** Task 14 `FileOpenRouter` reads the header — in ward list → open by id; copy → `openCopy`; unknown id → Request access; unreadable → not a Ward Planning document; used at startup and for forwarded arguments. **I14 legacy half:** no special handling of `.emergencyplan` (user: no migration, treated like any non-app file) — removed `LegacyPlaintext`, whole-file probe, `LegacyFileExtension`, legacy message, design *Legacy documents* section (now *Earlier versions*), failure row, verification item 8 (renumbered). Ward list UI (I3) and header-based file routing still awaiting decision. |
| D4 | *(raised during I5)* One-time code mechanism sat in a UI task, outside the core | new | fixed | Moved `OneTimeCode` and its tests into core **Task 1.11**; added to Task 2 review list and core intro; Task 31 renamed "Device linking and in-person invite" and builds only the flows. |
| D5 | *(raised during I13)* Header/roster disagreement was surfaced in a dialog instead of refused | new | fixed | Under the strict-parsing rule, **a version whose header and roster differ is refused** like any invalid version. `unattributableEntries`/`unsealedDevices` replaced by `AccessControl::headerMatchesRoster`, checked in the load path and `validateDownload`; access dialog no longer lists mismatches. Edited design (signing, roster, server, failures, tests) + plan Tasks 1.5, 1.7, 1.9, 26. |
| R2-M1 | Fetch race and empty chain (Task 1.9) | — | fixed | **Consistent snapshot**: `GET /documents/{id}` returns version + header + blob together; `headers?after={base}&through={version}` ends at that version; no retries. `laterHeaders` never empty (empty → `ServerFault`); server at base → caller skips `validateDownload`. Edited design (endpoints, server) + plan Tasks 1.8, 1.9, 16. |
| R2-M | Minors: copy vs. USB hand-off; fetch race and empty chain; invite code length; qtkeychain options; `.cmake_marker`; `Qt6::Network`; stale version text; launch/tasks.json sites; TestMain in 1.2 files; `:279`/`:289` file; `importCopy` return; KeyStore API drift; shrunken-roster enforcement point | I12, I13, I14, I16, M1 | fixed | Minors 2–11 accepted: qtkeychain `BUILD_WITH_QT6=ON`, `BUILD_TRANSLATIONS=OFF` (Linguist check only as fallback); `.cmake_marker` advice reworded to "if no new test binary"; `Qt6::Network` noted as already linked; Task 0 table gains `CMakeLists.txt:430-541`, `launch.json` lines, `tasks.json:61,63,67`, `MainWindow.cpp:530`, `JsonService.cpp:110` (verified); `sodium_init` runner step moved from 1.2 to Task 4; `:279`/`:289` labelled `DocumentManager.cpp`; design KeyStore API `store/load/remove`; `static_assert` removed from test list; old-data-key test; sync indicator status-bar only; `GET /documents/{id}/requests/mine` for requester status. Already settled earlier: copy vs USB, code length, stale version text, `importCopy` return, shrunken roster. |

**Round 1 items not raised again in Round 2 — still open, unchanged by the restructure:**

| R1 | Item | Still true in current plan |
|---|---|---|
| I2 | `SaveResult` undefined; `expectedSaveCounter` leftover (Task 10) | **Fixed with R2-17** |
| I3 | No task builds the ward list, Show in folder, Save/Open a copy actions, `/me/documents` restore, fetch-missing-file | Yes; R2-20 covers only file association |
| I4 | "Unpublished changes" never defined or persisted | **Fixed with R2-8** |
| I5 | Who wires `SyncService` and `DocumentManager` in `main.cpp`; no removed-access signal | **Fixed:** `DocumentManager` owns decisions and files; `SyncService` is server I/O with signals (`serverAhead`, `publishRefused`, `accessRefused`, `published`, `headerChainFetched`); `main.cpp` wiring order in Task 16 |
| I9 | `IdentityService` singleton touches the real keychain in tests | **Fixed with R2-17** |
| I11 | End-emergency `setEndedAt` needs a manager method; cancel leaves `endedAt` set (Task 5) | **Fixed:** `EmergencyManager::endedSnapshot()` returns a copy with the end time; the report is made from it; live response ends only on confirm; `clearEndedAt` deleted |
| I13 | Malformed roster entries should be preserved verbatim rather than blocking saves | **Fixed (user decision): strict parsing.** Nothing is skipped or carried unread. `NeedsNewerVersion` (higher format/payload version or **unknown recipient kind** — reverses R2-6's keep/drop rule) → local copy **read-only for everything** until the app is updated, persistent notice + update check; `Malformed` → refused like any invalid version. Removed tolerant-kind handling, roster malformed-entry counting and refuse-to-write rule. Edited design (file format, post-quantum, failures, tests) + plan Tasks 1.4, 1.6, 1.7, 1.9, 2, 12, 16, 24, 26, limits. |
| I14 | Legacy-file startup message unreachable | Yes; copy/USB half is in R2-M |

**Proposed discussion order:** R2-1, R2-2, R2-3, R2-16, R2-5, R2-6, R2-7, R2-11, R2-8 with I4,
R2-9, R2-14, R2-15, R2-13, R2-12, R2-10, R2-17 with I2 and I9, R2-4, R2-20 with I3 and I14,
I5, I11, I13, R2-18, R2-M.

## Notes from Claude on Round 2 (not the reviewer)

- **Caused this session:** R2-2 and R2-16 (the chain anchor and `validateDownload` inputs
  I wrote for D1/C2), R2-3 (`leave`, added in D2), R2-5 and R2-7 (two-member removal,
  D2), R2-4 (the restructure moved the harness before the rename), and the design's
  "with no fresh document id" wording — that last one was a typo from C4, **fixed**
  immediately to "under a fresh document id".
- **Verified this session:** none of Round 2's claims yet. The reviewer marked R2-10
  (registry) and the qtkeychain and Ninja minors as recalled.
- R2-11 and R2-5 are design questions, not plan defects.

## Round 1 tracker — history (pre-restructure numbering)


Status: `open`, `discussing`, `fix`, `fixed`, `skip`

| # | Item | Status | Decision |
|---|---|---|---|
| C1 | Task 30 adopt-and-republish overwrites their work, resurrects revoked member | fixed | Every refusal → validate download (C2) → adopt their key/generation/roster → content via Task 32/33, overwrite disabled across a generation change. Drop the "adopt and publish" and "save counter moved" bullets. Revoked branch follows D1. Affects plan Task 30 and design "Local-first sync". |
| D1 | *(from C1 discussion)* Revocation deletes the removed member's local copy | fixed | Delete local copy and in-memory work on proven revocation; no recovery copy. **Every write signs its header** (Ed25519 over AD + payload hash). Recipient entries carry verify keys; server keeps every version's header. The removed member proves removal by **walking the header chain** from its own authenticated header — each writer must appear in the previous header's recipients — so a revoker granted after the last sync still verifies. Anything short of proof deletes nothing. Best-effort cleanup, documented. Also: adoption re-applies this session's grants. Edited design + plan Tasks 8, 9, 11, 15, 18, 21, 23, 30, 31, 32, 40, verification, known limits. |
| D2 | *(raised during D1)* Any member can revoke every other member | fixed | **Removal takes two members**: one proposes (signed proposal on server, hidden from target), a different member approves and performs the rotation; the approval is attached to the header. Chain rule: a dropped recipient needs an approval from a previous-header member who is neither writer nor target, bound to document id + key generation. Server enforces the same link rule on upload and on allow-list removal. Leaving yourself needs no approval. Roster of two can't remove either. Edited design + plan Tasks 8, 9, 11, 21, 40, verification, known limits. |
| C2 | Task 23 downloads replace local file without validation | fixed | One `SyncService::validateDownload` used on open and by Task 30: self still listed (else Removed path), `verifyChain` = StillMember, key unseals, payload hash, decrypt, signature vs. roster, GenerationGuard. Failure leaves file, base version, high-water mark untouched; reported as server fault. Edited design "Local-first sync" + plan Tasks 23, 30. |
| C3 | Task 33 response merge unbuildable on current models | fixed | **Base copy + three-way merge**, no model changes. Base copy written on every successful fetch/publish, hash recorded in AppSettings, read without GenerationGuard. Routing: mine differs from base only in the response → silent merge; otherwise (or no usable base) → conflict dialog. One-side change wins incl. deletions; attempts unioned; single value changed differently on both sides → **reconnecting device wins** (option a). Assignment and resolution merge as units. Edited design "Local record", emergency section, tests, limits; plan Tasks 23, 33. |
| C4 | Document id lifecycle contradictory | fixed | **App-generated 128-bit id at creation**, never changed; names the file, keys settings, bound in AD from first save. Share registers the same id (server refuses if taken). "Shared" = local record has a base version; `isShared()` removed from header. Exported and recovery copies get a **fresh id**. Edited design header list, copies, POST /documents, sharing, tests; plan Tasks 8, 12, 16, 27, 32, 36. |
| I1 | libsodium version floor doesn't enforce fix | fixed | Verified: local baseline `245eb91` builds 1.0.20 pv3 (pre-fix); upstream vcpkg has 1.0.22. Task 4: update vcpkg clone, move baseline, `version>= 1.0.22`, confirm `ad3004e` is ancestor of `1.0.22-RELEASE` (stop if not), warn about libmupdf rebuild and re-test PDF import. Design decisions row and library pin paragraph updated. |
| I2 | Task 15 cannot land as a working commit | resolved | See Round 2 tracker |
| I3 | No task for ward list UI, copies, restore | resolved | See Round 2 tracker |
| I4 | "Unpublished changes" never defined or persisted | resolved | See Round 2 tracker |
| I5 | SyncService ownership undefined | resolved | See Round 2 tracker |
| I6 | Read-only in writeToDisk leaves phantom edits | resolved | See Round 2 tracker |
| I7 | resealAll keeps unknown-kind entries on revoke | resolved | See Round 2 tracker |
| I8 | Task 32 roster merge publishes too early; allow-list race | resolved | See Round 2 tracker |
| I9 | Task 14 tests: Q_ASSERT and real keychain | resolved | See Round 2 tracker |
| I10 | QSettings on Windows writes registry | fixed | See R2-10 |
| I11 | Task 1 setEndedAt needs more | resolved | See Round 2 tracker |
| I12 | Task 39 in-person invite missing key-transfer mechanism *(now covered by R2-5's S1 one-time code, Task 32)* | resolved | See Round 2 tracker |
| I13 | Shrunken roster refusal makes document unwritable | resolved | See Round 2 tracker |
| I14 | Unreachable design paths (copy with id, legacy message) | resolved | See Round 2 tracker |
| I15 | Verification split wrong | fixed | See R2-18 |
| I16 | qtkeychain likely needs BUILD_WITH_QT6 | resolved | See Round 2 tracker |
| M1 | Minor items (see list) | resolved | See Round 2 tracker |

## Notes from Claude on Round 1 (not the reviewer)

- **I14** is a consequence of this session's change moving the server address into the
  app: a copy with a cleared id no longer leads anywhere for requesting access.
- **Task 43's version wording** was written this session without noticing Task 2 already
  moves the version to 1.1.0.
- Reviewer claims flagged as uncertain — **I1** (libsodium release date vs. fix),
  **I10** (QSettings registry), **I16** (qtkeychain option) — are not yet verified.

---

## Round 1 review (verbatim)

### Strengths
- Task 1 references are accurate throughout (178 hits across 17 files; `EmergencyManager.cpp:85,182,187-199`; `MainWindow.cpp:2,214-216,237-240,1075-1188,1193,1198-1199`; `MainWindow.h:62-63,66,120-121`; `DocumentManager.cpp:89,106,122`; view sites in `WardListView`, `TeamsView`, `ActionButtonsWidget`, and the three models).
- Task 15's two traps are real and correctly located: `QIODevice::Text` at `JsonService.cpp:13,28` and `Indented` at `:114`; the three remaining callers at `DocumentManager.cpp:148,185,361` are correct.
- Task 28's call-site count is exact (16 `persistResponseData` sites, 15 after `:187` goes; the `syncFamilies` one at `:659`).
- Build facts verified: `build.bat:19-27,36,48,51`, `CMakeLists.txt:19,370,577`, the `DocumentManager` constructor's `Q_ASSERT` + network services (`DocumentManager.cpp:33,36-40`).
- Several tests target the actual failure mode (AD-after-round-trip in Task 8; same-save-counter nonce test in Task 15).

### Issues

#### Critical (Must Fix)

**C1. Task 30 bullet 1 (plan ~1414-1416): "adopt the new key, re-encrypt, publish again" silently overwrites the other member's work and resurrects the revoked member.**
A publish refused because the server version moved means *their* content is newer; re-encrypting *my* payload and publishing it conditional on their version destroys their changes. Worse, my payload's roster still contains the person they just revoked, so the next `resealAll`/grant re-seals the new key to them and the roster disagrees with the allow-list. The design (lines 369-371) says adopt the generation, not bypass the conflict. Fix: after adopting, take *their* roster, then route content through the Task 32/33 path — never publish directly.

**C2. Task 23 on-open table "Ahead of base → Download, replace" (~1234) has no requirement to validate before overwriting the local file.**
The operator is in the threat model (design 85-90). A server reporting a higher version but serving garbage, an unauthenticating blob, or a lower key generation would replace the one copy that must survive. Task 30's "generation moved down → refuse" only covers the refusal path. Fix: download must parse, unseal, decrypt, and pass `GenerationGuard` before `QSaveFile` replaces anything; otherwise keep the local file and report.

**C3. Task 33 (~1525-1541) cannot be implemented on the current models.**
- "Later write wins" needs timestamps that don't exist: `ResponseTask` has no modified time (`EmergencyResponse.h:140-152`); `FamilyResponseRecord::m_contactStatus` is a bare enum (`:197`); `ContactAttempt` is mutable via `setMethod`/`setNotes` (`:79-80`), so "set keyed by id" doesn't resolve edits either.
- The signature takes `base`, and the test "a task removed offline stays removed" requires it, but the design says "no retained base copy" (line 459) and no task persists one. Deletions will always resurrect.
- Choosing "response records only → insert" vs. "directory changed → Task 32" requires a diff there is no base to compute — and automatic writes (`onWardLookupComplete`/`onGeocodingFinished` → `autoSave`) count as directory changes.
Fix: add per-record `updatedAt` (and tombstones or a retained base) to the model in an earlier task, and define the routing detection.

**C4. The document-id lifecycle is self-contradictory (blocks Tasks 8, 12, 16, 19, 23, 36).**
Task 8 (~538, 551): `documentId` "empty until shared," `isShared()` = non-empty. Task 12 (~789-790): every document gets a local id at creation "which the server adopts." Task 36 / design `POST /documents` (design 299): server *returns* the id. Consequences: with a local id, `isShared()` is always true and SyncService probes the server for never-shared documents; if the server assigns a different id, the app-data filename (Task 16), id-keyed high-water mark and base version (Tasks 12, 19) all need migration no task plans; and since id is bound in AD (Task 8), Share must post the blob *before* knowing the id — the server's first copy has the wrong id, needing a re-encrypt and second upload Task 36 doesn't mention. Exported copies with empty id also collapse onto one `GenerationGuard` record. Fix: pick one model (e.g., client-generated random id the server accepts verbatim) and update Tasks 8, 12, 16, 19, 36.

#### Important (Should Fix)

**I1. Task 4 Step 2 (~286-305): the libsodium floor doesn't enforce the stated fix.**
`"version>=": "1.0.20"` admits the 1.0.20 release, which (to my recollection) was tagged in 2024, before the CVE-2025-69277 fix commit `ad3004e` — so it doesn't make "newer than `ad3004e`" a constraint. Not certain; verify. Also, "adding a baseline where none existed" is stale — `vcpkg.json:4` already has `builtin-baseline 245eb91…`. Fix: verify the port version/port-version against `ad3004e` and floor on that exact value.

**I2. Task 15 (~915-956) cannot produce a working commit.**
`DocumentManager` has no header, data key, or identity until Task 17, which itself says without it the app writes files nobody can open. `SaveResult` is never defined. `lastWriter` "from this identity" isn't in the signature (hidden `IdentityService` dependency in a static class). `expectedSaveCounter` is a folder-design leftover now that Task 30 says detection is at publish. Fix: merge 15+17 or add header/key members in Task 15; define `SaveResult`; drop the counter parameter.

**I3. No task implements the document UI or restore paths.**
Task 16 says "Open becomes a list of wards," but no task builds that list, Show in folder, Save a copy / Open a copy menu actions, `GET /me/documents` restore (design 497-501, 307), or "file missing → fetch from server" (design 691). Task 20 still edits `MainWindow.cpp:287,343,351` after Task 16 removed those; `main.cpp:38-40` passes a *path* from file association while Task 18 changes `openDocument` to take an id. Verification items 1 and 10 depend on these.

**I4. "Unpublished changes" is never defined or persisted (Task 23 table ~1231-1237).**
Base version alone can't answer "I moved"; a last-published save counter (or similar) must be stored per document id, and a close during a pending publish (Task 28) otherwise loses the queue.

**I5. SyncService ownership is undefined (Tasks 18, 23, 27).**
Both `DocumentManager::openDocument(id)` and `SyncService::openDocument(id)` exist with no stated call direction; `writeToDisk` calls `SyncService::publish()` while SyncService replaces the file DocumentManager holds; no task wires either into `main.cpp`. The Task 23 test "removal from the allow-list reports removed access" has no corresponding signal or `SyncState` value.

**I6. Read-only enforced only in `writeToDisk` (Task 31, ~1442) leaves phantom edits.**
`executeCommand` mutates `m_document` before `autoSave` (`DocumentManager.cpp:97-101`), as does `setEmergencyResponse` (`:350`). A read-only session shows edits that are never saved; when the lease frees, the next flush publishes them over a newer version. Unspecified: an offline session reconnecting to a foreign lease. Fix: reject before mutation; define publish behavior when a lease is held elsewhere.

**I7. Task 11 `resealAll` (~715-719) preserves unknown-kind entries even on revoke.**
After rotation those entries hold the *old* data key (and may belong to the revoked member); a newer client gets `AuthenticationFailed` ("file altered") instead of `NoAccess`. Fix: preserve on grant, drop on rotation (or when their fingerprint was revoked), and test both.

**I8. Task 32 roster merge (~1461-1468) publishes prematurely and races.**
It publishes after merging "if the rosters differ only by additions" without checking the rest of the payload is equal — their content changes are lost. `PUT /members` is whole-list replacement, so two concurrent grants also race on the server allow-list.

**I9. Task 14 tests (~905-907) are unimplementable as written.**
`keypair()` "asserts" means `Q_ASSERT` — aborts the whole test runner in Debug, no-op in Release; Qt Test has no death tests. The `IdentityService::instance()` singleton has no account seam, so its tests (and Task 26's fixture "a generated test identity") will touch/create the developer's real keychain identity.

**I10. Tasks 3/12 (~232-234, 772-775, 799) misdescribe QSettings on Windows (to my recollection).**
`QSettings` NativeFormat writes to the registry (`HKCU\Software\<org>\<app>`), not `AppDataLocation`, and `setTestModeEnabled` does not redirect the registry — tests aren't isolated from prior runs, so every settings-touching suite needs Task 19-style `init()` clearing or `IniFormat`. Task 12's "migrated from the JSON file" can't find the file because Task 2 already moved `CacheLocation`.

**I11. Task 1 Step 2 (~111): "Call `setEndedAt()` before generating" is under-specified.**
`MainWindow` only has a const `EmergencyManager::response()`, so this needs a new manager method; if the user cancels the report, `generateEmergencyReportWithConfirm` returns false (`MainWindow.cpp:1024-1033`) and the emergency stays active with `endedAt` set. Fix: set it on a copy passed to `ReportGenerator`, or clear on cancel.

**I12. Task 39 (~1673-1679) omits the load-bearing mechanism.**
It never says how the granter's app obtains the invitee's public key (presumably from the spoofable server request list). A 6-8 character code is roughly 30-40 bits, which the named request-spoofer could plausibly grind a matching keypair fingerprint against (estimate). "Not a secret, so length is a usability choice" is the wrong framing — it authenticates a key. Specify mechanism and test.

**I13. Task 10 (~660-665): refusing to write a shrunken roster makes the document unwritable** for the session, including during an emergency. Fix: preserve raw JSON of unparseable entries and write them back verbatim, as with unknown kinds.

**I14. Unreachable design paths.**
Copies clear their id (Task 16), so "opens a `.wardplanning` file that carries a document id" (Task 37) never occurs — design contradiction too (lines 150-158). Task 41's startup legacy message can't fire: `lastDocumentPath` is lost at Task 2 and no file-open path remains.

**I15. Verification split (~1833-1834) is wrong.** Items 2-7 need a real server (`FakeServer` exists only in `tests/`); item 9 needs none.

**I16. Task 4 Step 3 likely missing option.** Not certain, but I believe qtkeychain requires `BUILD_WITH_QT6=ON` or it searches for Qt5, and may offer a translations option that removes the `Qt6LinguistTools` requirement in Step 1. Verify against the fetched `CMakeLists.txt`.

#### Minor (Nice to Have)
- Task 22 (~1174): `Qt6::Network` is already found and linked (`CMakeLists.txt:30,392`).
- Task 2 sets version 1.1.0 but Task 43 (~1772-1775) says bring CMake "up to the current release" from 1.0.0 — contradictory.
- "Before you start" (~29-32) / Task 3: I believe Ninja itself re-runs CMake when any `CMakeLists.txt` (including `tests/`) changes, so the marker-deletion instruction may be unnecessary (harmless).
- Task 2 site table incomplete: `launch.json:5,8,22,25,39,42` (only `:55` listed), `tasks.json:61,63,67`, window title `MainWindow.cpp:530`, `exportedBy` at `JsonService.cpp:110`, `CMakeLists.txt:563,567,583`; Tasks 2 and 3 both rename the test target.
- Task 28 (~1365): `:279`/`:289` are `DocumentManager.cpp` lines but read as `EmergencyManager.cpp`.
- Task 24 (~1266) forward-references "placement decisions made in Task 35."
- Task 37 (~1633): a non-member polling `head` is refused by FakeServer's "only allow-listed read" rule, so pending/denied/unknown are indistinguishable.
- Weak tests: Task 5 "every `static_assert` compiles" isn't a runtime test; Task 11 "old sealed key does not open the new payload" can't fail — assert the *old data key* fails `decryptPayload`.
- Design drift: design `KeyStore` API is `store/load/hasKey`, plan is `store/load/remove`.

### Assessment
**Ready to implement?** With fixes.
**Reasoning:** Phases 0-1 are close; the id lifecycle, adopt/merge/download sync paths, and missing response-model timestamps would lead an implementer to build data-losing or untestable behavior.

---

## Round 2 review (verbatim)

### Summary
The plan is detailed, and nearly all of its source-line references check out against the tree. But the core has several defects that would ship broken or unsafe behaviour. Nothing in the plan separates the different things one Ed25519 key signs. The chain check is anchored on the wrong header. "Leave" produces a document nobody can open. And the Phase 2 build steps name CMake targets that don't exist yet.

### Strengths
- The line references I checked are accurate. That includes the three `JsonService` callers left after Task 5 (`DocumentManager.cpp:148,185,361`; archive callers `EmergencyManager.cpp:122,160,594`, `ArchiveBrowserDialog.cpp:174`), the 15 `persistResponseData()` sites, the "178 references across 17 files" count, `JsonService.cpp:13,28` (`QIODevice::Text`) and `:114` (`Indented`), `MainWindow.cpp:59,68,86,97,214-216,237-240,592-595,630-668`, `Document.h:158`, `Document.cpp:642-656`, `CommandHistory.cpp:114-117`, `DocumentManager.cpp:36-40`, and `build.bat:19-27,36,48,51`.
- The traps it calls out are real: the `QIODevice::Text` CRLF corruption (plan line 1237), AD built from serialised strings (328-339), the legacy `Indented` first-line probe (1365-1372), and the `build.bat:51` deploy path breaking on rename (1018-1029).
- The emergency-merge rules line up with the model. `ContactAttemptId` exists (`Id.h:124`), and the task fields match `EmergencyResponse.h:97-153`.

### Issues

#### Critical (must fix)

**1. No domain separation between the things one Ed25519 key signs. The operator can forge a removal and delete members' local copies.**
- **Where:** Task 1.2 `sign()` (plan 212), Task 1.4/1.5 `signedBytes`/`signRemovalProposal` (297, 403-406), Task 16 challenge-response (1459-1462).
- **Evidence:** the same signing key signs three things:
  - server-chosen challenge bytes
  - header `signedBytes()`
  - removal proposals

  None of them carries a context prefix. `verifyLink` never looks at the payload (395-400). The design promises removal is proven "on evidence the server cannot forge" (design 233-237).
- **Why it matters:** the operator is in the threat model (design 88-90). It can issue challenges whose bytes are exactly a crafted header's `signedBytes()` from member A and a removal proposal from member D. That yields a chain which verifies. Member B's app returns `Removed` and deletes the local file (Task 24). Signing a header needs no data key.
- **Fix:**
  - Prefix every signed message with a distinct context string, e.g. `"WardPlanning/header/v1\0"`, `"…/removal/v1\0"`, `"…/auth/v1\0"`.
  - Have the auth signature cover a structured message (context, server origin, nonce), never raw challenge bytes.
  - Make `sign`/`verify` take the context as a parameter, so a raw path doesn't exist.
  - Add a test that a signature from one context fails verification in the others.

**2. The chain is anchored on the local file's header, which breaks every conflict where the user has unsaved-to-server work.**
- **Where:** Task 1.9 `validateDownload(localHeader, …)` (637-641) and step 2 (665); Task 17 (1529-1530); Task 24 step 1 (1721-1723).
- **Evidence:**
  - Every local write sets `saveCounter + 1` and a fresh nonce (Task 10, 1255-1257).
  - Local saves are never uploaded, so the local header is not in the server chain.
  - `verifyLink` requires the save counter to increase (394-395).
  - Another member's next header is based on the base version's counter, and may be lower than the local counter after N offline saves.
- **Why it matters:**
  - Every refused publish after local edits yields `Broken` → `ServerFault`. That is exactly the conflict/emergency-merge path, which then never runs.
  - A removed member who kept editing is never cleaned up.
  - `adopt` already takes `baseHeader` (652), so the two functions disagree about the anchor.
- **Fix:** anchor `validateDownload` and `verifyChain` on the base copy's header, whose hash is recorded (Task 17, 1510-1515). Add a test with the local counter ahead of the other writer's counter.

**3. `leave` produces a version no remaining member can open.**
- **Where:** Task 1.5 (383-389), Task 1.9 step 3 (666-667), Task 10 load (1258-1261), Task 33 Leave (1972-1973).
- **Evidence:** the leaver writes the header, so `lastWriter` is the leaver. Loading looks `lastWriter` up in the decrypted roster, and "a writer absent from the roster fails". `leave` has just removed the leaver from that roster.
- **Why it matters:** after any leave, every member gets `SignatureInvalid` locally and `ServerFault` on download, permanently. The test `leaveRemovesSelfAndLinkVerifiesWithoutApproval` (556) only checks the link, so it passes anyway.
- **Fix:** check the writer against the previous roster, or against the verify key in the previous header's recipient entry (as `verifyLink` does). Add a test where a remaining member opens the version written by a leave. Also specify whether `leave` rotates the data key; the plan doesn't say.

**4. Phase 2 build steps reference `WardPlanningLib`, which doesn't exist until Task 6.**
- **Where:** plan 35, 102, 911-916 (Task 4 `target_link_libraries(WardPlanningLib …)`); Task 3 renames the test executable (832).
- **Evidence:** the target is `EmergencyPlanLib` (`CMakeLists.txt:355,370,388`), and `tests/CMakeLists.txt:38` links `EmergencyPlanLib`. The rename is Task 6.
- **Why it matters:** Task 4's CMake edits fail at configure. Task 4 is also the gate where "every core test must pass".
- **Fix:** use `EmergencyPlanLib` in Tasks 3-4, or move the rename before Task 3. Task 4's `vcpkg.json` snippet (862-863) also uses the post-rename port name and version early.

#### Important (should fix)

**5. One person can satisfy the two-member removal rule alone.**
- **Where:** Task 1.5/1.7/1.8; design 103-108, 71-72.
- **Evidence:** any key holder may grant (design 71), and the roster lists machines, not people (456-459). A member can grant a second keypair they generated themselves, then use it to approve their own proposal.
- **Why it matters:** the design lists "one member acting alone removing others" as defended against, which is the data-loss scenario. The plan builds and tests a rule that doesn't deliver it.
- **Fix:** raise this with the user as a design question (e.g. a grace period before removal-by-new-member, or counting approvers by grant age) before Task 1 is written. At minimum, correct the threat-model claim.

**6. Keeping unknown-kind entries through a key rotation leaves stale, wrong entries.**
- **Where:** `resealAll` "preserving any entry whose kind this build does not recognise" (524-529), used by `revoke`.
- **Why it matters:**
  - After rotation, a kept entry still seals the old data key, so the newer client it belongs to can't open the document.
  - If the removed member also has an unknown-kind entry, that entry is kept. `verifyLink` then doesn't see the member as missing, and their chain says `StillMember`.
- **Fix:** on rotation, drop unknown-kind entries (a newer client re-adds its own), or at least drop those whose fingerprint matches a removal target. Specify which, and add a test with an unknown-kind entry for the target.

**7. Allow-list replacement after revocation may never succeed.**
- **Where:** Task 1.8 rules (591-595), Task 33 steps 2-3 (1968-1970).
- **Evidence:** step 2 uploads the new generation. The fake then "clears proposals when the key generation moves". Step 3 must carry "a valid `RemovalApproval`", but that approval is bound to the previous generation.
- **Why it matters:** if the server checks approvals against the current generation, step 3 always fails, and the revocation stays "incomplete" forever.
- **Fix:** have the server accept an allow-list removal when the current accepted header contains the matching approval, and state which generation it validates against.

**8. The emergency merge routing can't be tested with the declared API, and the open-time path skips it.**
- **Where:** Task 1.10 (701-710, 743-750), Task 17 table (1525).
- **Evidence:**
  - `ResponseMerge::merge` takes non-optional `EmergencyResponse`, but `Document` holds `std::optional` (`Document.h:133`). An emergency started or ended on one side isn't covered.
  - `name`, `startedAt`, `endedAt` and `taskCategories` (`EmergencyResponse.h:238-242`) have no merge rule.
  - The tests "routes to the conflict dialog" and "missing or hash-mismatched base copy" have no function in the core to exercise.
  - Task 17's "Ahead of base + unpublished → Conflict" skips adopt and merge, so a responder who reopens the app after reconnecting gets the dialog instead of a silent merge.
- **Fix:**
  - Add a core `Reconcile` routing function that returns Merge or Conflict and a merged `Document`.
  - Handle the optional response case and the unlisted fields.
  - Route the open-time "both moved" case through the same Task 24 sequence.

**9. Automatic writes outside the response defeat response-only routing.**
- **Evidence:** `onFamilyGeocoded` (`DocumentManager.cpp:326-336`) and `onWardLookupComplete` (`:263`) change families and metadata on their own. Both fire when the network returns, which is the reconnect moment.
- **Why it matters:** `onlyResponseChanged` returns false, so responders get the conflict dialog.
- **Fix:** hold these updates while a reconcile is pending, or leave them out of the comparison.

**10. `QSettings` doesn't write under `AppDataLocation` on Windows, and test mode doesn't isolate it.**
- **Where:** Task 7 (1081-1084), Task 3 comment (806-808, 1108-1109).
- **Evidence:** the plan relies on `QSettings`' default format. On Windows that is the registry, `HKCU\Software\<Org>\<App>`, which `QStandardPaths::setTestModeEnabled` doesn't affect. (Recalled from Qt docs, not verified this session.)
- **Why it matters:**
  - The stated durability and location rationale is wrong.
  - Tests share persistent registry state across runs. Task 14 even patches around this in `init()`.
- **Fix:** use `QSettings::IniFormat` with an explicit path under `AppDataLocation`, and point it at a temporary directory in the test runner.

**11. The lease makes an online emergency single-writer.**
- **Where:** Task 19 (1585-1591), Task 25.
- **Evidence:** a live foreign lease means `ReadOnly`. The merge path only applies while offline.
- **Why it matters:** during a connected emergency, every responder but one is read-only. That contradicts the design's purpose (design 546-548), and would push people to disconnect in order to record contacts.
- **Fix:** raise it with the user. One option: exempt response-only writes from the lease and reconcile them with `ResponseMerge` on every publish.

**12. Read-only enforced only at `writeToDisk` silently loses edits.**
- **Where:** Task 25 (1761-1766).
- **Evidence:** `executeCommand` changes `m_document` and the undo stack before `autoSave` (`DocumentManager.cpp:97-101`). The same is true of `setEmergencyResponse` (`:350`).
- **Why it matters:** in a read-only session, the UI accepts edits that are never saved, or that get written later over newer content once the lease frees up.
- **Fix:** refuse at the mutation entry points (`executeCommand`, `undo`, `redo`, `setEmergencyResponse`) as well as in `writeToDisk`. Also specify how a read-only session refreshes when the lease holder publishes.

**13. Task 26's roster merge contradicts Task 24.**
- **Where:** 1780-1786 compared with 1724-1729.
- **Evidence:** Task 26 does "`mergeRosters`, then `resealAll` … under the current data key, publish". Task 24 already adopted their key and grants, and says "Nothing publishes this session's payload over theirs directly."
- **Why it matters:** an implementer can't tell which data key and payload get published. Following Task 26 as written overwrites their content.
- **Fix:** remove the duplicate from Task 26 and rely on `Reconcile::adopt`.

**14. `adopt` has no defined behaviour without a usable base copy.**
- **Where:** 652-656, 713-715.
- **Evidence:** adopt runs before routing, and needs `baseHeader` to tell a local grant from a remote removal.
- **Why it matters:** with no base, a revoked member could be restored.
- **Fix:** state the fallback (take their roster only). Also state that grants carried through adoption must re-issue the allow-list update.

**15. "Overwrite theirs" can't be carried out as specified.**
- **Where:** Task 26 option 3 (1794-1797).
- **Evidence:** the upload is conditional on a stale base version. The local header's save counter may not exceed theirs. If they granted someone, mine lacks that entry, which counts as a drop without approval under `verifyLink`.
- **Why it matters:** the server refuses it even within the same generation.
- **Fix:** specify an overwrite that rebuilds the header from theirs (their base version, their counter + 1, their recipients) carrying my payload. Or drop the option.

**16. The removal path can't call `validateDownload` after an allow-list refusal.**
- **Where:** Task 24 (1731-1733).
- **Evidence:** a refused key can't fetch the blob, but `validateDownload` requires `ciphertext`.
- **Fix:** split out a chain-only check for this path.

**17. Task ordering and tests.**
- Task 13's test for `OlderGeneration` (1382) depends on `GenerationGuard`, which arrives in Task 14.
- Task 9's test that `keypair()` asserts (1216) can't be written in QtTest. `Q_ASSERT` aborts the whole runner. Return an invalid value or `std::optional` instead.
- Task 21's test (1653-1655) breaks once Task 22 debounces; it needs `flush()`.
- Task 21 says `writeToDisk` owns the save-counter increment (1642-1643), but Task 10's `saveDocument` also increments (1255). Pick one, or the counter jumps by 2.

**18. The verification list mislabels what needs the real server.**
- **Where:** 2116-2117.
- **Evidence:** `FakeServer` exists only in `tests/`, so items 2-7 in the running app need the companion server. Item 9 (emergency start and end) needs no server.

**19. Task 7 migrates from a file Task 6 has already orphaned.**
- **Where:** Task 7 "migrated from the JSON file" (1092).
- **Evidence:** Task 6 changes the names that set the `CacheLocation` path (1056-1062), so the migration finds nothing.
- **Fix:** do the migration in Task 6, or read the old path explicitly.

**20. File-association opens aren't designed.**
- **Where:** Task 15 only swaps the extension at `main.cpp:38`.
- **Evidence:** `window.openFile(args[1])` takes a path, but Task 13 moves `openDocument` to ids.
- **Fix:** route to `importCopy` or Request access, per the design (170-172).

#### Minor (nice to have)

- **Design contradiction:** design line 613-614 says Save a copy uses "no fresh document id"; lines 183-185 say copies get a fresh id. Also, a fresh-id copy can't offer "Request access" for the ward's document, which conflicts with the USB hand-off in design 170-172. Plan Task 11 picks fresh ids silently; flag it to the user.
- **Task 1.9:** head, headers and blob are fetched separately. A server moving between fetches produces a hash mismatch, which gets reported as `ServerFault` when it should be retried. Also, `laterHeaders` empty has no defined behaviour.
- **Task 32:** a 6-8 character code is 30-40 bits, which the operator (in the threat model) can grind against a request it can see. Size the code against a stated bound rather than calling its length a usability choice.
- **Task 4:** from recollection, not verified this session, qtkeychain 0.15 needs `BUILD_WITH_QT6=ON` and has a `BUILD_TRANSLATIONS` option. Turning that option off would remove the LinguistTools prerequisite in Step 1. The plan already asks for the option names to be checked.
- **Plan 29-32, 834-836:** I believe the Ninja build re-runs CMake itself when any `CMakeLists.txt` changes, including `tests/`, so the `.cmake_marker` warning is probably unnecessary. That is recalled, not verified.
- **Task 16 (1455):** "Add `Qt6::Network` to the link line" is already done (`CMakeLists.txt:30,392`).
- **Task 36 (2054-2058):** "`CMakeLists.txt:4` says 1.0.0" will be stale after Task 6 sets 1.1.0.
- **Task 6 table:** it cites only `launch.json:55`. `launch.json:5,8,22,25,39,42` and `tasks.json:61,63,67` also carry `EmergencyPlan`.
- **Task 1.2 Files** lists `tests/TestMain.cpp`, which Task 3 creates. **Task 22** cites `:279`/`:289` right after `EmergencyManager.cpp` references, but those lines are in `DocumentManager.cpp`.
- **Task 11:** `importCopy` returns a `LibraryEntry`, which suggests the copy is added to the library, contradicting "never becomes the live document".
- **Design/plan drift:** the design's `KeyStore` has `hasKey` (design 325); the plan's has `remove`.
- **Unenforced rule:** "refuse to write a roster that shrank because of a parse failure" (470-471) has no enforcement point in Task 10's `saveDocument` signature.

### Assessment
**Overall:** Needs Work. Findings 1-3 are defects in the core's security and correctness: forgeable removals, conflicts failing whenever the user has unsaved-to-server edits, and unopenable documents after a leave. Finding 4 stops Phase 2 at configure. All of these should be fixed before Task 1 is written.

---

## Round 3 tracker — Fable reviewer, current plan numbering

Run after all Round 2 decisions, with this document moved out of the repository.

| # | Item | Status | Decision |
|---|---|---|---|
| R3-1 | `PersonId` already exists (`Id.h:60`) — crypto type collides | fixed | Crypto type renamed **`MemberId`**; identifiers `memberId()`, `ownMember`, `RecipientEntry::member`; prose "person id" → "member id" in both docs; comment in 1.1 distinguishes it from the directory's `PersonId`. |
| R3-2 | `WardListDialog` already exists — new "Open ward" dialog would overwrite it | fixed | New dialog named **`OpenWardDialog`** in Task 10. |
| R3-3 | `revoke`/`completeLeave` have no caller parameter to enforce "caller's person" rule | fixed | Both take `const MemberId& ownMember` and return `std::optional<AccessChange>`: `revoke` refuses caller = target or proposer's person; `completeLeave` refuses caller = leaver; tests added. Plan Task 1.7. |
| R3-4 | `Malformed` payload has no `LoadFailure`; `Document::fromJson` change breaks Task 4 build | fixed | Core 1.6 adds `Document::parse` (`Ok`/`NeedsNewerVersion`/`Malformed`) **beside** `fromJson`, so Task 4 builds; Task 9 switches `JsonService` to `parse` and deletes `fromJson`. |
| R3-5 | Task 9 never defines the new `JsonResult`; `SignatureInvalid` used before `LoadFailure` exists | fixed | Task 9 defines `LoadFailure` (`HeaderMalformed`, `PayloadMalformed`, `NeedsNewerVersion`, …) and `JsonResult {failure, errorMessage, header, dataKey, document}`; Task 12 keeps open-by-id and per-failure tests. |
| R3-6 | Link/unlink "in every ward document" has no multi-document write path | fixed | **Option 2: `RosterChangeQueue`** — persisted per document; open ward via `DocumentManager` (lease-gated); other wards in background: take lease, fetch snapshot, validate against base, apply on top of server version, publish, release; local file replaced only if no unpublished changes; retries on backoff; pending shown in access dialog. New device polls `/me/documents` (also minor 17). Edited design (identity) + plan Task 31. |
| R3-7 | Server contract gaps: link outcome to new device; `requests/mine` auth for non-member; `lookup`/`tag` on requests | fixed | New device learns member id/display name from its first ward's authenticated roster (no outcome endpoint). **Two auth levels**: *device* (key possession — `/me/documents`, `requests/mine`, posting requests) and *member* (in current header — everything else). `lookup`/`tag` in `ServerContract.h` and fake. Edited design (server auth, endpoints, identity) + plan Tasks 1.8, 15, 31. |
| R3-8 | Roster operations (grant, revoke, leave, link) bypass the read-only gate | fixed | All roster-changing operations check `mayWriteDirectory() && !requiresUpdate()` before touching the roster: Grant/Approve disabled with reason; complete-leave only by the lease holder; open-ward link/unlink wait in the queue. Proposing, leave requests, and local deletion ungated. Tests added. Edited plan Tasks 24, 30, 31, 32. |
| R3-9 | Task 0 misses `tests/CMakeLists.txt:33,37,38` | fixed | Task 0 table cites `tests/CMakeLists.txt:33,37,38,48`. |
| R3-10 | `maybeRenameForWard()` unreplaced; Task 14 cites lines Task 10 deletes | fixed | Task 10 deletes `maybeRenameForWard()`; each save updates the library entry's `wardName` from `Document::suggestedFilename()`; test added; Task 14 drops `DocumentManager.cpp:406,431`. |
| R3-11 | Task 23 removes settings records that the removed-documents marker needs | fixed | Task 23 lists the per-document records deleted and keeps the removed-from record. |
| R3-M | Minors 12–20 | fixed | 12 Task 16 table row restored, snapshot wording; 13 `DocumentHeader::parse` only; 14 `AccessChange::dataKey` comment; 15 Unlink wording; 16 `IdentityService::setInstanceForTesting`; 17 fixed with R3-6; 18 fake verifies a first header (one person, own-device writer, self-signed); 19 Task 0 fixes Qt 6.10.1 paths (verified); 20 "about 180 — recount". |

**Verified by Claude this session:** R3-1 (`Id.h:60` `class PersonId`), R3-2 (`src/widgets/WardListDialog.h/.cpp` exist), R3-9 (`tests/CMakeLists.txt:33,37-38,48` carry old names), R3-10 (`maybeRenameForWard` at `DocumentManager.cpp:409`, called at `:377`).

## Round 3 review (verbatim)

### Summary
The plan is unusually well grounded — nearly every file:line citation I checked matches the tree, and the crypto/format traps it calls out (`QIODevice::Text`, AD built from serialized strings, fresh nonce per write) are real and correctly located. But it has two name collisions with existing code that will break the build as written, plus several API/contract gaps that would stall an implementer at Tasks 1.7, 9, 29, 31, and 32.

### Strengths
- Citations verified accurate: `build.bat:19-27,36,48,51`; `CMakeLists.txt:3,5,19,30,370,392,513,525,528,537,577,602`; `src/main.cpp:12-13,25,33,38`; `DocumentManager.cpp:36-37,89/106/122,97-101,136-144,148,185,304,326,354-368,361,392-407,406,431`; `EmergencyManager.cpp:85,122,160,182,187-199,554-557,594` and the 15 `persistResponseData()` call sites (count confirmed); `JsonService.cpp:13,28,110,114`; `Document.h:158`; `Document.cpp:642-656`; `CommandHistory.cpp:114-117`; every `MainWindow.cpp`/`.h` archive site; `Info.plist`, `installer.iss`, `launch.json`, `tasks.json`, `.gitignore:93`, `USER_GUIDE.md:14`.
- Plan line 1499-1503: the `QIODevice::Text` warning is correct and load-bearing — both `C:\Users\johnv\Documents\Church\WardPlanningQt\src\services\JsonService.cpp:13` and `:28` open with `Text`.
- Plan line 2049-2052: confirmed no `closeEvent` override or `aboutToQuit` connection anywhere under `src/`.
- Plan 1.4 (lines 421-451): the serialize-then-build-AD rule and the `toJson`/`fromJson` round-trip AD test is the right test; "byte-identical across two constructions" would indeed miss the timestamp trap.
- Plan 1.8: FakeServer enforces the real rules (chain verification, 7-day clock, allow-list = header recipients) rather than being permissive.

### Issues

#### Critical (must fix)

1. **`PersonId` already exists and the plan redefines it.** Plan 1.1 line 187 declares `PersonId (16)` in `src/services/CryptoTypes.h`, and it is used throughout (`RecipientEntry::person` 347, `RemovalApproval::target` 357, `AccessControl` 639, `IdentityService::personId()` 1428, `Reconcile`). `C:\Users\johnv\Documents\Church\WardPlanningQt\src\models\Id.h:60` already defines `class PersonId : public IdBase<PersonId>`, used in 61 files (354 occurrences), including `EmergencyManager.h:55` and `Document.h`. Task 1.6 puts `AccessRoster` inside `Document`, so `Document.h` will include both — a redefinition error in every TU. Fix: rename the crypto type (e.g., `MemberId` or `RosterPersonId`) everywhere in the plan.

2. **`WardListDialog.h/.cpp` already exists with a different purpose.** Task 10 (lines 1550, 1608) lists `src/widgets/WardListDialog.h/.cpp` as files to create for the "Open ward…" list. `C:\Users\johnv\Documents\Church\WardPlanningQt\src\widgets\WardListDialog.h:36` is an existing family/person picker (`selectFamilies`, `selectPersons`) used by `EmergencyAssetView.cpp`, `TeamsView.cpp`, and `NeedsSubView.cpp`, and is in `WIDGET_SOURCES` (`CMakeLists.txt:230`). An implementer following the plan overwrites a live dialog. Fix: name the new dialog something else (e.g., `OpenWardDialog`).

#### Important (should fix)

3. **`AccessControl::revoke`/`completeLeave` cannot enforce the rule their comments state.** Plan lines 648-659: "The caller's person is neither the proposer's nor the target", and test `revokeRefusesAnApprovalFromTheCallersOwnPerson` (689). The signature `revoke(header, roster, approval)` carries no caller identity; `header.lastWriter` is the *previous* writer (Task 9 line 1528-1530 sets `lastWriter` at save time). Fix: add a caller parameter (`ownPerson`, as `linkDevice`/`unlinkDevice` already take at 639/645).

4. **`Malformed` payload result has no home, and `Document::fromJson`'s change breaks the Task 4 build.** Task 1.6 (594-597) says the roster/payload parse reaches `JsonResult` as `NeedsNewerVersion` or `Malformed`; Task 12's `LoadFailure` (1654-1658) has neither `Malformed` nor a payload-malformed value (`HeaderUnreadable` is the header). Separately, changing `Document::fromJson`'s return type breaks `JsonService.cpp:94` (`result.document = Document::fromJson(json)`), yet Task 4 Step 5 (1188-1193) requires the tree to build and every core test to pass before Task 9 rewrites `JsonService`. Fix: add a `PayloadMalformed` value; state whether 1.6 keeps `fromJson` and adds a `parse` beside it, or whether Task 4 patches `parseJson`.

5. **Task 9 never specifies the new `JsonResult`.** `loadDocument(filePath, keys)` (1506) returns `JsonResult`, which today is `{success, errorMessage, document}` (`JsonService.h:8-13`). Task 11 (1638-1639) needs the header and data key out of the load, and the design (1041-1047) says "the implementation plan should carry the concrete signatures". Task 9 also uses `SignatureInvalid` (1533) three tasks before `LoadFailure` exists. Fix: give `JsonResult` its fields (`failure`, `header`, `dataKey`, `document`) in Task 9 and define `LoadFailure` there.

6. **Link/unlink across "every ward document" has no write path.** Task 31 (2360-2362, 2367-2368) runs `AccessControl::linkDevice`/`unlinkDevice` in every document this device belongs to, but the plan's `DocumentManager` holds one `m_document` (`DocumentManager.h:99`) and Task 20's `writeToDisk` is the only write path — for that one document. No task describes loading a non-open document, mutating its roster, re-saving, publishing, or taking its lease. Fix: add an explicit batch operation (open by id via `JsonService::loadDocument`, apply, `saveDocument`, publish) and say how it interacts with the lease and the currently open document.

7. **Server contract gaps for flows the plan builds.**
   - Task 31 (2362-2364): the existing device "tells the new device (through the same link record) the person id and display name" — the design's endpoint table (design 459-473) has only `POST /links` and `GET /links/{lookup}` ("authenticated as any allow-listed device"). There is no write-back endpoint and no way for the not-yet-allow-listed new device to read the outcome. Task 1.8's fake rules (748-749) do not cover it.
   - Task 29 (2305-2307): `GET /documents/{id}/requests/mine` is "authenticated by the requesting device", but authentication is defined as challenge-response "against the devices in the document's current header" (design 66, 480-484). A requester is by definition not in the header. The endpoint needs its own auth mode, and the fake must implement it.
   - Task 31 (2370-2372) adds `lookup`/`tag` to an access request; Task 1.8's shapes don't mention them.
   Fix: add all three to Task 1.8's `ServerContract.h` and FakeServer rules.

8. **Roster operations bypass the read-only gate.** Design 540-541 puts the roster under the lease. Task 24 (2145-2150) gates `executeCommand`/`undo`/`redo` with `writeToDisk` as backstop. Grant (Task 30), revoke/leave (Task 32), and link/unlink (Task 31) mutate the roster via `AccessControl` outside commands, and nothing gates them at entry. "Completing a leave ... with no prompt" (2416-2417) on a `ReadOnly` session would mutate memory and then be refused at the backstop — the exact failure Task 24 (2151-2152) warns against. Fix: state that each roster operation checks `isDirectoryReadOnly()`/`requiresUpdate()` before touching the roster, with tests.

9. **Task 0 misses rename sites in `tests/CMakeLists.txt`.** Plan cites only `:48`. `C:\Users\johnv\Documents\Church\WardPlanningQt\tests\CMakeLists.txt:33` (`add_executable(EmergencyPlanTests`) and `:37-38` (`target_link_libraries(EmergencyPlanTests PRIVATE EmergencyPlanLib`) also carry old names. Task 3 (1096-1097) assumes the rename is done, so when `TEST_SOURCES` goes live the link against a non-existent `EmergencyPlanLib` fails at configure. Fix: cite 33, 37, 38, 48.

10. **`maybeRenameForWard()` has no replacement, and Task 14 cites lines Task 10 deletes.** Task 10 removes `ensureFilePath()` but says nothing about `maybeRenameForWard()` (`DocumentManager.cpp:409-443`, called from `autoSave()` at `:377`). Design 988-992 says the ward name becomes a display label "set at the same moment as before"; with no task doing that, `LibraryEntry.wardName` stays "Untitled" after an import. Task 14 then lists `DocumentManager.cpp:406,431` as extension sites — both inside functions Task 10 deleted. Fix: Task 10 replaces `maybeRenameForWard` with updating the library entry's `wardName` from `Document::suggestedFilename()` on the same trigger; Task 14 drops those two lines.

11. **Task 23 wording conflicts with the removed-documents record.** Line 2114-2115 says to "remove the library entry and its `AppSettings` records", while Task 6 (1327) keeps "the ids of documents this device was removed from" and Task 10's `forgetRemoved` "records the removal". Fix: say which per-document records go and that the removed-from marker stays.

#### Minor (nice to have)

12. Task 16 table (1854-1866): the `| Unreachable | Either | ... |` row at line 1866 is separated from its table by two paragraphs and renders as stray text. Line 1869 "headers after the local version" should read "after the base version" (per 1846-1849).
13. Task 1.4 declares both `fromJson` returning `std::optional` (383) and `parse` returning `HeaderParse` (414); pick one.
14. `AccessChange::dataKey` comment (623) "unchanged on grant, freshly generated on revoke" omits `unlinkDevice` and `completeLeave`, which also rotate.
15. Task 26 (2237-2238): "Unlink on each of your devices other than this one's last companion (disabled on your only device)" is garbled.
16. Task 19: `IdentityService::instance()` is a static singleton (1417); say whether the test constructor (1475) registers as the instance, since the fixture's `DocumentManager` will call it.
17. Task 31: the new device fetches `/me/documents` right after joining, but the existing device only "queues the publish" — a race; the new device should poll.
18. Task 1.8: the first header on `POST /documents` has no previous to `verifyLink` against; say the fake verifies the writer signature against the header's own `lastWriter` entry.
19. `.vscode/launch.json:15,32,62,79,96` and `tasks.json:97,110` still point at Qt 6.10.1 (toolchain is 6.11.0 per `CMakePresets.json:39`); Task 0 edits both files and could fix it in passing.
20. Task 5 says "178 references across 17 files"; case-insensitive grep finds 179 — trivial.

### Assessment
**Overall:** Needs Work — the citations and core cryptographic guidance are sound, but two name collisions break the build at Tasks 1 and 10, and the `revoke` signature, `JsonResult` shape, multi-document link path, and three server-contract gaps would each stop an implementer before the corresponding task could be completed as written.

---

## Round 4 tracker — adversarial security review (Fable)

Run after all Round 3 decisions, with this document moved out of the repository. The
reviewer was pointed at the design's own threat model and asked for concrete attack
sequences.

| # | Item | Status | Decision |
|---|---|---|---|
| S4-1 | One member swaps a device's verify key, then forges that device's leave request — removes anyone alone, impersonates writes | fixed | **Fingerprint = BLAKE2b(X25519 pk ‖ Ed25519 vk)**; recipient entries now carry the X25519 public key so readers recompute it. Chain rule 4: fingerprint matches keys, keys unique in header, a device in both headers keeps member id and both keys. A device refuses a version whose entry for its fingerprint carries other keys (`validateDownload` step 3). Tests added. Edited design (header, rule 4, fingerprints) + plan Tasks 1.1, 1.2, 1.4, 1.5, 1.6, 1.9, 8. |
| S4-2 | One-time code tag is an offline oracle — server brute-forces the ~40-bit secret and links its own key | fixed | **Option (b): commit–respond–reveal pairing with a six-digit comparison number** (Bluetooth-style). Core 1.11 `OneTimeCode` replaced by `Pairing` (`commit`, `revealMatches`, `comparisonNumber`, non-secret pairing code). Endpoints `/pairings`, `/pairings/{code}/responder`, `/pairings/{code}/reveal`, each step write-once, 10-minute expiry. Linking and in-person invite both use `PairingDialog`; Grant/link proceed only after the numbers are confirmed. Edited design (identity, in person, endpoints, failures, tests) + plan Tasks 1, 1.8, 1.11, 2, 31, verification. |
| S4-3 | 7-day wait checked on the approval signer, not the writer; client reads writer-controlled `grantedAt` | fixed | Server refuses a removal when the writer's **or** approver's person is <7 days old by its clock; first-appearance times returned with head and chain; apps use them for "new" and Approve; `grantedAt` removed from the roster. Edited design (server rules, head endpoint, roster, granting, tests) + plan Tasks 1.6, 1.8, 26, 32. |
| S4-4 | Reusing a former member's id skips the 7-day wait and the announcement | fixed | **Member ids never reused**: server refuses a header bringing back an id from any earlier kept header; `AccessControl::grant` refuses an id already in the roster; re-granted people get fresh ids; announcements keyed by (member id, first-appearance time). Edited design + plan Tasks 1.7, 1.8, 6, 26. |
| S4-5 | Removal and unlink need the lease — the target can block its own removal forever | fixed | **Roster changes need no lease**: all (grant, link, unlink, revoke, complete leave) go through `RosterChangeQueue`, applied on top of the server's current version and published; the open ward receives it as a newer version and merges. Lease covers only families/ministering/ward details (reverses R3-8's lease gate; `requiresUpdate` gate stays). **Only the proposer can withdraw.** No lease age cap. Edited design (decisions, lease, propose, endpoints, tests) + plan Tasks 1.8, 18, 24, 30, 31, 32. |
| S4-6 | Unverifiable "newer format" head from a hostile server freezes the app, emergency included | fixed | User judged the forgery low-value; **simpler fix, no envelope**: `requiresUpdate` is re-evaluated on every head check and clears when the server serves an understood version. **Emergency response stays editable locally** while an update is required (user decision, revising I13's "read-only for everything"); directory read-only, publishing stops, response records merge after update. Edited design (file format, failures, tests) + plan Tasks 16, 24, limits. |
| S4-7 | High-water mark lost with app data (and deleted on removal) enables rollback to a revoked insider's key | fixed | User noted it needs a former member + hostile operator + lost app data; chose the cheap fix anyway. Marks stored **in the keychain record** beside the keys (document id → generation), **never deleted** (including on removal); `AppSettings` keeps a working copy raised from the keychain at startup. Edited design (rollback defence, identity, tests) + plan Tasks 6, 7, 13, 23. |
| S4-8 | Withdrawn proposal is a bearer token for removal until the next rotation | fixed | Server accepts a removal only when its approval matches a proposal **currently pending**; withdrawal deletes, a completed removal consumes. No signed expiry. Residual: needs member + operator collusion (chain check can't see pending). Edited design (revoking, tests, limits) + plan Tasks 1.8, 32. |
| S4-9 | Server equivocation (forks) is silent | skip | **Accepted and documented** (user decision): operator-only, disruption without exposure; detection would need every header kept or client-to-client comparison. Added to design threat model and limits, plan known limits. |
| S4-10 | Stolen device can plant devices under its owner, surviving the phone's unlink | fixed | Unlinking a device lists every device it linked (from kept headers' writers) and offers **Unlink both** by default; access dialog shows which device added each device and when; design's Accepted paragraph and limits updated. Edited design + plan Tasks 26, 31, limits. |
| S4-11 | Plan weaker than design on `GET /links/{lookup}` auth; lookup overwrite | fixed | Superseded by S4-2: `/links` removed; pairings readable only by their two parties, responder needs member auth, each step write-once, and the pairing code carries no security. |
| S4-M | Minors: leave before request accepted; phantom wards; former devices read chain forever; document-id squatting; local IPC access; unbounded registrations; duplicate names flagged only for granter | fixed | **Minors 1, 3–7 accepted:** leave deletes locally only after the server accepts the request; former devices read headers only through the one that removed them; Share re-ids a squatted document; `QLocalServer::UserAccessOption`; `POST /documents` rate-limited and capped per key; duplicate display names flagged in every member's access list. **Minor 2 (phantom wards) fixed:** option (b) — server accepts a header adding a device only with that device's pending access request for the document or a completed pairing with the person it joins; remote request path kept. Design server rules + tests; plan Task 1.8. |

## Round 4 review (verbatim)

I have what I need; the code references the plan relies on check out (`JsonService.cpp:13,28` use `QIODevice::Text`, `:114` uses `Indented`, `Document.h:158` has no failure channel, `tests/CMakeLists.txt:14-29` is all commented out, `vcpkg.json` is at baseline `245eb91` with a bare `libmupdf`). Below is the adversarial review. `D` = `C:\Users\johnv\Documents\Church\WardPlanningQt\docs\plans\2026-08-30-encrypted-shared-document-design.md`, `P` = `C:\Users\johnv\Documents\Church\WardPlanningQt\docs\plans\2026-08-30-encrypted-shared-document-plan.md`.

### Summary

The cryptographic envelope, AD binding, tagged signatures, and "delete nothing without a trusted anchor" rule are sound and carefully specified. The membership chain rule and the one-time-code scheme have holes that let a single member remove anyone without a second person, and let the server operator obtain a roster key; the 7-day slowdown is bypassable with an unmodified app.

### Strengths

- Every signature is domain-tagged and there is no untagged `sign` overload (D:347-359, P:304-308); this genuinely closes the server-chosen-challenge attack.
- AD is built from serialised strings in fixed order with the `QJsonDocument::toJson` and `QDateTime` traps called out (P:424-442); the round-trip test at P:450-453 is the right one.
- Fresh random 192-bit nonce on every write, with the concurrent-save case named as the reason (D:231-238, P:535-538, P:1597-1601).
- `verifyLink` requires the writer to be found in `previous`, never only in `next` (P:493-495), and `checkRemoval` returns `ServerFault` with no base header (P:826-828, P:865-867). Nothing is deleted without an anchor.
- The allow-list is the accepted header itself (D:453-458), so there is no second list to drift, and the fake server enforces the same rules as the real one (P:744-786).
- `QIODevice::Text` removal is flagged as the single most important line (P:1558-1562); confirmed present at `JsonService.cpp:13,28`.

### Issues

#### Critical (must fix)

**1. A single member can remove anyone, make their app delete its copy, and impersonate their writes, by swapping a device's verify key.**
Location: D:257-279 (chain rule 4), P:498-499 (rule 4), P:302-306 (`headerMatchesRoster`).
Attacker: any one roster member M, no puppet, no waiting, no operator.
Sequence:
1. M writes header H1 that is identical to the previous one except that X's device D1 keeps its fingerprint and member id but its `verifyKey` is replaced by K', a key M holds. M writes the payload too, so the roster's entry for D1 also carries K'. Rule 1 passes (M signed with M's key), rule 2 passes, rule 4 passes ("belongs to the same person in both"), rules 5/6 are not triggered (no device added or dropped by fingerprint), `headerMatchesRoster` passes (header and roster agree on K'), `verifyHeader` passes (M is the writer). The server keeps H1 because the recipient tuple changed (D:284-290), which only helps the attack.
2. M signs a `removal`-tagged leave request `{docId, target=X, gen}` with K' and sets `proposer = D1's fingerprint`. M writes H2 dropping X entirely with that approval attached and the generation bumped. Rule 7: the approval verifies against `previous`(H1)'s entry for D1, whose key is K', and that entry "belongs to the target (a leave request)" (P:506-509). The server's 7-day check exempts leave requests (P:769). Accepted.
3. X's devices fetch the chain from their base copy: H1 and H2 both verify link by link, H2 has no entry for X → `Removed` → local file, base copy, and recorded copies deleted (D:630-639, P:2175-2184). X's *own* app is what wipes the data.
The same trick lets M sign as D1 in later headers (rule 1 checks the signature against H1's K'), breaking write attribution (D:67, D:252-256), and lets M unlink X's other devices "as X" (rule 6). Between H1 and H2 the server also refuses X's real key at the allow-list, so X is locked out but sees only "server fault".
Breaks: "Defended against, within the roster: one member acting alone removing others" (D:105-110); "Every header signed by its writer's Ed25519 key… attributable to a roster member" (D:252-256).
Fix: derive the fingerprint over both public keys (`BLAKE2b(X25519 pk ‖ Ed25519 vk)`) so any verify-key change is a new device and falls under rule 5; additionally make rule 4 require identical `{member id, verify key}` for any fingerprint present in both headers, require verify keys and X25519 keys to be unique across the header, and have each device treat a downloaded header whose entry for its own fingerprint carries keys other than its own as `ServerFault`. Add tests `linkChangingVerifyKeyOfExistingDeviceIsBroken` and `linkWithLeaveRequestSignedByReplacedKeyIsBroken`.

**2. The one-time code can be brute-forced offline by the server, which then substitutes its own key and gets sealed the data key.**
Location: D:391-402, D:867-878; P:1022-1045 (`OneTimeCode`), P:2439-2444, P:2490-2492.
Attacker: server operator (the primary threat, D:88-90).
Sequence:
1. New device posts `{keys, deviceName, lookup, tag = keyedBLAKE2b(keys, secret)}`. The secret is "8 characters' worth" from an alphabet of about 31 symbols, roughly 40 bits (P:1023-1028).
2. The server holds `keys` and `tag`, which is a complete offline verification oracle. 2^40 keyed-BLAKE2b evaluations is minutes on a GPU, well within the 10-minute expiry (P:471), and the server also controls when it answers the existing device's `GET /links/{lookup}`.
3. Having recovered the secret, the server replaces the stored request with its own X25519/Ed25519 keys and a freshly computed matching tag, copying the device name.
4. The person types the code on their existing device; `matches` succeeds; `AccessControl::linkDevice` seals the data key to the server's key under the person's member id and publishes. The server can now decrypt every version of every ward that person belongs to (the queue carries the link to all wards, P:2439-2441).
5. The real new device polls `/me/documents` forever and the user retries; the roster on the existing device shows a device with the expected name. Nothing looks wrong.
The in-person invite path (P:2490-2492) has the identical weakness. The design's claim that "the server gets one online guess" (D:400-402) is wrong because the tag is a keyed hash the server can test locally.
Breaks: "The server operator reading member data" (D:88-90); "a hostile server cannot substitute a key of its own that matches" (D:871-873, P:1038-1040).
Fix: the tag must not be cheaply verifiable offline. Either run a PAKE between the two devices through the server, or derive the tag key from the secret with `crypto_pwhash` (Argon2id, `SENSITIVE` limits, salt = lookup) so each guess costs seconds and hundreds of MB, making 2^40 guesses infeasible inside the window; encrypt the posted keys under that derived key rather than just tagging them. Independently, the existing device should display the fingerprint it is about to link and the new device should display its own, and the new device must verify after linking that the header entry under its fingerprint carries exactly its keys. Also require `POST /links` to refuse a lookup that is already pending, otherwise the outsider variant below applies.

#### Important (should fix)

**3. The 7-day waiting period is checked against the wrong party on the server and against attacker-controlled data on the client, so one member with a puppet skips it using an unmodified app.**
Location: D:502-504, P:767-769 (server checks the *approval signer*), D:74, D:900-903, P:2523-2524 (design intent: the *approver/writer* waits), P:585 (`grantedAt` "as recorded by the granting device"), P:2311-2312.
Sequence: M grants puppet P today, backdating P's roster `grantedAt` by a month (M's device writes it). M proposes removing T. P's app (M's second machine, stock build) evaluates "granted at least 7 days ago" from the only time source it has, the roster, enables Approve, writes the removal header with M's proposal attached. Server rule: the approval was *signed by M*, whose id is old → accepted. T's copy is deleted the same day P was granted.
Breaks: "Made visible and slowed, not prevented… enforced by the server's clock and by every app" (D:112-119).
Fix: the server must refuse a removal header when *either* the writer's person *or* the approval signer's person first appeared under 7 days ago (server clock). Expose the server's first-appearance time in `GET /head` (or in the chain response) so the client's "new" badge and Approve gating never read roster `grantedAt`. Add fake-server tests for a new *writer* with an old proposer.

**4. Reusing a former member's id defeats both the 7-day wait and the grant announcement.**
Location: P:770-771 ("records when each member id first appears"), P:2317-2320 (announced set keyed per person), D:112-119, P:642-646 (`grant` does not say what happens when `newPerson.member` already exists).
Sequence: Q was a member two years ago and left. M grants a "new person" whose member id is Q's (member ids are chosen client-side; rule 8 is unrestricted; rule 4 is satisfied because the device is fresh). The server's first-appearance record for that id is two years old, so the puppet can approve immediately. Every other member's app has already "announced" that id, so nothing is shown. Combined with 3, M needs nothing else.
Fix: the server records first appearance per continuous membership stint (reset whenever the id is absent from an accepted header), or better, the chain rule refuses a member id introduced in `next` that appears in any earlier kept header; a re-granted person gets a fresh member id (the re-grant path can update the keychain record). Announcements keyed by `(member id, header version)` rather than by member id. `AccessControl::grant` must refuse a member id already in the roster.

**5. Any roster change, including removing a person and unlinking a stolen phone, needs the lease, so the target can block its own removal indefinitely.**
Location: D:549-551 (lease covers the roster), P:2233-2238 (approve/complete-leave/unlink gated on `mayWriteDirectory()`), P:2485-2486 (queue retries "when the lease is held elsewhere"), P:2549-2550.
Sequence: T (hostile member, or a thief with T's unlocked phone) takes the lease on every ward and keeps heartbeating. Two other members propose and approve T's removal; the approver's app refuses to write ("another member is editing"), retries on backoff, forever. Meanwhile T keeps fetching every version, including emergency-response records published by responders (which need no lease). The owner of a stolen phone cannot unlink it for the same reason. A puppet of T can additionally withdraw every proposal against T as soon as it appears (D:893-894 "any other member can withdraw").
Breaks: "a lost phone is cut off everywhere within minutes" (D:404-407); "the removed person's devices are refused from that moment" (D:902-903); Accepted-risk text at D:121-124 assumes recovery works.
Fix: the server accepts a `PUT` whose header drops the lease holder's person or device regardless of the lease and revokes the lease; give the lease a hard maximum age; make withdrawal of a proposal possible only by its proposer (the server knows the proposer's key). The displaced lease holder's next publish takes the ordinary refused-publish route, which already ends in `Removed`.

**6. An unverifiable "newer format" header from the server freezes the app, emergency response included.**
Location: D:188-192, P:1941-1946, P:2208-2211.
Sequence: a hostile, compromised, or drop-caught server (D:1304-1310 says the domain trust model accepts a stranger at the address) answers `GET /head` with `{formatVersion: 99}`. Parsing refuses whole, so no signature can be checked, yet the app enters "read-only for everything", stops publishing, and re-enters that state on every open. During an emergency no responder can record a contact until an update that never arrives.
Breaks: "The server going down denies *sharing*, not *use*" (D:147-149); "Every member continues to open, edit, and save" (D:33-36).
Fix: keep local editing (at least the emergency response) fully available under `NeedsNewerVersion` and stop only publishing; and wrap every header in a version-independent outer envelope `{formatVersion, writerFingerprint, signature}` that an old app can verify against its own roster before it believes a newer version exists. Note the same server can also put every member's directory into "another member is editing" indefinitely via a fake lease holder; that one is milder because users can disconnect to edit.

**7. Losing the high-water mark lets a hostile operator roll a member back so that a removed insider reads their future edits.**
Location: D:412-415 (mark lives in application data), P:2179-2182 (mark *deleted* on removal), P:1964-1967 (no base copy after reinstall).
Sequence: T is removed at generation g+1 and colludes with the operator. Member A reinstalls or loses application data. The server serves A the last generation-g version (A can decrypt it, no base copy so the chain is skipped, no mark so step 4 passes). A edits and publishes under the generation-g key; the operator hands the blob to T, who holds that key.
Breaks: "A revoked person's access to future versions" (D:92-93).
Fix: keep the per-document high-water mark (at minimum the highest generation seen) inside the keychain record next to the identity, which the design already treats as the durable store; never delete it on removal.

**8. A withdrawn proposal is a bearer token for removal until the next rotation, which may be years away.**
Location: D:919-923, P:2734-2735 (acknowledged but understated).
Sequence: X proposes removing T, thinks better of it, withdraws. M saved the bytes. A year later M alone attaches it and removes T; "two people" consented at moments a year apart, and X never intended this outcome.
Fix: the server accepts an approval only when it matches a proposal currently in its pending list (withdrawal deletes it), and proposals carry a signed expiry. The chain rule cannot see time; the server can.

**9. Server equivocation is silent.**
Location: D:96 ("silent corruption" defended), D:539 (indicator states).
Sequence: the server accepts A's v5, tells B the head is v4, accepts B's v5', and keeps serving each member their own fork. Both indicators say "Up to date"; neither member's edits ever reach the other. Because save counters must increase, the forks can never be rejoined, so the ward silently splits.
Fix (detection, not prevention): include the previous header's hash in each header so a client that ever sees two heads for one version can prove it; and have each client verify, on every head fetch, that the served head's chain passes through its own last accepted publish.

**10. A stolen unlocked device can plant persistent devices, not just unlink.**
Location: D:121-124 ("Accepted"), rule 5 (P:500-501).
Sequence: thief on Bob's phone links their own laptop under Bob. Bob later unlinks the phone from his desktop; the thief's laptop remains a device of Bob's with a plausible name, reading every future version until Bob notices and unlinks it, or two others remove and re-grant Bob.
Fix: on unlink, show and offer to unlink every device that the unlinked device itself added (the chain records `lastWriter` for each link); state the consequence in the design's Accepted paragraph.

**11. Plan is weaker than design on `GET /links/{lookup}`.**
Location: D:472 ("member authentication on any document") vs P:782 ("readable by any authenticated device").
With the plan's rule, anyone who installs the app can enumerate the ~20-bit lookup space, read pending link requests (public keys, device names, tags), brute-force the 40-bit secret as in finding 2, and, if `POST /links` allows overwriting a pending lookup, substitute keys without being the operator. Fix as in 2, plus refuse overwrites and match the design's authentication level.

#### Minor (nice to have)

- **Leave deletes before the server has the request** (D:906, P:2536-2538). Offline Leave deletes the local copy while the leaver stays on the allow-list; persist the pending leave and delete only after `POST /removals` is accepted.
- **Phantom wards**: anyone who knows a device's public keys (every header, every file copy) can register a document and grant that device, and it appears in the victim's ward list via `/me/documents` (P:1964-1968). Require the device to have an outstanding request or link before auto-adding, or present new wards for acknowledgement.
- **Former devices read the chain forever** (D:467, D:498-499), so a removed person watches later membership metadata indefinitely. Serve former devices headers only through the one that removed them.
- **Document-id squatting**: a managed file copied out via Show in folder keeps its id; the recipient can register it first and the owner's Share fails permanently (P:2355-2357). Offer re-id on "taken".
- **Local IPC**: `QLocalServer` without `UserAccessOption` (P:1342-1346) lets other local users forward "open this path" to the running instance; `POST /documents` with device-level auth allows unbounded registrations against a 5 GB free tier (D:1261-1263).
- **Duplicate display names** are flagged only in the granter's own dialog (P:2408-2410); other members' access dialogs should flag them too, since the granter is the party being defended against.

### Assessment

**Overall:** Needs Work — findings 1 and 2 each defeat a headline guarantee (single-member removal with copy deletion; operator obtains a roster key), and 3–6 are cheap for a single member or the operator to exploit, but every one has a contained fix within the existing structure.

---

## Round 5 tracker — adversarial security review with owner's cost-benefit weighting (Fable)

Run after all Round 4 decisions, with this document moved out of the repository. The
prompt carried the owner's weighting: balance operator-only disruption and member-plus-
operator attacks on cost and benefit; findings whose only outcome is a member exposing
data they already hold are not meaningful.

| # | Item | Status | Decision |
|---|---|---|---|
| S5-1 | Spoofed access request over a forwarded invite link; fingerprint check has nothing to compare against | skip | **Accepted (user decision, option c):** remote requests stay unverified because any comparison adds too much friction. Design no longer claims the fingerprint check meets the attack; threat model, granting, hosting note, and limits state the accepted risk and what limits it (name-match flag, announcements, 7-day approval rule, removal, in-person pairing). Grant dialog shows fingerprint only under Details. |
| S5-2 | Pairing safety depends on each side storing the other's message before posting; plan never says so | fixed | Order stated in 1.11 and Task 31: responder fetches and **keeps** the commitment before posting keys and verifies the reveal against the kept copy; initiator keeps the responder keys before revealing and computes the number from them; no re-reads. Fake-server control serves different pairing contents to successive reads, with tests. New device accepts a ward only if the adding header's writer is the paired device. Edited design (linking, tests) + plan Tasks 1.8, 1.11, 31. |
| S5-3 | Look-alike ward via `/me/documents`; linking device takes identity from first listed ward | fixed (part) | **Part 1 only** (user: hostile server is rare; balance against usability): responder signs a `Handover` {member id, display name, ward ids} under a new `pairing` tag over the pairing nonce; the new device joins with that identity and adds only those wards. **Parts 2–3 skipped** (keychain set of requested docs, unexpected-ward prompt) and documented as accepted in the design threat model and limits. Edited design (signature tags, linking, threat model, limits) + plan Tasks 1.2, 1.11, 31. |
| S5-4 | Two-person roster with compromised device has no recovery; no "new ward from copy" | fixed | User rejected "new ward from copy": the way out of a two-person ward is **both leave**, then start a new ward and re-import. Made leaving unconditional — local copies deleted immediately on confirm, leave request queued (reverses S4-M1's wait-for-acceptance), **Leave always available including for the last person**, replacing "Remove from this computer" for shared documents. Roster tidy-up by a remaining member is cosmetic. Edited design (leaving, last person, two-person ward) + plan Tasks 26, 32. |
| S5-5 | 7-day rule: proposer age not gated; waiting-period refusal retried until it succeeds | fixed | Waiting period now covers writer, **proposer**, and approver; Remove disabled for anyone <7 days; new waiting-period `ServerError`, treated as **final** (change dropped, approver told). **Exemption: the ward's creator** — the person in its first header — so a new ward works from day one (user decision). Edited design (decisions, server rules, tests) + plan Tasks 1.8, 31, 32. |
| S5-6 | Withdrawn-proposal replay needs only the operator | fixed | Design wording corrected: the operator alone can re-offer a withdrawn proposal; operator-only disruption, undone by re-granting. No signed expiry. |
| S5-7 | Rollback within a key generation for a device with no base copy | fixed | Keychain record also keeps a **save-counter high-water mark** per document; a lower counter is refused like a lower generation. Design rollback defence + plan Tasks 7, 13. |
| S5-8 | Key generation can jump to `INT_MAX`, blocking all later rotation | fixed | Chain rule 2 requires the generation to stay equal or rise by exactly one; `saveCounter` is `qint64`; chain test added. Design rule 2 + plan Tasks 1.4, 1.5. |
| S5-9 | Poisoned sealed key for one member persists across saves | fixed | Every save **re-seals all recipients** from the roster instead of carrying entries forward. Design file format + plan Task 9. |
| S5-10 | Removal deletes unpublished emergency work | skip | Documented as a limit: rare, deliberate, and the removed person already holds the data. |
| S5-11 | Removed-from record in app data, not keychain; stale after re-grant | fixed | Moved into the keychain record; cleared when the device reappears in a current header. Plan Tasks 6, 7, 11, 23. |
| S5-12 | Leave request cleared by rotation leaves leaver stranded | fixed | A "leaving" state persists in the keychain record until the person is gone from the roster; the request is re-posted after a rotation. Plan Tasks 7, 32. |
| S5-13 | Indefinite directory read-only from a live lease or perpetual `NeedsNewerVersion` | skip | No change: a stale lease is takeover-able, roster changes need no lease, and the newer-version half is operator-only disruption re-checked on every head fetch (S4-6). |
| S5-14 | Spec drift: access request auth level; request signature coverage | fixed | Access requests are **device-authenticated**, the only write open to a non-member; endpoint table and authentication paragraph aligned. Signature coverage unchanged (TLS covers replay). |

## Round 5 review (verbatim)

### Summary

The cryptographic core is sound and unusually well-argued (tagged signatures, AD-bound headers, base-anchored removal proofs, allow-list = header recipients). The weak points are at the edges where a human or the server relays public keys: the invite-link grant path, the pairing message ordering, and `/me/documents`. Each is a place where the plan as written would let an implementer build something weaker than the design intends.

### Strengths

- Domain-separated signatures (design `C:\Users\johnv\Documents\Church\WardPlanningQt\docs\plans\2026-08-30-encrypted-shared-document-design.md:370-382`; plan `...-plan.md:310-314`) close the challenge-as-header forgery cleanly, and the plan forbids an untagged overload.
- Chain rule 1 requires the writer to be in the *previous* header (plan `:499-501`), and removal is only ever concluded from a base-copy anchor (design `:685-698`, plan `:898-900`). This is what makes "the server cannot forge a removal" true.
- Allow-list is the accepted header's recipients with no second step (design `:492-497`); member ids never reused (design `:546-548`); first-header rule for `POST /documents` (plan `:775-778`).
- Nonce policy and the explicit warning against counter-derived nonces (design `:243-250`).
- Fingerprint covers both keys; a device checks its own entry (design `:332-341`).
- The plan's chain test list (`:549-577`) covers every rule I tried to break by hand.

I spot-checked the plan's code references (`JsonService.cpp:13,28`, `DocumentManager.cpp:36`, `Document.h:158`, `main.cpp:13`, `tests/CMakeLists.txt:14,32`, 17 `persistResponseData` occurrences = definition + 15 sites + `:187`); they match the tree.

### Issues

#### Critical (must fix)

**1. Spoofed access request over a forwarded invite link; the fingerprint mitigation is not verifiable as specified.**

- *Attacker:* anyone holding the invite link (not a member, not the operator). Alternatively the operator, by substituting the public keys inside a genuine request.
- *Sequence:* (1) Attacker learns a forwarded link and the name of the incoming counselor. (2) Posts a request `{name: "Tom Hale", device: "Tom's laptop", keys: attacker's}`. (3) The bishop, expecting Tom's request, sees the name, device, and a 32-hex-char fingerprint he has nothing to compare against, and clicks Grant. (4) Attacker's device is sealed the data key and is on the allow-list; full read/write. (5) Real Tom's request arrives later; the name-match flag says "Tom already has access, if this is his new phone he should link it", which reads as *Tom's* mistake.
- *Guarantee broken:* "Anyone who obtains a file without ever having held a key" (design `:92-93`), and the claim that spoofed requests are "met by the grant dialog's fingerprint check" (design `:917-925`, `:1376-1378`). Plan Task 29 (`:2444-2462`) never has the requester's app display its own fingerprint, and Task 36 (`:2755-2763`) never tells the granter to compare one, so the check has nothing to check against.
- *Cost-benefit:* attacker effort is trivial; gain is the whole directory; harm is exactly what the feature exists to prevent; the fix costs one label and one sentence of guide text. This is not the "member already holds the data" category.
- *Fix:* Make the pairing-number comparison the standard grant path for remote invites too (the code and number work fine over a phone call); the plan already builds it for in-person (`:2573-2576`). Keep the bare Request-access path but show the requester's own grouped fingerprint on the pending screen with "read this to the person granting you access", and put a warning on Grant when no number was compared. Hosting note `:1376-1378` should stop citing the fingerprint check as sufficient.

#### Important (should fix)

**2. Pairing: safety depends on read-before-write binding that the plan never states, and the specified tests would pass without it.**

- *Attacker:* hostile operator (relay).
- *Sequence (responder side unbound):* (1) Initiator posts `C = commit(K, n)`. (2) Responder posts `R` **without first fetching and retaining `C`**. (3) Initiator fetches `R`, reveals `(K, n)`. (4) Server now knows `K, n, R`; it computes `N = number(K, R, n)`, then searches `n'` (about 10^6 BLAKE2b calls) for `number(K', R, n') == N`, and returns to the responder `C' = commit(K', n')` together with reveal `(K', n')`. (5) `revealMatches(C', K', n')` passes, both screens show `N`, Bob confirms, the responder links the **operator's** device under Bob's person in every ward. Symmetric attack if the initiator reveals before fetching and retaining `R`.
- *Guarantee broken:* design `:425-429` ("must commit before it sees the existing device's keys... cannot search") and plan `:1075-1084`. The assumption holds only if each side binds the other's message before posting its own. Task 31 (`:2507-2515`) describes the dialogs without that ordering, and the 1.11 test "a simulated relay that swaps keys after commitment" (`:1089-1091`) exercises the pure functions, not the fetch order.
- *Cost-benefit:* operator only, but the gain is a full confidentiality break (a permanent device under a real member), which is the design's primary guarantee. The fix is zero-cost.
- *Fix:* State in 1.11 and Task 31: responder fetches and stores `C` before `PUT responder`, verifies the later reveal against the stored `C`; initiator fetches and stores `R` before `PUT reveal`, computes its number from the stored `R`. Add a fake-server control that serves different pairing state to successive reads, and a test that the dialogs refuse. Cheap belt-and-braces: after linking, the new device accepts a ward only if the header that added it has `lastWriter` whose verify key equals the `R` it displayed.

**3. Look-alike ward through `/me/documents`, and a linking device takes its identity from whichever ward the server lists first.**

- *Attacker:* hostile operator. The design names this attack (`:555-558`) and closes it with a *server-side* check, which the operator does not run.
- *Sequence:* (1) Operator creates document D' with its own device, then a second header adding Bob's device (public keys are in every header Bob is on) under any member id, with a payload naming "Tanglewood Ward" and an empty directory. (2) Bob reinstalls or clears app data; plan `:2024-2027` fetches `/me/documents` and "restores them without asking" (design `:557-558`). (3) Bob opens what looks like his ward, sees it empty, re-imports the ward PDF; the operator holds D's key and reads it. (4) Worse for a newly linked phone: Task 31 (`:2511-2515`) has it read its member id and display name from "the first ward's authenticated roster"; the roster of D' is authenticated by the operator, so `joinPerson` gets the operator's chosen member id and the device is wrong in every real ward.
- *Guarantee broken:* "The server operator reading member data" (design `:89-91`) and "Nothing about its identity comes from the server" (plan `:2515`).
- *Cost-benefit:* operator only; needs Bob to enter data into a suspicious empty ward, so likelihood is moderate; harm is disclosure of whatever he types; fix is small.
- *Fix:* (a) After the number compare, the responder signs `{member id, display name, ward ids}` under a new `pairing` tag with the verify key the initiator just authenticated; the initiator accepts only those wards and that identity. (b) Keep the set of document ids this device requested or was paired into in the keychain record beside the high-water marks; a ward in `/me/documents` outside that set is shown as "You were added to a ward you did not ask for" rather than added silently.

**4. A two-person roster with a compromised device has no recovery, and nothing in the app can fork a ward from a copy.**

- *Scenario:* roster {Alice, Bob}; Bob's unlocked phone is stolen (an accepted scenario, design `:121-127`).
- *Sequence:* (1) Alice cannot remove Bob (design `:979-981`). (2) Alice grants Carol and waits 7 days; the thief, as Bob, grants an invented "Dave" the same day. (3) At day 7 whichever side proposes and approves first removes the other, and the removed side's app deletes its local copy on next connect. (4) Alice's escape is Save a copy, but `openCopy` is read-only and "never becomes the live document" (plan `:1711-1713`); New ward starts empty. The same missing path is the rescue the design leans on when the server goes away (design `:866-867`) and for finding 8 below.
- *Guarantee affected:* "recovery is two other people removing that person" (design `:124-125`) silently assumes three or more people; the design says nothing for the two-person case beyond "either can leave".
- *Cost-benefit:* requires a stolen unlocked device; the thief already holds the data, so this is lockout/disruption. Worth fixing anyway because the fix is one action and it also serves the server-loss and generation-overflow cases.
- *Fix:* "Create a new ward from this copy": decrypt a copy this device can open, write it as a fresh unshared document with a fresh id and this device as the sole roster. Document it as the recovery for a compromised two-person ward.

**5. The 7-day rule is under-specified on the client: proposer age is never gated, and a waiting-period refusal would be retried until it succeeds.**

- *Sequence:* (1) Bob, granted yesterday, proposes removing Tom; nothing in Task 32 disables Remove for him (`:2603-2606` gate Approve only). (2) Alice (old) approves; `Revoke` is enqueued. (3) The server refuses because the proposer is under 7 days (design `:544-547`). (4) Task 31 says the queue "retries on a backoff when the server is unreachable or the publish is refused" (`:2569`), and `ServerError` (`:1755-1757`) has no waiting-period value, so the refusal looks retryable. (5) On day 7 the retry rebuilds the header on the current version and the server accepts. Tom is removed and his copies deleted by a decision Alice made six days earlier and has no way to withdraw.
- *Guarantee weakened:* "a person granted within the last 7 days cannot approve a removal, so the invented person sits visibly... during which the others can remove it" (design `:113-116`). The week still elapses, but the second human action does not happen inside it.
- *Cost-benefit:* attacker gain is small (the delay survives); the user-side harm is a removal firing without a fresh decision. Fix is a few lines.
- *Fix:* Add `ServerError::WaitingPeriod`; the queue drops the change and tells the approver; disable Remove (propose) for anyone under 7 days with the reason, matching what the server checks.

#### Minor (nice to have)

**6. Withdrawn-proposal replay needs only the operator, not a colluding member.** Design `:974-977` and `:1235-1236` say replay "needs a member and the server operator acting together". Sequence: Bob proposes, withdraws; the operator keeps it and shows it to Alice as pending; Alice approves in good faith; Tom is removed and his copies deleted. Operator-only, disruption-only, and recoverable by re-grant, so by the owner's weighting this is low. Still worth two cheap changes: correct the wording, and put a signed timestamp in the proposal so the approver's app shows its age and refuses one older than, say, 30 days.

**7. Rollback within a key generation against a device with no base copy.** Fresh install, cleared data, or a new grantee skips the chain check (design `:660-662`, plan `:901-903`) and accepts any decryptable version at or above the generation mark, so the operator can serve any genuine older version at the current generation. Disruption only, operator only. Cheap closure: keep a save-counter high-water mark beside the key-generation mark in the keychain record; the save counter is monotonic in the server chain, so a lower one is refused the same way.

**8. Chain rule 2 allows arbitrary jumps; a single member can pin `keyGeneration` at `INT_MAX`.** After that no rotation can satisfy "the key generation increases" (rules 6 and 7), so unlink and removal are impossible for that document forever, and every device's high-water mark is pinned. Inside the trust boundary, but permanent and subtle, and the fix is one comparison: require `next.keyGeneration` to equal `previous` or `previous + 1` in `verifyLink` and on the server. Make `saveCounter` a `qint64` while there.

**9. A poisoned sealed key for one member persists across ordinary saves.** `saveDocument` carries recipients forward (plan `:1637-1639`); rule 4 checks keys, not that the sealed box opens. A tampered app (or a bug) writing garbage in one member's `sealedKey` locks that member out of every later version with "server fault" while everyone else reads "Up to date". Inside the trust boundary. Simplest fix: `resealAll` on every save (sealing is cheap at roster sizes here, and sealed bytes are not part of the server's membership-change set), or a "Repair access" action any member can run.

**10. Removal deletes unpublished work with no hand-off.** A responder released and removed while holding offline emergency records loses them (plan `:2239-2244`, "no recovery copy is written"), contradicting "the record of who was contacted and when survives" (design `:796-797`) for that case. Rare, deliberate, and the removed person already holds the data, so low. Possible fix: when the local counter exceeds the published one, offer to write a hand-off copy sealed to the remaining roster's public keys (known from the last header) before deleting.

**11. Durability mismatch and stale removed-from record.** The high-water mark lives in the keychain so it survives cleared data (design `:350-353`); the removed-from set lives in `AppSettings` (plan `:1407`), so clearing data reopens copies. Also, a device removed and later re-granted is refused its own copies forever. Move the set to the keychain record; clear an id when the device reappears in a current header.

**12. Leave edge case.** The leaver deletes its copy on request acceptance (plan `:2619-2622`); if any rotation clears the pending leave before a remaining member completes it, the leaver stays on the roster with no local file and `/me/documents` restores the ward on next start. Persist a "leaving" state and re-post the request after a rotation.

**13. Indefinite read-only from a live lease or a perpetual `NeedsNewerVersion`.** A member who leaves the app open, or a hostile operator, holds every other directory read-only; design `:205` says a false newer version "cannot freeze the app beyond that" but a hostile server can present it on every check. Operator-side this is accepted disruption. For the honest case, consider a maximum lease age or a takeover-with-warning for live leases.

**14. Spec drift.** Design `:514` and `:534` call `POST /documents/{id}/requests` "the one unauthenticated write"; design `:526-528` and plan `:807-809` require device authentication. Pick one; it decides how rate limiting is keyed. Also, request signatures cover document id and body hash but not method, path, or a timestamp (design `:531-533`); replay by anyone but the operator is prevented by TLS, so this is hygiene only.

### Assessment

**Overall:** Acceptable — the core cryptography and chain rules hold up against every sequence I could construct, but the invite-link grant path (finding 1) and two relay-ordering gaps (findings 2 and 3) would let the plan be implemented weaker than the design intends, and all three are cheap to close before Task 1 is written.
