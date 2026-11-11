# Ward Planning - User Guide

## Overview

Ward Planning is a desktop application for managing ward family data, ministering assignments, emergency response teams, and resources. It includes an interactive map and tools for coordinating welfare checks and task assignments during emergencies.

---

## Getting Started

### Wards

- The first launch asks for a name for this device and for your name
- **File > New Ward** (Ctrl+N) - Creates a ward on this device, under your name
- **File > Open Ward** (Ctrl+O) - Lists the wards on this device by name; a ward with changes you have not yet sent is marked *changes to share*, and one no longer shared with this device is marked *no longer shared*
- **File > Show in Folder** - Opens the folder that holds the ward's file, with the file selected; only this ward's devices can open it
- Each launch reopens the last ward; when there is none, or it does not open, the ward list shows instead

### Importing Data

The fastest way to populate your ward data is by importing PDFs from church tools:

1. **File > Import > Ward Directory PDF** - Import families, members, addresses, and contact info
2. **File > Import > Ministering PDF** - Import EQ or RS ministering districts and assignments

After importing, the app automatically geocodes addresses in the background (shown in the status bar).

### Settings

**File > Settings** holds settings for the whole app, on this device:

- **Use the Ward Planning server** - Turn this off only if the Ward Planning server has shut down. Every member of your wards must turn it off too.
- **This device's name** - The name this device has in your wards, asked for at the first launch. Changing it renames this device in every ward on this device.
- **Your name** - The name you have in the wards you create or join, asked for at the first launch. Changing it renames you in every ward on this device. A new name set on another of your devices reaches this one in the updates you open.
- **Map home** - Where the map opens. The app first sets it from your approximate location; **Use current view** sets it to what the map shows now.

### Sharing updates

With **Use the Ward Planning server** turned off in Settings, the members of a ward share their changes by sending each other updates:

- **File > Send Update** - Saves the ward as it stands on this device as an update, named for the ward and the day (such as *Tanglewood Ward 2026-11-08.wardupdate*), in the *Ward Planning* folder in Documents, in place of an update of that ward saved the same day, and opens the folder with the file selected. Send the file, by email, a message or anything else that carries a file, to every other member of the ward. Each member sends their own updates to every other member.
- **You have changes to share** - Shown above the map, with a **Send Update** button, while this device has changes that no update it has sent or opened carries. Sending an update clears it. Opening an update from someone who did not have your changes yet puts your changes on top of theirs, so it shows again.
- **Opening an update** - Double-click the file, drop it on the window, or use **File > Open Update...**. When another ward is open, the update's ward opens first. An update older than the one this device has is set aside with *You already have a newer update*.
- **This ward is no longer shared with this device.** - Shown above the map when an update opened here leaves this device out. The copy stays on this device, read-only, until you delete it with **Delete this copy**, which then shows the ward list.

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

Opening a ward, or **File > New Ward**, shows that ward's own emergency, if it has one in progress.

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

**Emergency > Generate Report** creates a PDF summary of the emergency in progress, without ending it: contact status breakdown, task assignments, resolution statistics, and each family's contact attempts and tasks. Its header shows the time the emergency started and the time the report was written. People and teams are named as they stood when each attempt or assignment was recorded. A family is named as it is in the ward, or, once it has left the ward, as its attempts and tasks recorded it.

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New Ward | Ctrl+N |
| Open Ward | Ctrl+O |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Quit | Ctrl+Q |

---

## Undo / Redo

Most actions support undo and redo: family edits, team changes, imports, and more. The Edit menu shows the name of the action that will be undone or redone.

Contact attempts and task changes can be undone too. Starting an emergency and adding a task category cannot. Undoing the end of an emergency brings it back as it was; the report already saved stays where it is.

---

## Saving

- Every change is saved as it is made
- If saving fails, the app says why, keeps the change while it stays open, and saves it as soon as saving works again; closing, or moving to another ward, asks first
- The title bar shows the ward's name, from the wards it has imported
