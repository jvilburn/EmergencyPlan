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

### 3. "Who" Field on Contact Attempts

The contact attempt `who` field is described as "person ID or free text name." This dual-mode field is tricky — is the intent that any ward member can log an attempt, or that the leader records who made the call? If it's selecting from ward members, a person ID is fine. If it's free text for external helpers (neighbors, stake members), that changes the data model. The combo box description suggests both, but the storage format should clarify whether this is `std::optional<QString> personId` + `QString displayName`, or just a single `QString`.

### 4. Auto-Save for Response Data

The design says response data "auto-saves periodically." The main document doesn't auto-save — the user explicitly saves. Having different save semantics for response vs. preparation data could be confusing. On the other hand, losing response data during an emergency is much worse than losing a prep edit. This seems intentional and reasonable, but worth noting the asymmetry.

### 5. Notification Model

The task has a single `notification` field — one notification record. What if the assignee doesn't respond and you need to notify again? A single notification slot means overwriting the previous one. Should this be a list, like contact attempts?

### 6. Archive File Management

The design says archives are "JSON files in designated archive directory" but doesn't specify where that directory is or how it's discovered. Is it relative to the main document? A user-configured path? An app-data directory? This affects portability — if someone shares their ward document, do archives travel with it?

### 7. Emergency Banner and Sidebar Layout

The ASCII mockup shows the banner above sidebar navigation, with two rows of buttons. Currently the sidebar has a single column of buttons. The mockup implies adding a second row (Medical, Comms, Recovery) — are these the existing resource view tabs becoming sidebar-level navigation, or new entries? This seems like a layout change that should be explicit.

### 8. ~~Map Marker Conflict~~ (Resolved)

**Resolution:** Keep existing `ResponseArea` marker colors and add a small badge (bottom-right corner) for welfare check status. No badge = not contacted; badges only render for families whose status has changed. Design doc updated.

## Minor Notes

- The filter tabs show counts. These need to update in real time as statuses change, which means the filter model needs to react to response data changes. Not difficult with signals, but worth noting.
- "Add new..." category during an emergency — does this persist to settings, or is it emergency-scoped? If a leader adds "Snow Removal" during a snowstorm, should it be available for the next emergency too?
- The archive matching fallback to "display name matching" is fragile. Family names can change (marriage, divorce). ID matching should be primary, with display name as a human-readable label rather than a matching key.

## Summary

The design is well-structured and the core decisions (augment-not-replace, derived status, data separation) are strong. The main area needing clarification before implementation is the notification/re-notification model. Everything else is resolvable during Phase 1 implementation.
