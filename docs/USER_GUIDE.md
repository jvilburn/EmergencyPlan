# Ward Emergency Plan - User Guide

## Overview

Ward Emergency Plan is a desktop application for managing ward family data, ministering assignments, emergency response teams, and resources. It includes an interactive map and tools for coordinating welfare checks and task assignments during emergencies.

---

## Getting Started

### Starting a Document

- The app starts each launch with an empty document
- **File > New** (Ctrl+N) - Start again with an empty document

### Importing Data

The fastest way to populate your ward data is by importing PDFs from church tools:

1. **File > Import > Ward Directory PDF** - Import families, members, addresses, and contact info
2. **File > Import > Ministering PDF** - Import EQ or RS ministering districts and assignments

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

During an emergency, each team also lists the tasks assigned to it, and **Unassigned Tasks** lists the tasks no one has been given; right-click one to assign it to a team. A task assigned to a person is listed in the Tasks tab.

### Needs

Track special needs for ward members (e.g., oxygen dependency, mobility issues, dialysis).

- **Add Special Need**: Assign a need to a person or family with a note
- **Edit**: Update the note
- **Delete**: Remove a special need entry

### Medical / Communications / Skills & Gear

These three tabs manage emergency assets: people with specific skills or equipment.

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
- **+/-** buttons (top-left) to zoom in/out
- **Recenter** button to fit all families in view
- **Layer toggle** (top-right) to switch between Street and Satellite views

### Markers

- Click a marker to select that family in the sidebar
- Marker colors change during emergencies to reflect contact status
- The **Unmapped** panel (bottom-right) lists families without coordinates; click a name to select it

---

## Emergency Response

### Starting an Emergency

1. **Emergency > Start Emergency**, offered while no emergency is in progress
2. Enter a name (e.g., "January 2026 Ice Storm")
3. The app enters emergency mode:
   - An orange banner appears: **EMERGENCY: [name]**
   - The **Tasks** tab appears in the sidebar
   - Action buttons appear in the Families tab
   - Map markers gain status badges
   - A progress bar shows welfare check status

Everyone who has the ward open sees the emergency once their copy is up to date, and their welfare checks and tasks go into the same emergency.

A ward has one emergency in progress at a time. Two people who each start one while they are offline end up with one emergency: the first to reach the ward is the one in progress, under its name, and the other person's checks and tasks go into it. In the same way, if an emergency ends and a new one starts while someone is offline, the checks and new tasks they recorded go into the new one.

Opening a ward, or **File > New**, shows that ward's own emergency, if it has one in progress.

### Welfare Checks

In the Families tab during an emergency, each family has action buttons:

- **OK** (green) - Family has been contacted and is fine
- **Unable to Reach** (orange) - Contact attempted but no response
- **Add Task** - Create a task for something the family needs
- **Log Contact** - Record a contact attempt: the method, who made the contact, the outcome (none, OK, or Unable to Reach), and notes

**OK** and **Unable to Reach** each record a contact attempt marked with that outcome, as does a logged contact given an outcome. A family's status is the outcome of its most recent attempt that has one, so two people who mark the same family differently both appear in its history, and the family shows the later one. A family with an unresolved task shows as "Needs Help" whatever its last outcome.

The progress bar at the top updates in real time:
```
47 families: 23 OK - 3 need help - 2 unable to reach - 19 remaining
```

Use the filter tabs (**All**, **Remaining**, **OK**, **Needs Help**, **Unable to Reach**) to focus on specific groups.

### Tasks

The Tasks tab shows all emergency tasks across all families in a single table.

- **Add Task**: Select a family, choose a category (Tree removal, Flooding, Generator, Medical, Transport, Shelter, Other, or one added for this emergency), enter a description, and optionally assign to a team or person
- **Edit**: Update task details or assignment
- **Delete**: Remove a task, after you confirm
- **Notify**: Record that you've notified the assignee (method, notes). After you add or edit a task whose assignee has not been told, the app offers this straight away.
- **Resolved checkbox**: Toggle task completion

A family with unresolved tasks automatically shows as "Needs Help". Giving a task to someone else clears its notification, since the new assignee has not been told.

### Ending an Emergency

1. **Emergency > End Emergency**, then **End** to confirm, or **Cancel** to keep the emergency in progress
2. Choose where to save the summary report. The report is the record of the emergency: ending saves it and then removes the emergency's data from the ward. Cancelling the save ends nothing.

If someone else ended the emergency while you were choosing, the app says so and nothing more happens. If the report cannot be saved where you chose, the emergency is not ended.

### Reports

**Emergency > Generate Report** creates a PDF summary of the emergency in progress, without ending it: contact status breakdown, task assignments, resolution statistics, and each family's contact attempts and tasks. Its header shows the time the emergency started and the time the report was written. Families, people and teams are named as they stood when each attempt, task or assignment was recorded.

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New | Ctrl+N |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Quit | Ctrl+Q |

---

## Undo / Redo

Most actions support undo and redo: family edits, team changes, imports, and more. The Edit menu shows the name of the action that will be undone or redone.

Contact attempts and task changes can be undone too. Starting an emergency and adding a task category cannot. Undoing the end of an emergency brings it back as it was; the report already saved stays where it is.

---

## Your Document

- The app holds the document in memory while it runs, and the next launch starts with an empty one
- The title bar shows the ward's name, from the wards in the document
