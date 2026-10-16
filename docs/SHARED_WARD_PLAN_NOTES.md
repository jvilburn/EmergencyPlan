# Shared Ward: Notes for the Implementation Plan

Code sites and what the code does today, moved out of `SHARED_WARD_DESIGN.md` so the design
says only what the system does. These notes are input to the implementation plan. They are
not part of the design and are not reviewed with it; the plan supersedes them, and this file
is deleted once the plan is written.

## Current-code notes

- `fromJson` on the models has no way to fail; each reads a missing field as empty or false.
- `vcpkg.json` lists only `libmupdf`. libsodium joins it with `"version>=": "1.0.22"` and a
  `builtin-baseline` (the baseline is what fixes the version), under the
  `x64-windows-static-md` triplet that MuPDF uses. For a static libsodium on MSVC,
  `SODIUM_STATIC` must be defined, which vcpkg's `unofficial-sodium::sodium` target supplies
  through `find_package(unofficial-sodium CONFIG REQUIRED)` (recalled by a reviewer, not
  verified). `tests/spec` adds `nlohmann-json`, `zlib` and `libsodium`.
- `ImportWardDirectoryCommand` and `ImportMinisteringCommand` read their previous state inside
  `execute`; the directory import's `undo` restores none of the cleanup. `SetFamiliesCommand`
  has no callers.
- `UpdateTeamCommand`, `UpdateEmergencyAssetCommand` and `UpdateTagCommand` hold whole objects.
- `Document::cleanupPersonReferences` has no callers; it can become the person half of the
  cleanup routine. It touches tags only when `isPersonLevel()`, and no family-level cleanup;
  `ImportWardDirectoryCommand::cleanupRemovedFamily` also removes the family id from tags and
  groups.
- `MainWindow::generateEmergencyReportWithConfirm` holds `const EmergencyResponse&` across its
  save dialog (`MainWindow.cpp:1012`), safe only while `EmergencyManager` keeps its own copy.
- `MinisteringImportService::mergeFamilies` points groups at the id of an unmatched,
  unadded family or person when not authoritative (`MinisteringImportService.cpp:156-164`,
  `:330-334`).
- `WardDirectoryImportService::preserveIds` drops the special-need note and takes the PDF's
  coordinates, so every re-import wipes special needs; it should copy the note and keep the
  coordinates when the address is unchanged.
- Directory import matching: `findMatchingFamily` (surname + display name) and
  `findMatchingPerson` (first name). `PersonMatching` (`findFamilyByMembers`,
  `findReplacedFamily`, `findBestPersonMatch`) is used only by the ministering import and is
  not one-to-one: two PDF families can claim one existing family, and the second overwrites
  the first under the same id (`WardDirectoryImportService.cpp:49-63`). `IMPORT_DESIGN.md`
  describes `PersonMatching` for the directory import.
- The directory's date is `QFileInfo::lastModified()` (`WardDirectoryImportService.cpp:41-43`).
- Geocoding: `startBatchGeocoding` and `checkForIncompleteWards`; `onGeocodingFinished` saves
  once per batch. `BackgroundGeocodingService::m_familyAddresses` and its change detection
  go; its `documentChanged` hook is replaced by the explicit triggers.
  `m_addressToFamilyId` holds one family per address (`BackgroundGeocodingService.h:74`), so
  two families at one address leave the batch unfinished (`.cpp:67-71`).
- A failed ward lookup stops the one-at-a-time chain: `onLookupFailed` does not continue to
  the next ward (`DocumentManager.cpp:211-225`, `292-298`).
- The undo stack holds `CommandPtr`s; `CommandHistory` has no cap.
- A ward's name today is `DocumentMetadata::suggestedFilename`, which also names stakes and
  multi-ward sets; the design names wards only, in alphabetical order. Its strings are not
  passed through `tr()`, and the two-ward case takes its order from QHash iteration.
- `installer.iss` has no `[UninstallDelete]`.
- `tests/CMakeLists.txt` lists no sources.
- `DocumentManager` and `EmergencyManager` assert a single instance
  (`DocumentManager.cpp:36`, `EmergencyManager.cpp:17`); `BackgroundGeocodingService` binds to
  `DocumentManager::instance()`. The singletons become thin wrappers over the one session.
- The models read the system clock in `TaskNotification::create`, `ContactAttempt::create`,
  `ResponseTask::create`, `ResponseTask::resolve` and `EmergencyResponse::create`; each takes
  the time as an argument.
- `ReportGenerator::generateReport` takes only the response (`ReportGenerator.cpp:149`) and
  counts through `EmergencyResponse::totalFamilies` / `countByStatus` (lines 203-207), which
  see only the records; its attempt tally indexes `attemptsByMethod[attempt.method()]`
  (line 259).
