# Ward Emergency Plan - User Guide

## Overview

Ward Emergency Plan is a desktop application for managing ward family data, ministering assignments, emergency response teams, and resources. It includes an interactive map and tools for coordinating welfare checks and task assignments during emergencies.

---

## Getting Started

### Creating or Opening a Document

- **File > New** (Ctrl+N) — Start with an empty document
- **File > Open** (Ctrl+O) — Open an existing `.emergencyplan` file
- The app automatically reopens your last document on launch

### Importing Data

The fastest way to populate your ward data is by importing PDFs from church tools:

1. **File > Import > Ward Directory PDF** — Import families, members, addresses, and contact info
2. **File > Import > Ministering PDF** — Import EQ or RS ministering districts and assignments

After importing, the app automatically geocodes addresses in the background (shown in the status bar).

---

## Application Layout

```
+-----------------------------------------------+
| File  Edit  Emergency                          |
+------------------+----------------------------+
| [Families]       |                            |
| [Ministering]    |                            |
| [Teams] [Needs]  |         Map                |
| [Medical] [Comms]|                            |
| [Skills & Gear]  |                            |
|                  |                            |
| (sidebar content)|                            |
|                  |                            |
+------------------+----------------------------+
| status bar                                     |
+-----------------------------------------------+
```

- **Sidebar** (left): Navigation tabs across the top, with the selected view below
- **Map** (right): Interactive map showing family locations
- **Status bar** (bottom): Family count, geocoding progress, and operation feedback

---

## Sidebar Tabs

### Families

Lists all ward families. Click a family to select it and highlight it on the map.

- **Search**: Type in the search field to filter by name
- **Double-click** a family to open the edit panel
- **Right-click** for options: Edit, Delete

The edit panel slides in from the left and lets you modify:
- Address (multi-line)
- Latitude/longitude (with a "Look Up Coordinates" button for geocoding)
- Family members: name, gender, birthday, phone, email, callings

### Ministering

Shows ministering assignments organized by district and group. Switch between **Elders Quorum** and **Relief Society** using the tabs at the top.

- **Unassigned section** (collapsible) shows families not yet in a ministering group
- Expand districts to see groups and their assigned ministers and families
- Selecting a group highlights all its families on the map

### Teams

Create and manage emergency response teams.

- **Add**: Create a new team
- **Edit**: Rename a team
- **Delete**: Remove a team
- **Set Leader**: Designate a team member as leader
- **Remove**: Remove a person from a team

Expand a team to see members and their contact details (phone, email).

### Needs

Track special needs for ward members (e.g., oxygen dependency, mobility issues, dialysis).

- **Add Special Need**: Assign a need to a person or family with a note
- **Edit**: Update the note
- **Delete**: Remove a special need entry

### Medical / Communications / Skills & Gear

These three tabs manage emergency assets — people with specific skills or equipment.

- **Add**: Create a new asset (e.g., "Generator", "EMT Certified")
- **Edit**: Rename an asset
- **Delete**: Remove an asset
- **Remove Person**: Unassign a person from an asset

Expand an asset to see assigned people and their contact details.

---

## Map

The interactive map shows family locations as markers.

### Controls

- **Drag** to pan
- **Scroll wheel** to zoom
- **+/−** buttons (top-left) to zoom in/out
- **Recenter** button to fit all families in view
- **Layer toggle** (top-right) to switch between Street and Satellite views

### Markers

- Click a marker to select that family in the sidebar
- Marker colors change during emergencies to reflect contact status
- The **Unmapped** panel (bottom-right) lists families without coordinates — click a name to select it

---

## Emergency Response

### Starting an Emergency

1. **Emergency > Start Emergency**
2. Enter a name (e.g., "January 2026 Ice Storm")
3. The app enters emergency mode:
   - An orange banner appears: **EMERGENCY: [name]**
   - The **Tasks** tab appears in the sidebar
   - Action buttons appear in the Families tab
   - Map markers gain status badges
   - A progress bar shows welfare check status

### Welfare Checks

In the Families tab during an emergency, each family has action buttons:

- **OK** (green) — Family has been contacted and is fine
- **Unable to Reach** (orange) — Contact attempted but no response
- **Add Task** — Create a task for something the family needs
- **Log Contact** — Record a contact attempt (method, who contacted, notes)

The progress bar at the top updates in real time:
```
47 families: 23 OK · 3 need help · 2 unable to reach · 19 remaining
```

Use the filter tabs (**All**, **Remaining**, **OK**, **Needs Help**, **Unable to Reach**) to focus on specific groups.

### Tasks

The Tasks tab shows all emergency tasks across all families in a single table.

- **Add Task**: Select a family, choose a category (Tree Removal, Generator, Medical, Transport, Shelter, Flooding, Other), enter a description, and optionally assign to a team or person
- **Edit**: Update task details or assignment
- **Delete**: Remove a task
- **Notify**: Record that you've notified the assignee (method, notes)
- **Resolved checkbox**: Toggle task completion

A family with unresolved tasks automatically shows as "Needs Help".

### Ending an Emergency

1. **Emergency > End Emergency**
2. Choose:
   - **Archive & End** — Save all response data for future reference
   - **Discard & End** — Delete response data permanently
   - **Cancel** — Keep the emergency active

### Reports

**Emergency > Generate Report** creates a PDF summary including contact status breakdown, task assignments, and resolution statistics.

### Archives

**Emergency > Open Archive** lets you browse past emergencies:

- **View**: Open in read-only mode (gray banner, no editing)
- **Reopen**: Resume a previous emergency as active
- **Delete**: Permanently remove an archive

Close an archive with **Emergency > Close Archive**.

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save As | Ctrl+Shift+S |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Quit | Ctrl+Q |

---

## Undo / Redo

Most actions support undo and redo: family edits, team changes, imports, and more. The Edit menu shows the name of the action that will be undone or redone.

Emergency response actions (contact status, tasks) are **not** part of the undo system — they are event logs. To correct a mistake, edit or delete the entry directly.

---

## Saving

- **Ctrl+S** saves the document
- The app auto-saves after changes
- The title bar shows an asterisk (*) when there are unsaved changes
- On close, the app prompts to save if there are unsaved changes
