# Emergency Response Design

## Overview

Emergency response is not a separate mode — it augments the existing views. When an emergency is active, the Families, Ministering, and Teams views gain response-specific UI elements (status tracking, tasks, progress). When the emergency ends, those elements disappear and the views return to normal.

No new tabs. No mode toggle. The leader uses the same views they already know.

---

## Activating an Emergency

### Starting

Menu action: **File → Start Emergency**

1. Confirmation dialog prompts for emergency name (e.g., "January 2026 Ice Storm")
2. If document has unsaved changes, prompt to save first
3. Confirm → initializes response data, all families start as "Not contacted"

### Visual Indicator

When an emergency is active, a banner appears above the sidebar navigation buttons showing the emergency name. This is the only persistent visual change — it signals that response UI is active without disrupting the layout.

```
┌─────────────────────────────────┐
│ ⚠ January 2026 Ice Storm       │  ← banner (amber/warm tint)
├─────────────────────────────────┤
│ [Families] [Ministering] [Teams] [Needs]  │
│ [Medical]  [Comms]  [Recovery]            │
├─────────────────────────────────┤
│ (view content)                  │
```

### Ending

Menu action: **File → End Emergency**

```
End "January 2026 Ice Storm"?

☑ Generate summary report (PDF)

[Archive & End] [Discard & End] [Cancel]
```

**Archive & End:**
- Saves full response data to archive file
- Optionally generates summary report (PDF)
- Clears response data and undo/redo stack
- Response UI elements disappear from all views

**Discard & End:**
- Confirms "This will permanently delete all response data"
- Clears response data and undo/redo stack
- Response UI elements disappear from all views

---

## Data Architecture

### Data Separation

- **Preparation data** (families, ministering, teams) — persists in the main document
- **Response data** (contact statuses, attempts, tasks) — stored separately, keyed by family ID
- **Archives** — each completed emergency saves as a separate file

Response data is ephemeral and separate from the main document. This keeps emergency tracking cleanly isolated.

### Response Data Per Family

Each family entry in response data includes:
- Family ID (primary key for matching)
- Family display name (for fallback matching and display)
- Family address (for context and matching)
- Contact status
- List of contact attempts
- List of tasks

Redundant family info ensures archives remain useful even if preparation data changes.

---

## Status Model

### Family Contact Status

| Status | Storage | Description |
|--------|---------|-------------|
| Not contacted | Default | Starting state for all families |
| OK | Explicit | Family has been contacted and is fine |
| Needs help | Derived | Family has one or more unresolved tasks |
| Unable to reach | Explicit | Contact attempted but couldn't get through |

**Key design decision:** "Needs Help" is derived from having unresolved task entries, not stored as a separate status. This prevents data inconsistency.

### Contact Attempt

Attempts accumulate as a list per family:

```
ContactAttempt {
    method: enum (phone, text, email, visit, other)
    who: string (person ID or free text name)
    timestamp: datetime
    notes: string (optional, required if method is "other")
}
```

Method is selected via radio control for quick entry. "Who" is a combo box allowing selection from ward members or free text entry.

---

## Task Model

### Task Entry

```
TaskEntry {
    id: string
    category: string (from configured list)
    description: string (can be updated anytime)
    createdAt: datetime

    // Assignment (optional)
    assignee: TeamId | PersonId | null
    assignmentNotes: string (progress updates, partial completion)
    notification: {
        method: enum (phone, text, email, visit, other)
        timestamp: datetime
        notes: string
    } | null

    // Resolution
    resolved: boolean
    resolutionNotes: string
    resolvedAt: datetime | null
}
```

### Task Categories

Configured in Settings:
- Default list provided: Tree removal, Generator, Medical, Transport, Shelter, Other
- Ward leader can add/remove/reorder
- Can also add new categories on the fly during emergency

### Task Resolution

Three valid resolution paths:
1. **Quick resolve** — Toggle resolved, no notes
2. **Resolve with notes** — Add resolution notes describing what was done
3. **Full workflow** — Assigned team/person marks complete from Teams view

When all tasks for a family are resolved, status automatically returns to "OK".

---

## Families View — Emergency Enhancements

When an emergency is active, the Families view gains:

### Progress Summary

Displayed at top of the family list:

```
47 families: 23 OK • 3 need help • 2 unable to reach • 19 remaining
[====green====][orange][yellow][----gray----]
```

Segmented progress bar showing all statuses at a glance.

### Filter Tabs

```
[All (47)] [Remaining (19)] [Needs Help (3)] [OK (23)] [Unable to Reach (2)]
```

These status filter tabs appear above the family list. Existing preparation filters (search, tags, skills) remain available for combined filtering like "Remaining families with generators".

Filtering applies to list; map dims filtered-out families rather than hiding them.

### Family Row Additions

Each family row gains:
- Status icon (color + icon for accessibility)
- Phone number (prominent for quick calling)
- Action buttons: [OK] [Add Task] [Unable to Reach]
- Expandable section additions:
  - Contact attempt history
  - Active tasks with actions

### Map Marker Changes

- Pins colored by welfare check status with icon overlay:
  - Gray circle: Not contacted
  - Green checkmark: OK
  - Orange flag: Needs help
  - Yellow question mark: Unable to reach
- Existing click-to-select and highlighting behavior unchanged

---

## Ministering View — Emergency Enhancements

When an emergency is active, the Ministering view gains welfare check progress tracking overlaid on the existing district/companionship hierarchy.

### Hierarchy Display with Status

```
District 1 - Brother Johnson              [====75%====]
├── Companionship: Smith & Jones          [✓✓✓○]
│   ├── ✓ Anderson Family
│   ├── ✓ Baker Family
│   ├── ✓ Clark Family
│   └── ○ Davis Family
└── Companionship: Brown & Wilson         [✓○!]
    ├── ✓ Evans Family
    ├── ○ Foster Family
    └── ! Green Family (needs help)
```