- `ContactStatus` and `m_contactStatus` go, and `effectiveStatus()` reads the attempts.
  `addContactAttempt` and `addTask` use `prepend`; the fromJson comments at
  `EmergencyResponse.cpp:466,476` describe most-recent-first order.
  `removeContactAttempt` has no interface caller.
- `assignToTeam` and `assignToPerson` keep the previous assignee's notification.
- Entry points the old file model has that go: command-line open of `.emergencyplan`
  (`main.cpp:36-41`); `MainWindow::openFile` (`MainWindow.cpp:306-320`);
  `DocumentManager::autoSave`, `autoSaveFailed` and `MainWindow::onAutoSaveFailed`
  (`DocumentManager.cpp:374-390`, `MainWindow.cpp:271,592`); the `lastDocumentPath` setting
  (`MainWindow.cpp:54-69`, `657-668`), which becomes the last ward's id and today clears
  silently on a failed restore.
- Old names to rename beyond the work list below: `README.md`, `docs/BUILDING.md`,
  `docs/DESIGN_GUIDE.md`, and `packaging/macos/Info.plist`.

## The work list as it stood in the design

**Making every write a command is the core of this work.** What runs through
`CommandHistory` today is family update; team add, update, delete and member removal;
emergency-asset add, update, delete and unassign; and the two imports. Ministering changes
only by import. The tag commands exist and convert with the rest, though nothing in the
interface calls them yet: `AddTagCommand`, `UpdateTagCommand` and `DeleteTagCommand` stay,
while `AssignTagToPersonCommand`, `AssignTagToFamilyCommand`,
`UnassignTagFromPersonCommand` and `UnassignTagFromFamilyCommand` are deleted, since each is
an `UpdateTagCommand` holding one id added or removed. `AddFamilyCommand`, `AddTeamMemberCommand` and
`AssignEmergencyAssetToPersonCommand` have no callers - families arrive by import, and the
team and asset dialogs change members through their update commands - so they are deleted.
`RemoveTeamMemberCommand` and `UnassignEmergencyAssetFromPersonCommand` are deleted too:
under the update shape above, removing one member is an `UpdateTeamCommand` or
`UpdateEmergencyAssetCommand` holding that one id removed, and, for a team leader, the
leader change as well, both fixed at construction. `RemoveTeamMemberCommand` today clears
the leader inside `execute` and does not restore it on undo.
The Delete Family action, whose confirmation ends in "Delete not yet implemented" today, is
wired to `DeleteFamilyCommand`, which carries the same cleanup as an import's removal,
worked out at construction. The rest has to join them, because a write that is not a command cannot
replay:

- **Emergency responses become a keyed collection.** `Document` holds one
  `std::optional<EmergencyResponse>` today, and `EmergencyManager` assumes at most one:
  `startEmergency` returns early when one is active, and `isActive()` gates the response
  views throughout the interface. Each response gains an id, the document holds them by it,
  and the interface lists the emergencies in progress and switches between them. Ending
  saves the summary report and then removes the response, as `endEmergency` clears it
  today (see *Emergency archives are removed*).
- **Contact status becomes the outcome of an attempt.** `FamilyResponseRecord` stores one
  `m_contactStatus` today, overwritten by the OK and Unable to Reach buttons. Instead,
  `ContactAttempt` gains a `std::optional<ContactOutcome>` (`OK` or `UnableToReach`), the two
  buttons each record an attempt carrying their outcome, and a family's status is the outcome
  of its latest attempt that has one, or Not Contacted when none does. Latest means the
  greatest `timestamp`, ties broken by `ContactAttemptId`. A command inserts an attempt at
  its place in that order, most recent first, as `CODING_STYLE.md` asks of embedded child
  objects, and a task likewise by `createdAt`, ties broken by `TaskId`; `prepend` does this
  today only because attempts arrive in time order on one device. So the stored order is the
  displayed order whatever order replay lands them in, the same attempts always make an
  equal list, and a family's status is its first attempt that has an outcome. Timestamps
  come from each device's clock, so a wrong clock can show an older outcome as current; every
  attempt stays in the history, and this is accepted. `method` and `who`
  become optional: a button press records only its outcome and time, shown as *Marked OK*
  or *Marked Unable to Reach*, while the contact dialog fills in both. `ContactStatus` and
  `m_contactStatus` are deleted, and `effectiveStatus()` reads the attempts. Needs Help
  still comes first while a family has unresolved tasks, whatever its latest outcome.
