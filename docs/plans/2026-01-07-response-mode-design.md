# Response Mode (Welfare Check) Design

## Overview

Response Mode is a separate application mode activated during emergencies. The UI shifts from preparation concerns (data completeness, assignment planning) to crisis coordination (who's been reached, who needs help).

This design covers the Welfare Check view. Needs & Assignments and Resource Lookup views will be designed separately.

---

## Mode Structure

### Mode Toggle

A prominent control in MainWindow switches between Preparation and Response modes. The toggle should provide clear visual distinction when in Response Mode (color change, banner, or similar).

### View Structure in Response Mode

```
[Preparation / Response toggle]

─── Response Mode ───
├── Welfare Check (toggle: Families | Ministering)
├── Needs & Assignments (future)
└── Resource Lookup (future)
```

Welfare Check has two sub-views toggled within it:
- **Families View** - flat list of all families with status
- **Ministering View** - hierarchical by district/companionship with progress bars

Both views share the same underlying family status data.

---

## Data Architecture

### Data Separation

- **Preparation data** (families, ministering, teams) - persists in the main document
- **Response data** (contact statuses, attempts, needs) - stored separately, keyed by family ID
- **Archives** - each completed emergency saves as a separate file containing full response data

Response data is ephemeral and separate from the main document. This keeps emergency tracking cleanly isolated.

### Response Data Per Family

Each family entry in response data includes:
- Family ID (primary key for matching)
- Family display name (for fallback matching and display)
- Family address (for context and matching)
- Contact status
- List of contact attempts
- List of needs

This redundant family info ensures archives remain useful even if preparation data changes.

---

## Status Model

### Family Contact Status

| Status | Storage | Description |
|--------|---------|-------------|
| Not contacted | Default | Starting state for all families |
| OK | Explicit | Family has been contacted and is fine |
| Needs help | Derived | Family has one or more unresolved needs |
| Unable to reach | Explicit | Contact attempted but couldn't get through |

**Key design decision:** "Needs Help" is derived from having unresolved need entries, not stored as a separate status. This prevents data inconsistency.

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

## Need Model

### Need Entry

```
NeedEntry {
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

### Need Categories

Configured in Settings during Preparation Mode:
- Default list provided: Tree removal, Generator, Medical, Transport, Shelter, Other
- Ward leader can add/remove/reorder
- Can also add new categories on the fly during emergency

### Need Resolution

Three valid resolution paths:
1. **Quick resolve** - Toggle resolved, no notes
2. **Resolve with notes** - Add resolution notes describing what was done
3. **Full workflow** - Assigned team/person marks complete from Needs & Assignments view

When all needs for a family are resolved, status automatically returns to "OK".

---

## Welfare Check - Families View

### Layout

Same pattern as existing Families view: sidebar list on left, map on right.

### Progress Summary

Displayed at top of sidebar:

```
January 2026 Ice Storm
47 families: 23 OK • 3 need help • 2 unable to reach • 19 remaining
[====green====][orange][yellow][----gray----]
```

Segmented progress bar showing all statuses at a glance.

### Filter Tabs

```
[All (47)] [Remaining (19)] [Needs Help (3)] [OK (23)] [Unable to Reach (2)]
```

These status tabs are most frequently used. Additionally, preparation mode filters (search, teams, tags, skills) remain available for combined filtering like "Remaining families with generators".

Filter hierarchy:
- Status tabs - prominent, most used
- Search field - always visible
- Additional filters (teams, tags, skills) - secondary, collapsible

Filtering applies to list; map dims filtered-out families rather than hiding them.

### Family Row

- Status icon (color + icon for accessibility)
- Family display name
- Address
- Phone number (prominent for quick calling)
- Action buttons: [OK] [Add Need] [Unable to Reach]
- Expandable section showing:
  - Family members
  - Contact attempt history
  - Active needs with actions

### Map

- Pins colored by status with icon overlay:
  - Gray circle: Not contacted
  - Green checkmark: OK
  - Orange flag: Needs help
  - Yellow question mark: Unable to reach
- Clicking pin selects family in list
- Filtered families dimmed but visible (not hidden)

---

## Welfare Check - Ministering View

### Layout

Same sidebar + map pattern. Sidebar shows hierarchical list.

### Toggle

EQ / RS toggle at top, same pattern as Preparation Mode ministering.

**RS handling:** RS assignments are to individual sisters, but for welfare check purposes, status is tracked at the family level. The RS view shows sisters grouped by companionship, but clicking a ministered sister highlights/shows her family's status.

### Hierarchy Display

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

- District name and leader
- Progress bar (segmented by status)
- Leader phone number for quick contact

### Per Companionship

- Minister names
- Status icons for assigned families (compact row)
- Minister phone numbers
- If a minister's own family has status, show their icon too

### Selection and Highlighting

- Click district → highlights presidency member's family + all families in that district
- Click companionship → highlights ministers' families + ministered families
- Click individual family → highlights just that family
- Map shows highlighted families prominently, others dimmed

---

## Needs Workflow

### Creating a Need

From family row, click [Add Need] → dialog opens:
- Category dropdown (from configured list, plus "Add new...")
- Description text field
- Assignment (optional): team or person picker
- [Save] [Cancel]

Family immediately shows "Needs Help" status.

### Viewing Needs

Expanded family row shows active needs:

```
Needs:
• Generator - "No power since Tuesday"
  Assigned to Chainsaw Crew ✓ notified
  [Edit] [Resolve]
• Medical - "Needs medication pickup"
  Assigned to Dr. Smith ○ not notified
  [Edit] [Notify] [Resolve]
```

### Assigning a Need

Click [Assign] → picker for team or person:
- Shows teams with their capabilities
- Shows individuals (searchable)
- Once assigned, shows assignee name and contact info

### Notifying Assignee

After assigning, need shows [Notify] button. Clicking opens quick picker for method (phone/text/email/visit/other), records timestamp automatically.

### Assignment Notes

Can add ongoing notes to track progress: "Started work, waiting for equipment", "50% cleared", etc.

---

## Emergency Lifecycle

### Starting an Emergency

1. Click mode toggle → "Start Emergency" confirmation dialog
2. Prompt for emergency name (e.g., "January 2026 Ice Storm")
3. If document has unsaved changes, prompt to save first
4. Confirm → clears any previous response data, switches to Response Mode
5. All families start as "Not contacted"

### During Emergency

- Response data auto-saves periodically (separate from main document)
- Auto-save preserves undo/redo stack
- Can switch back to Preparation Mode temporarily if needed (response data preserved)
- Undo/redo works for response actions
- Summary report viewable anytime for briefings or handoffs

### Ending an Emergency

```
End "January 2026 Ice Storm"?

☑ Generate summary report (PDF)

[Archive & End] [Discard & End] [Cancel]
```

**Archive & End:**
- Saves full response data to archive file
- Optionally generates summary report (PDF)
- Clears response data and undo/redo stack
- Returns to Preparation Mode

**Discard & End:**
- Confirms "This will permanently delete all response data"
- Clears response data and undo/redo stack
- Returns to Preparation Mode

---

## Emergency Archives

### Accessing Archives

Menu action: File → Open Emergency Archive

### Archive Browser

Shows list of past emergencies:

```
January 2026 Ice Storm - Jan 4-6, 2026 - 47 families, 12 needs
December 2025 Power Outage - Dec 18, 2025 - 47 families, 3 needs
Training Exercise - Nov 15, 2025 - 47 families, 0 needs
```

### Viewing an Archive (Read-Only)

- Opens in Response Mode UI but clearly marked "ARCHIVED - READ ONLY"
- Full Welfare Check view with all statuses, attempts, needs
- Families matched to current prep data by ID, fallback to display name matching
- Families no longer in ward shown grayed with stored name/address
- Cannot edit - view and report only

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

**Needs Summary:**
- Total needs created
- By category (Generator: 5, Medical: 3, etc.)
- Resolved vs unresolved
- Assigned vs unassigned

**Response Activity:**
- Total contact attempts logged
- Attempts by method (phone, visit, etc.)
- Needs assigned to teams/individuals
- Resolution rate

**Unresolved Items (if any):**
- List of families still needing help
- List of families not contacted

**Format:** PDF, printable

---

## UI Consistency and Integration

### Consistent Patterns

- Sidebar + map layout (matches Families view, Ministering view)
- Map dimming for filtered items (matches ministering view design)
- EQ/RS toggle in ministering view (matches Preparation Mode)
- Search field always visible (matches all views)
- Family row expansion pattern (matches existing)

### Map Widget Extensions

- Status-colored pins with icon overlays (new)
- Same highlighting behavior for selection
- Same zoom/pan persistence across views

### Shared Components

- Person/team picker (reusable for assignment and "who attempted")
- Filter chips (prep mode filters available in response mode)
- Progress bar widget (new, reusable)

### Settings Additions

- Need categories configuration (in Preparation Mode settings)
- Default categories shipped with app

### Undo/Redo

- Response actions are undoable throughout the emergency
- Separate undo stack from preparation document
- Auto-save preserves undo/redo stack
- Archive & End clears undo/redo stack

---

## Implementation Notes

### Phased Implementation

1. **Phase 1:** Core welfare check - mode toggle, families view, status tracking, basic needs
2. **Phase 2:** Ministering view with progress tracking
3. **Phase 3:** Emergency lifecycle (start/end/archive)
4. **Phase 4:** Summary reports
5. **Phase 5:** Needs & Assignments view (future design)
6. **Phase 6:** Resource Lookup view (future design)

### Data Model Changes

- New: EmergencyResponse model (status, attempts, needs per family)
- New: NeedCategory model (configurable list)
- New: EmergencyArchive model (metadata + full response data)
- Settings: Add need categories configuration

### File Format

- Response data: JSON, separate from main document
- Archives: JSON files in designated archive directory
- Reports: PDF generated on demand
