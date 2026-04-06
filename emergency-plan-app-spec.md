# Emergency Plan App — UI/UX Specification

## Overview

A desktop application for ward leaders to manage emergency preparedness and coordinate response during emergencies. This is a **single-user, offline-first tool** where the leader serves as the sole operator, coordinating teams and tracking welfare checks.

---

## Core Data Model

### Entities

**Families**
- Name (head of household format: "Last, First & Spouse")
- Members (expandable list)
- Address
- Phone number(s)
- Location coordinates (for map)
- Skills (e.g., nurse, EMT, electrician)
- Resources (e.g., generator, chainsaw, truck/trailer)

**Ministering Assignments**
- Companionship (1-2 ministers)
- Assigned families
- Used for welfare check accountability

**Response Teams**
- Team name (e.g., "Chainsaw Crew", "Medical Team")
- Team lead + contact info
- Members
- Associated skills/equipment

**Skills & Resources** (inventory view across all families)
- Skill/resource type
- Which families have it
- Quantity where applicable

---

## Application Modes

The app operates in two distinct modes, toggled explicitly by the user.

### Preparation Mode (Default)

**Purpose:** Build and maintain complete, accurate data before an emergency.

**Pace:** Leisurely, updated over weeks/months.

**Key user questions:**
- Who's missing info?
- What skills do we have coverage gaps on?
- Are ministering assignments current?
- Which families aren't geocoded?

### Response Mode (Emergency Active)

**Purpose:** Rapid coordination during an active emergency.

**Pace:** Urgent, minutes matter.

**Key user questions:**
- Who haven't we reached?
- Who needs help right now?
- Where's our nearest chainsaw?
- What's assigned to each team?

---

## Mode Toggle Behavior

### Starting an Emergency

When switching from Preparation → Response Mode:

```
Start new emergency?
This will reset all contact statuses and assignments.
[Cancel] [Start]
```

### Ending an Emergency

When switching from Response → Preparation Mode:

```
End emergency?
[Save report & clear] [Discard & clear] [Cancel]
```

### Data Persistence

- **Preparation data persists:** Families, skills, ministering assignments, response teams
- **Response data is ephemeral:** Contact statuses, needs, task assignments reset when emergency ends

---

## View Structure

```
[Mode Toggle: Preparation / Response]

─── Preparation Mode ───
├── Families
├── Ministering Assignments
├── Response Teams
└── Skills & Resources

─── Response Mode ───
├── Welfare Check
├── Needs & Assignments
└── Resource Lookup
```

---

## Preparation Mode Views

### Families View (existing, enhance)

**Current features:**
- Side-by-side list + map
- Expandable family rows (members, address, phone)
- Search bar
- Edit/Delete actions
- "Unknown Location" section for non-geocoded families

**Enhancements to add:**

Data completeness indicators:
- Visual flags for missing phone, missing address, not geocoded
- Summary: "12 families missing phone numbers"

Skills/resources on family record:
- Editable list per family
- Displayed in expanded view

### Ministering Assignments View

**Purpose:** Manage who checks on whom.

**Layout:** List of companionships, each showing:
- Minister name(s)
- Assigned families (count + expandable list)
- Gap indicator if companionship has no assignments

**Features:**
- Add/edit companionships
- Assign/unassign families
- Highlight unassigned families
- Summary: "4 families without ministers"

### Response Teams View

**Purpose:** Define capability-based teams for emergency work.

**Layout:** List of teams, each showing:
- Team name
- Team lead + phone
- Member count (expandable roster)
- Associated capabilities (chainsaw, medical, transport, etc.)

**Features:**
- Create/edit/delete teams
- Add/remove members
- Designate team lead

### Skills & Resources View

**Purpose:** Inventory of what capabilities exist across the ward.

**Layout:** Grouped by skill/resource type:
```
Generators (4)
├── Smith, John — 238 Oak Dr
├── Patel, Anand — 412 Elm St
└── ...

Nurses (2)
├── Carter, Maria — 89 Pine Rd
└── ...

Chainsaw (6)
└── ...
```

**Features:**
- Search/filter by skill or resource
- Click to view family details
- Show on map

---

## Response Mode Views

### Welfare Check View

**Purpose:** Track ministering contact status during emergency.

**Layout:** Similar to current Families view with additions.

**Progress summary (top of list):**
```
47 families: 23 OK • 3 need help • 21 remaining
[progress bar visualization]
```

**Filter tabs:**
```
[All (47)] [Remaining (21)] [Needs Help (3)] [OK (23)]
```

