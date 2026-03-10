# Response Mode Design Review

Review of [2026-01-07-response-mode-design.md](../plans/2026-01-07-response-mode-design.md).

## Strengths

**Core architectural decision is sound.** Augmenting existing views rather than adding a separate mode is the right call. It reduces cognitive load for leaders during high-stress situations — they're using familiar views with additional context, not learning a new interface.

**"Needs Help" as derived state.** Deriving it from unresolved tasks rather than storing it as a separate status is a good data integrity choice. It eliminates the class of bugs where status and tasks disagree.

**Data separation.** Keeping response data separate from the preparation document is clean. It means emergencies don't pollute the persistent ward data, and archives are self-contained.

**Redundant family info in response data.** Storing display name and address alongside family ID in response records ensures archives remain meaningful even after families move out of the ward. Practical decision.

## Issues and Questions

### 1. ~~Undo/Redo Separation~~ (Resolved)

**Resolution:** No undo/redo for response data. Response actions are event logs (contact attempts, status changes, tasks), not document edits. Mistakes are corrected by deleting the wrong entry or changing the status back. The preparation document's undo stack is unaffected by emergency lifecycle. Design doc updated.

### 2. ~~Task Assignee Model~~ (Resolved)

**Resolution:** Two optional fields: `std::optional<TeamId> assignedTeamId` + `std::optional<PersonId> assignedPersonId`, at most one populated. Uses strong ID types (`StrongId<Tag>` template) — a prerequisite refactor to convert existing `QString` IDs to `FamilyId`, `PersonId`, `TeamId`, etc. will be designed and planned separately before response mode implementation begins. Design doc updated.

### 3. ~~"Who" Field on Contact Attempts~~ (Resolved)

**Resolution:** Limit `who` to a `PersonId` — only ward members can be recorded as making contact attempts. External helpers don't need to be tracked by the app; if a leader wants to note that a neighbor checked on a family, they can put that in the notes field. This keeps the data model clean and avoids the dual-mode string problem. Design doc updated.

### 4. ~~Auto-Save for Response Data~~ (Resolved)

**Resolution:** Auto-save for everything — both preparation and response data. This eliminates the asymmetry entirely. Every action is a discrete, intentional operation, so there's no need for manual save-as-commit. The undo/redo stack is preserved across saves (current behavior of clearing on save will need to change). Undo stack clears on app close, which is the real session boundary. Design doc updated.

### 5. ~~Notification Model~~ (Resolved)

**Resolution:** Keep as a single notification record. The `assignmentNotes` field handles the narrative of re-notification attempts ("Called 2pm no answer, texted 4pm"). The single record answers the key UI question — "has the assignee been told?" — and if repeated failures occur, reassigning the task is the better action. No design doc change needed.

### 6. ~~Archive File Management~~ (Resolved)

**Resolution:** Derive the archive directory from the main document path — if the document is `MyWard.json`, archives go in `MyWard_archives/`. This is portable (archives travel with the document), zero-configuration, and discoverable. No settings UI needed; a configurable path could be added later if requested. Design doc updated.

### 7. ~~Emergency Banner and Sidebar Layout~~ (Resolved)

**Resolution:** Not an issue — the app already has two rows of tabs (Families/Ministering/Teams/Needs + Medical/Communications/Skills & Gear). The design mockup was reflecting the existing layout, not proposing a change. No design doc change needed.

### 8. ~~Map Marker Conflict~~ (Resolved)

**Resolution:** Keep existing `ResponseArea` marker colors and add a small badge (bottom-right corner) for welfare check status. No badge = not contacted; badges only render for families whose status has changed. Design doc updated.

## Minor Notes

- The filter tabs show counts. These need to update in real time as statuses change, which means the filter model needs to react to response data changes. Not difficult with signals, but worth noting.
- "Add new..." category during an emergency — does this persist to settings, or is it emergency-scoped? If a leader adds "Snow Removal" during a snowstorm, should it be available for the next emergency too?
- The archive matching fallback to "display name matching" is fragile. Family names can change (marriage, divorce). ID matching should be primary, with display name as a human-readable label rather than a matching key.

## Summary

The design is well-structured and the core decisions (augment-not-replace, derived status, data separation) are strong. All issues have been resolved.
