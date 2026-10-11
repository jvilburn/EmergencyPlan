# Hand-off: encrypted shared document, Tasks 20-28

For the next session continuing `docs/plans/2026-08-30-encrypted-shared-document-plan.md`.
Delete this file once the plan is finished.

## Where things stand

**Branch:** `docs/encrypted-shared-document`. Tasks 0-19 are done and committed; each
task's section in the plan opens with a **Done, with these differences** note recording
what was built where it departs from the plan text. Read those notes before relying on
the plan text for anything Tasks 11-19 touched.

| Commit | What |
|---|---|
| `be6cee1` | Task 18 review fixes - **built? not yet: build and test this first** |
| `eb13f22` | Task 19, ward notices - **review findings unfixed: see below** |
| `11bd912` | Task 18, access roster dialog |
| `263ded9` | Fix: wards folder made before a fetch |
| `65855e8` | Task 15-17 review fixes |
| `e5b23b2`, `2b9a3d5`, `f19f6de` | Tasks 17, 16, 15 |
| `75d706f`, `527929d` | Task 13 and its review fixes |
| `e2f2a62`, `24c7b6f` | Task 14 and its review fixes |
| `ce3029b`, `51e4956` | Task 12 and its review fixes |
| `4eb2f4f`, `526df76` | Task 11 and its review fixes |

**Last verified:** the build after `eb13f22` passed 694 tests, 0 failed
(`testresults4`). `be6cee1` changes `DocumentSession::versionAdopted`'s signature and
several call sites and has not been compiled.

**First steps for the new session:**

1. Ask the user to build (Ctrl+Shift+B) and run the tests; fix what fails.
2. Ask the user whether to adopt the review policy below.
3. Fix the Task 19 review findings below that the policy keeps, then continue at Task 20.

`testresults`, `testresults2`-`4` in the repository root are the user's test output
files, untracked.

## How the work runs

**Building and testing.** Never build from a Claude shell. The user builds with
Ctrl+Shift+B and runs `build/windows-debug/bin/WardPlanningTests.exe > testresultsN`;
read the file. Before trusting a run, compare the test binary's timestamp with the last
commit (`ls -la --time-style=full-iso build/windows-debug/bin/WardPlanningTests.exe`):
one run here tested a binary older than the fix. Summarize with
`grep -n "^FAIL\|QFATAL"` and the `Totals` lines. The conformance run against a real
server is skipped unless `WARD_PLANNING_SERVER_URL` is set; there is no real server yet.

**Per task:** implement, add a **Done, with these differences** note under the task's
heading in the plan (and fix its **Files:** line), commit with the plan's message (no
attribution lines - the user's CLAUDE.md overrides the harness), then dispatch the
`review-readonly` agent on the commit in the background. Relay its report verbatim.
The review prompt names the commit, the plan section, the files, and the standard
(CLAUDE.md, docs/CODING_STYLE.md, the plan) - nothing about what to look for.

**Editing.** Files are a mix of CRLF and LF; preserve each file's endings. Multi-line
edits went most reliably through Python scripts written to the scratchpad and run with
`python`, reading and writing with `encoding='utf-8'` and asserting each old string
occurs once. Heredocs through the Bash tool mangle backslashes. Keep everything ASCII.

**Style that reviews enforce:** Allman braces, braces always, one-line functions only
in headers, no multi-line lambdas (bind a named member with `std::bind`; lambdas are
accepted only as the `std::function` edit callbacks to `DocumentSession::edit` and
`DocumentManager::editResponse`), `tr()` on every user-visible string, every new file
in `CMakeLists.txt` and test in `tests/CMakeLists.txt` plus `tests/TestMain.cpp`.

## Review policy - proposed, not yet confirmed by the user

Fixing every review finding, minor ones included, roughly doubled the cost of each task,
and most fixes added code. Proposed instead:

- Fix correctness and data-loss findings.
- Take a minor finding only when its fix removes code or is a line or two.
- List the skipped ones in the task's plan note.
- Batch two or three tasks per build-and-test checkpoint.

Before fixing anything by adding a check, field, or path, ask what could be removed so
the problem cannot arise (the user's standing rule; see their CLAUDE.md memory on
simplifying before adding).

## Task 19 review findings, unfixed

From the read-only review of `eb13f22` (`src/widgets/WardNotices.*`). The first two are
real bugs, and one change fixes both at the source.

1. **Critical - null dereference.** `WardNotices::refresh` calls `setToolTip` and
   `layout()` on `makeNotice`'s result for the access-change notice, but `makeNotice`
   returns nullptr for a dismissed kind. Reachable: dismiss the access notice in ward B,
   open ward A and sync a change, open B again. `DocumentManager::install` emits
   `documentChanged` (a refresh) before `documentOpened` (which clears the events), so
   A's events are still held under B's dismissed key. The same ordering shows the
   previous ward's events under the new ward for one refresh.