### Per District

- Progress bar (segmented by status) added next to district name
- Leader phone number shown for quick contact

### Per Companionship

- Status icons for assigned families (compact row)
- Minister phone numbers shown

### RS Handling

RS assignments are to individual sisters, but status is tracked at the family level. The RS view shows sisters grouped by companionship, but clicking a ministered sister highlights/shows her family's status.

### Selection and Highlighting

Same as existing behavior:
- Click district → highlights presidency member's family + all families in that district
- Click companionship → highlights ministers' families + ministered families
- Click individual family → highlights just that family

---

## Teams View — Emergency Enhancements

When an emergency is active, the Teams view gains a task list showing work assigned to each team.

### Task List Per Team

Each team shows its assigned tasks:

```
Chainsaw Crew (4 members)
├── Generator - Anderson Family - "No power since Tuesday" ✓ notified
├── Tree removal - Baker Family - "Tree on driveway" ○ not notified
└── Tree removal - Clark Family - "Blocking road" ✓ notified [RESOLVED]
```

### Task Assignment

Tasks are created from the Families view ([Add Task] button) and assigned to a team or individual. The Teams view shows the receiving end — what work each team has.

### Unassigned Tasks

A section at the top or bottom shows tasks not yet assigned to any team, allowing the leader to dispatch them.

---

## Needs View — No Changes

The Needs view (special needs: oxygen, mobility, dialysis) is unchanged during emergencies. These ongoing conditions remain relevant for context — a family with oxygen dependency is a higher priority for welfare check contact.

---

## Task Workflow

### Creating a Task

From family row in Families view, click [Add Task] → dialog opens:
- Category dropdown (from configured list, plus "Add new...")
- Description text field
- Assignment (optional): team or person picker
- [Save] [Cancel]

Family immediately shows "Needs Help" status.

### Viewing Tasks

Expanded family row shows active tasks:

```
Tasks:
• Generator - "No power since Tuesday"
  Assigned to Chainsaw Crew ✓ notified
  [Edit] [Resolve]
• Medical - "Needs medication pickup"
  Assigned to Dr. Smith ○ not notified
  [Edit] [Notify] [Resolve]
```

### Assigning a Task

Click [Assign] → picker for team or person:
- Shows teams with their capabilities
- Shows individuals (searchable)
- Once assigned, shows assignee name and contact info

### Notifying Assignee

After assigning, task shows [Notify] button. Clicking opens quick picker for method (phone/text/email/visit/other), records timestamp automatically.

### Assignment Notes

Can add ongoing notes to track progress: "Started work, waiting for equipment", "50% cleared", etc.

---

## Emergency Archives

### Accessing Archives

Menu action: File → Open Emergency Archive

### Archive Browser

Shows list of past emergencies:

```
January 2026 Ice Storm - Jan 4-6, 2026 - 47 families, 12 tasks
December 2025 Power Outage - Dec 18, 2025 - 47 families, 3 tasks
Training Exercise - Nov 15, 2025 - 47 families, 0 tasks
```

### Viewing an Archive (Read-Only)

- Opens emergency UI overlays but clearly marked "ARCHIVED - READ ONLY"
- Full welfare check data visible across Families, Ministering, Teams views
- Families matched to current prep data by ID, fallback to display name matching
- Families no longer in ward shown grayed with stored name/address
- Cannot edit — view and report only

### Reopening an Archive

- Action: "Reopen This Emergency"
- Confirms: "This will restore this emergency as active. Any current emergency will be ended."
- Useful for: emergency not actually over, training exercises

---

## Summary Report

### Report Content

**Header:**
- Emergency name and date range
- Ward name

**Overview Statistics:**
- Total families in ward
- Families contacted (OK + Needs Help + Unable to Reach)
- Families not contacted
- Contact rate percentage

**Status Breakdown:**
- OK: count and percentage
- Needs Help: count (with breakdown by category)
- Unable to Reach: count (with attempt summary)
- Not Contacted: count

**Task Summary:**
- Total tasks created
- By category (Generator: 5, Medical: 3, etc.)
- Resolved vs unresolved
- Assigned vs unassigned

**Response Activity:**
- Total contact attempts logged
- Attempts by method (phone, visit, etc.)
- Tasks assigned to teams/individuals
- Resolution rate

**Unresolved Items (if any):**
- List of families still needing help
- List of families not contacted

**Format:** PDF, printable

---

## During Emergency

- Response data auto-saves periodically (separate from main document)
- Auto-save preserves undo/redo stack
- Undo/redo works for response actions (separate stack from preparation document)
- Summary report viewable anytime for briefings or handoffs

---

## Settings Additions

- Task categories configuration
- Default categories shipped with app

---

## Implementation Notes

### Phased Implementation

1. **Phase 1:** Data model + emergency lifecycle (start/end/archive menu actions, response data storage, banner)
2. **Phase 2:** Families view enhancements (status tracking, filter tabs, progress bar, action buttons, tasks)
3. **Phase 3:** Ministering view enhancements (progress roll-up per district/companionship)
4. **Phase 4:** Teams view enhancements (task list per team, unassigned tasks)
5. **Phase 5:** Summary reports (PDF generation)
6. **Phase 6:** Archives (browse, view read-only, reopen)

### Data Model Changes

- New: EmergencyResponse model (status, attempts, tasks per family)
- New: TaskCategory model (configurable list)
- New: EmergencyArchive model (metadata + full response data)
- Settings: Add task categories configuration

### File Format

- Response data: JSON, separate from main document
- Archives: JSON files in designated archive directory
- Reports: PDF generated on demand