**Family list enhancements:**

Status indicator on each row:
- Gray circle: Not contacted
- Green checkmark: OK
- Orange/red flag: Needs help
- Yellow question mark: Unable to reach (optional)

Quick status actions in expanded row:
```
[OK] [Needs Help] [Can't Reach]  |  [Add Note]
```

Notes field for details: "Out of town until Thursday", "Needs generator"

**Map enhancements:**
- Pin colors reflect status (gray/green/orange)
- Filter applies to map (show only remaining, etc.)

**Grouping option:**
- View by ministering assignment (which companionship responsible)
- View all (current default)

### Needs & Assignments View

**Purpose:** Triage incoming needs and assign to response teams.

**Layout:** Three-section view or tabs.

**Section 1: Unassigned Needs (Triage Queue)**
```
Unassigned (3)
─────────────────────────────────
Smith, John — tree on house
  238 Oak Dr • (919) 555-1234
  [Tree Removal ▼] [Assign to: Chainsaw Crew ▼] [Assign]

Patel, Anand — no power, needs generator  
  412 Elm St • (919) 555-5678
  [Generator ▼] [Assign to: ______ ▼] [Assign]
```

Need categories (configurable):
- Tree removal
- Generator
- Medical
- Transport
- Shelter
- Other

**Section 2: By Team**
```
Chainsaw Crew (2 assigned • 1 complete)
  Team lead: Mike Reynolds (919) 555-0000
  ─────────────────────────────────
  [ ] Smith, John — 238 Oak Dr — tree on house
  [✓] Carter, Luis — 89 Pine Rd — cleared

Medical Team (1 assigned)
  Team lead: Dr. Sarah Webb (919) 555-1111
  ─────────────────────────────────
  [ ] Nguyen, Thi — 567 Maple Ave — check on elderly mother
```

**Section 3: Completed**

Log of resolved needs for reference.

**Actions:**
- Mark assignment complete
- Reassign to different team
- Return to unassigned (unassign)

**Print Team Sheet:**

Generate printable/exportable sheet for team lead:
```
Chainsaw Crew — Jan 4, 2026
Team lead: Mike Reynolds (919-555-0000)

□ Smith, John — 238 Oak Dr — tree on house
□ Patel, Anand — 412 Elm St — blocked driveway

[Map with these addresses]
```

### Resource Lookup View

**Purpose:** Quickly find families with specific skills/resources.

**Layout:** Search-first interface.

```
[Search: generator___________]

Results (4 families):
─────────────────────────────────
Smith, John — 238 Oak Dr — 0.5 mi
  (919) 555-1234
  
Patel, Anand — 412 Elm St — 1.2 mi
  (919) 555-5678
  
...
```

**Features:**
- Search by skill or resource type
- Show distance from a reference point (optional)
- Show on map
- Quick call action (if platform supports)

---

## Creating Needs from Welfare Check

When a family is marked "Needs Help" in Welfare Check view, prompt for details:

```
What does this family need?

[Tree Removal] [Generator] [Medical] [Transport] [Other]

Notes: [___________________________]

[Save]
```

This creates an entry in the Needs & Assignments triage queue.

---

## Status Indicator Design

Use color + icon for accessibility (color blindness):

| Status | Color | Icon | Meaning |
|--------|-------|------|---------|
| Not contacted | Gray | ○ (empty circle) | Default starting state |
| OK | Green | ✓ (checkmark) | Contacted, no needs |
| Needs help | Orange/Red | ⚑ (flag) or ! | Contacted, requires follow-up |
| Unable to reach | Yellow | ? (question mark) | Attempted, no response |

---

## Post-Emergency Report

Before clearing response data, offer export:

**Summary statistics:**
- Total families in ward
- Families contacted
- Families unreached
- Families that needed help (by category)

**Response team activity:**
- Tasks assigned per team
- Tasks completed per team

**Unresolved needs** (if any)

**Export formats:** PDF, CSV, or both

---

## Technical Notes

- **Offline-first:** All data stored locally
- **Single user:** No authentication, roles, or sync needed
- **Desktop app:** Current implementation appears to be Windows (title bar style)
- **Map:** Using OpenStreetMap tiles (per screenshot)
- **Import:** Must support ward list and ministering assignment import (format TBD, likely CSV or LCR export)

---

## Future Considerations (Out of Scope for Now)

- Multiple simultaneous emergencies
- Historical event comparison
- Mobile companion app for field teams
- Real-time sync across multiple leaders
- Integration with church directory (LCR API)