2. **Important - access changes lost.** `onAccessChanged` replaces the list, and
   `onDocumentOpened` discards it, so a second sync, or switching wards, erases changes
   not yet read - exactly the hostile-admin case Task 18 exists for.
   **Fix for 1 and 2:** keep undismissed events per ward
   (`QHash<QString, QList<AccessEvents::Event>>` by document id), append on each
   `accessChanged`, clear on dismiss; take `AccessChanged` out of the dismissed set
   entirely. The null path and the clearing on `documentOpened` then go.
3. Important - the notices have no visual treatment (a plain `QWidget`, no stylesheet
   rule for `#wardNotice`, no `WA_StyledBackground`). The plan says to follow
   `EmergencyBanner`, a coloured `QFrame`.
4. Minor - every edit rebuilds the whole column (`documentChanged` -> `refresh`), which
   destroys a button under the mouse. Rebuild only when the set of notices differs.
5. Minor - the RequiresUpdate notice can be dismissed; the plan calls it only
   persistent. Decide which.
6. Minor - "Add Someone..." opens the access dialog, whose buttons read Share... or
   Invite Someone... and are not yet wired (Tasks 20, 23).
7. Minor - the access list is shown twice (tooltip and label); drop the tooltip.
8. Minor - the dismissed set encodes `documentId + "/" + kind` as a string; a
   `QHash<QString, QSet<Kind>>` says it directly.
9. Minor - `refresh()` need not be public; `linkRequested` has no comment; the class
   comment omits the copy banner between it and the emergency banner; `std::bind` is
   used without `<functional>`.
10. Minor - Task 19's **Files:** line in the plan omits `WardNotices.h/.cpp` and
    `CMakeLists.txt`.

## What is left

| Task | Size | Notes |
|---|---|---|
| 20 Share | medium | `DocumentSession::share` exists. `AccessDialog::shareRequested` is unconnected. The `IdTaken` re-id path is new work in `DocumentLibrary`/`DocumentManager`. |
| 21 Request access | medium | `ServerConnection` lacks `requestAccess`, `myRequestStatus`, `clearAccessRequest`, `accessRequests` (the `ServerApi` and `HttpServerApi` have them). Replaces the `RequestAccess` message in `MainWindow::openPath`. On grant, fetch through `DocumentLibrary::fetchWard`. |
| 22 Grant access | medium | Grant enqueues a `Grant` on `RosterChangeQueue`. |
| 23 Linking and in-person invite | large | `ServerConnection` lacks the four pairing calls. Replaces the `LinkDevice` message in `MainWindow::openPath`, the **Yes** message in `FirstRunDialog::onLinkDevice`, and `WardNotices::linkRequested`, which opens the access dialog for now. `AccessDialog::linkRequested`, `inviteRequested`, and `unlinkRequested` are unconnected. `Pairing` (Task 1.11) is the core. |
| 24 Remove, roles, leave | large | `AccessDialog::removeRequested`, `roleRequested`, `leaveRequested` are unconnected. Each is a `RosterChangeQueue` enqueue; Leave hides the ward at once and needs a successor picker. |
| 25 Failure messages | small | `AppSettings::writeFailed()` exists for its settings case. |
| 26 Developer export | small | Writes `Document::toJson` (JsonService is gone); the toggle is `AppSettings::developerExport`. |
| 27 Update notice | small | Should run when `DocumentManager::requiresUpdateChanged` fires (Task 19 note). |
| 28 Documentation | small | `docs/ARCHITECTURE.md` was partly updated in Task 14 for DocumentManager and CommandHistory; the rest is Task 28's. |

Then the plan's **Verification before calling it done** section.

My estimate, a guess from the plan text: Tasks 20-24 are about as much work as Tasks
14-19 were.

## Things a new session would not find on its own

- **The wire format** the companion server must follow is defined in
  `src/services/HttpWire.h` (routes, bodies, headers, error names and statuses). The
  design left it to the plan; Task 13 set it.
- **`DocumentSession` API changes after Task 6's core review:** `resolve` returns
  `bool` with a reason; `pendingConflict()`; `download`'s callback takes the downloaded
  document (`Downloaded`) and has a `WriteFailed` outcome; `versionAdopted` carries the
  base copy's roster and fires once per version taken in, including at a conflict;
  `setEmergencyActive` ignores an unchanged value; `setFault` re-announces a new reason.
- **`Document::parse` builds the lookup caches** (`familyIdForPerson` and the
  decoration caches); before Task 14's fix, a read document answered empty until a
  command ran.
- **Dialogs opened from session signals** are connected `Qt::QueuedConnection` and read
  current state when they run (see `MainWindow::askAboutConflict`); a modal opened
  inside a session's signal re-enters the session.
- **Untested paths recorded in plan notes:** the `RequiresUpdate` refusal message, a
  geocoding batch starting when the lease is taken (it would send a real request), and
  `DocumentManager::openDocument` through to a fetch.
- **Server ids name local files:** `DocumentLibrary` and `HttpWire` accept only canonical
  document ids from the server. Keep that for any new server-supplied id.