- **`EmergencyManager` keeps no copy of its own.** It holds `m_response` and writes it back
  through `persistResponseData`; instead it reads the response from the document, by the id
  of the emergency being shown, and every change goes through a command. Starting an
  emergency is one command that adds the response with a `FamilyResponseRecord` for every
  family, as `startEmergency` does today, keeping names and addresses as they stood at the
  start. `syncFamilies`, which adds records for new families on each document change, is
  deleted: a family added after the start has no record and reads as Not Contacted, and a
  command that writes to a family's record creates it when missing, from the name and address snapshot the command took at
  construction, so it depends on nothing but the response id. Two responders
  touching a new family both land, since each change is an addition. Statistics - the
  progress bar and the report - count the response's records plus every directory family
  without a record as Not Contacted. Both count through the same two functions,
  `EmergencyResponse::totalFamilies` and `countByStatus`, which today see only the records
  and take the directory's families instead. `ReportGenerator::generateReport` takes only
  the response today (`ReportGenerator.cpp:149`); it also takes the directory's families,
  and draws its overview from those functions, so the report and the progress bar agree
  when the emergency ends. Its attempt tally indexes by `method` (line 259), which becomes
  optional: an attempt with no method is a status mark, tallied on its own line as
  *Marked OK* or *Marked Unable to Reach*.
- **Everything that lets the user choose where a ward is stored is deleted**; the PDF import
  and report export dialogs stay. Known sites:
  - `DocumentManager`: the `filePath` property and `filePathChanged`, `setFilePath`,
    `openDocument(path)`, `saveDocument`, `saveDocumentAs`, `saveDocumentOnly`,
    `ensureFilePath`, `maybeRenameForWard`, and `setEmergencyResponse`, whose work the
    emergency commands take over
  - `JsonService`'s plaintext `loadDocument` and `saveDocument`; the payload's JSON reuses
    the models' `toJson` and `fromJson`
  - `MainWindow`'s File > Open and Save with their file dialogs, and the "use Save As to
    save manually" hint on a failed save
- **All archive code is deleted** with the archives themselves; the summary report saved
  when an emergency ends is how it is looked back on:
  - `EmergencyManager`: `m_savedResponse`, `loadArchive`, `closeArchive`, `reopenArchive`,
    `saveArchive`, `isViewingArchive`, the `archiveViewOpened` and `archiveViewClosed`
    signals, and `endEmergency`'s `archive` parameter
  - `DocumentManager`'s overlay: `m_archiveDocument`, `setArchiveDocument`,
    `clearArchiveDocument` and `isViewingArchive`
  - `ArchiveBrowserDialog` and `EmergencyBanner::setArchiveName`
  - `MainWindow`'s `onOpenArchive`, `onArchiveViewOpened`, `onArchiveViewClosed`,
    `m_openArchiveAction`, `m_closeArchiveAction`, the Open and Close Archive menu items,
    and the reopen-from-archive path
  - the end dialog's Archive && End and Discard && End buttons and its *Generate summary
    report* checkbox, replaced by one End confirmation that always saves the report first;
    cancelling the report's save dialog ends nothing, in place of today's *Skip Report*
    question
  - the `archiveViewOpened`/`archiveViewClosed` connections in `FamilyTreeModel`,
    `MinisteringModel`, `TeamsTreeModel` and `WardListView`, and the `isViewingArchive`
    checks in `ActionButtonsWidget`, `TeamsView` and `WardListView`
  - `EmergencyResponse::m_endedAt` and its accessors, since no response in the document
    has ended; `ReportGenerator` takes the end time as an argument
- **The emergency response** is written by `EmergencyManager` mutating it in place, outside
  the command path. Its mutations become these commands:

  | Command | Covers | On replay |
  |---|---|---|
  | `StartEmergency` | start, with a record per family | an addition; never conflicts |
  | `EndEmergency` | end, removing the response once its report is saved | conflicts if the response differs from what the report holds; result holds if already gone |
  | `RecordContactAttempt` | logging a contact, and the OK and Unable to Reach buttons | an addition |
  | `DeleteContactAttempt` | the inverse of `RecordContactAttempt`, which is how an attempt is undone; no button removes one today | as last seen; result holds if already gone |
  | `AddTask` | adding a task | an addition |
  | `UpdateTask` | the task dialog, resolve, reopen, assign, unassign, notify | per value, as `UpdateFamilyCommand` |
  | `DeleteTask` | removing a task | as last seen; result holds if already gone |
  | `AddTaskCategory` | adding a category | result holds if present |

  `UpdateTask` holds typed optional changes, each with its before and after, to four
  values: category, description, assignment (assignee, notes and notification as one
  value), and resolution (resolved, notes and time as one value, the time fixed at
  construction). A change of assignee sets the notification to none, since the new
  assignee has not been told; `assignToTeam` and `assignToPerson` keep the previous
  assignee's notification today.
- **The access list** is new in its entirety. Invite, remove a user, remove a device,
  withdraw an invite, set a role and leave are each a command, which is what lets an access
  change replay like any other. Join is a command too, run once by the joining device under
  the invite's key, and refused with *This invite has been used or withdrawn* when the
  current header no longer names the invite.
- **Every model `fromJson` gains a way to fail**, so a missing or malformed required field
  refuses the whole payload; today each reads a missing field as empty or false.
- **`Command` itself** gains `toJson` and `fromJson` with a type registry, so a journal
  entry reaches disk and comes back as the command it was. `undo` becomes `inverse` on
  every command, and `CommandHistory`'s `markSaved` and `isDirty`, with
  `DocumentManager::isDirty` and its `dirtyChanged` signal, are deleted: every command is
  saved as it is made, so nothing is ever unsaved.
- **The update check** is new: on start, the app reads the server's `GET /releases/latest`
  and, when it is newer than the running version, shows a dismissible notice offering the
  download. Installers are published to the server, which serves them and the download
  page. The running app knows no version today: `main.cpp` sets no
  `setApplicationVersion`; `CMakeLists.txt`'s `project` declares `1.0.0`, which reaches
  only the macOS bundle (`CMakeLists.txt:526-527`); `installer.iss` says `1.0.1`; and
  `vcpkg.json` says `1.0.0`. `project`'s version becomes the one source: it reaches the app
  as its application version and is written at configure time into a file `installer.iss`
  includes in place of its own `MyAppVersion`, and `vcpkg.json` drops its `version`, which
  a top-level manifest does not need. Versions compare numerically, part by part, so 1.10.0
  is newer than 1.9.0.
- **The app is renamed Ward Planning throughout.** The application name (`main.cpp:12`),
  the window title (`MainWindow.cpp:530`), the installer's name and publisher
  (`installer.iss`), the executable, and the CMake project and its targets (`EmergencyPlan`,
  `EmergencyPlanLib`) take the new name, with everything that names them, such as
  `build.bat`'s deploy step, the `name` in `vcpkg.json`, and the macOS bundle's name and
  identifier (`CMakeLists.txt:525,528`, and `packaging/macos/Info.plist`, whose
  `.emergencyplan` document type goes as the Windows association does). The installer keeps its `AppId`, so the release
  upgrades the old install in place. App data and the tile cache are named by Qt after the application,
  so both move; nothing is carried over from the old directories (see *Earlier versions*),
  and the tile cache refills as maps are viewed.
- **Receiving an invitation** is new: *Join a ward*, which reads the clipboard when clicked
  and otherwise offers a paste box, and the Windows instance lock. `installer.iss` registers
  `.emergencyplan` today, and that association is removed; nothing is registered in its
  place.
- **`docs/FORMAT.md` is new**, written with the code since the models change in this work.
  It specifies every field of the payload, `device` included, and changes in the same
  commit as any `toJson` it describes; a release that changes it raises the format version,
  as under *File format*.
- **`ARCHITECTURE.md`, `IMPORT_DESIGN.md` and `USER_GUIDE.md` change with the code they
  describe.**
  `ARCHITECTURE.md` gives commands `execute` and `undo`, `CommandHistory`'s stacks and an
  `AddFamilyCommand` example; it moves to `execute`, `inverse` and `refusal`, the journal
  and `WardSession`. `IMPORT_DESIGN.md` says a re-import replaces family and person data;
  it states that the special-need note and the coordinates are the app's own and stay, and
  that a ministering import makes a placeholder for someone it cannot match only when it is
  the newer source (see *Imports*).
  `USER_GUIDE.md` describes `.emergencyplan` files, File > Open, Save As and archives; it is
  rewritten for wards, *Who has access* and invites, in the words under *Words the user
  sees*, and carries the statements this design promises it makes: what the server knows,
  and that roles are a convention among honest clients, not a security measure.
- **Edits that re-read on confirm** take their start condition when their target is chosen,
  as above. The sites known today are a starting list, not a complete one: the family edit
  panel (`MainWindow.cpp` builds `UpdateFamilyCommand` from the live document, while the
  panel already holds the copy it opened with); the team dialog in `TeamsView.cpp`; the
  asset dialog in `EmergencyAssetView.cpp`; the special-needs dialogs in `NeedsSubView.cpp`,
  which can switch person; and the notify, resolve and assign-to-team actions in
  `WardListView.cpp`, `TaskListView.cpp` and `TeamsView.cpp`.
- **The special-needs dialog becomes one "Special Need" dialog.** Opened on an existing
  need, its person is locked; opened from empty space, picking a person fills in that
  person's current note, and that is when its start condition is taken. Today it reuses
  `WardListDialog::selectPersonWithName`, which sets the note once at open and never on a
  change of selection (`WardListDialog.cpp:292-317`), so it either gains a
  selection-changed signal from the picker or stops reusing it.

