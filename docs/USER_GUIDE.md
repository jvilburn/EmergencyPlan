# Ward Planning - User Guide

## Overview

Ward Planning is a desktop application for managing ward family data, ministering assignments, emergency response teams, and resources. It includes an interactive map and tools for coordinating welfare checks and task assignments during emergencies. A ward can be shared with other people, each on their own devices, through an encrypted copy on a server that holds who has access but can never read the ward itself.

---

## Getting Started

### The First Time

Ward Planning asks whether you already use it on another device. If you do, choose **Yes, link this device** and follow *Linking a Device* below, so your wards come to this device too. If not, give your name as your ward knows you and a name for this device.

### Creating or Opening a Ward

Wards live inside the app, in its own folder, so there is no file to find or lose.

- **File > New Ward** (Ctrl+N) - Start a new, empty ward
- **File > Open Ward** (Ctrl+O) - Choose from the wards on this device; a ward shared with you that has not arrived yet shows as *Requested* or *Waiting for access*
- The app reopens your last ward on launch
- **File > Save a Copy** writes a `.wardplanning` file wherever you choose, for a backup or to hand to someone. **File > Open a Copy** reads one back, read-only. A copy is sealed to the same devices as the ward, so only they can open it.

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
- **+/−** buttons (top-left) to zoom in/out
- **Recenter** button to fit all families in view
- **Layer toggle** (top-right) to switch between Street and Satellite views

### Markers

- Click a marker to select that family in the sidebar
- Marker colors change during emergencies to reflect contact status
- The **Unmapped** panel (bottom-right) lists families without coordinates: click a name to select it

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

- **OK** (green) - Family has been contacted and is fine
- **Unable to Reach** (orange) - Contact attempted but no response
- **Add Task** - Create a task for something the family needs
- **Log Contact** - Record a contact attempt (method, who contacted, notes)

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

Ending an emergency clears its response data from the document. The summary report is the record that remains.

1. **Emergency > End Emergency**
2. Choose:
   - **Save Report & End** - Save the summary report, then end the emergency
   - **End Without Report** - End the emergency with no record kept
   - **Cancel** - Keep the emergency active

### Reports

**Emergency > Generate Report** creates a PDF summary including contact status breakdown, task assignments, and resolution statistics. The report is a PDF saved wherever you choose, and it is not encrypted.

---

## Sharing a Ward

A ward starts on one device, readable by nobody else. Sharing puts an encrypted copy on the Ward Planning server and lets you choose who may open it.

### What the server knows

The server holds who has access to each ward - names, roles, and device names - and who published each version and when. The ward itself reaches the server already encrypted, with a key the server never has, so nobody who runs or breaks into the server can read a family's details. Only the devices you have let in hold the key.

### Share and invite

1. Open the ward and choose **Ward > Who Has Access**, then **Share**.
2. **Invite Link** shows a link to send to the people you want to add. Anyone who opens it can ask for access; only you, or another admin, can grant it.

### Requests and grants

Opening an invite link, or a `.wardplanning` copy of a ward you do not have, offers **Request access**. The ward then shows in your list as *Requested* until an admin answers.

An admin sees requests at the bottom of **Who Has Access**, with the person's name and device, and chooses **Grant** or **Decline**. The moment a request is granted the ward is readable on the device that asked; it downloads within a minute. A request whose name matches someone who already has access is flagged: if it is that person's new phone, they should link it from their other device instead (see below), so the ward does not end up with two of them.

### Admins and members

Everyone with access reads and edits the ward. **Admins** also grant and decline requests, remove people, and make others admins or members. The person who shared the ward is its first admin. A ward always keeps at least one admin: the last admin cannot step down or leave until someone else is made an admin, and Ward Planning says so.

### Removing someone

An admin selects a person in **Who Has Access** and chooses **Remove**. They lose access at once, their app deletes its copy the next time it connects, and the next version anyone saves is sealed with a fresh key they do not hold. A copy they saved outside the app beforehand keeps what it had.

**Leave** does the same for yourself. A member's copy goes at once; their other devices learn of it when they next connect.

### Linking a device

Your wards can live on more than one of your devices - a laptop and a phone, say - and the server sees them as one person.

1. On the new device, start Ward Planning and choose **Yes, link this device**. Name the device and choose **Show Code**.
2. On a device you already use, open **Who Has Access** and choose **Link a Device**, enter the code, and confirm *Link "Pixel 8" to your account?*
3. Every ward on the approving device goes to the new one at that moment. A ward the approving device did not hold shows *Waiting for access* on the new one until the approving device next saves it.

Each ward shows a notice while it is on only one of your devices: a second device is what keeps a ward if the first is lost or stolen.

### Unlinking a lost device

In **Who Has Access**, select one of your own devices and choose **Unlink**. It loses every ward at once and deletes its copies the next time it connects; if it had linked other devices, you are asked whether to unlink those too. You cannot unlink your last device.

If a device is lost whose owner cannot be reached, an admin removes that person and grants them again from a device they still have.

### Who has access changed

Every grant, removal, leave, and role change is announced to everyone with access, once, as a notice above the ward - *3 changes to who has access* - that expands to the list. Dismiss it and it stays dismissed; new changes bring a new notice.

### The sync indicator

The status bar shows how the open ward stands with the server:

- *Up to date*
- *Changes not yet uploaded* - saved here; they upload when the server answers
- *Server unreachable* - keep working; everything uploads when it returns. If a newer Ward Planning is known, the indicator says so, since an old build is one reason a server stops answering
- *Could not check the server's copy* - your copy here is untouched
- *Two versions are waiting for your choice* - see below
- *A newer version of Ward Planning is needed to sync this ward* - someone saved it with a newer version; update to see their changes and share yours
- *... another member is editing the directory* - one person at a time edits families and members while connected; emergency responses are never blocked

### Two people editing at once

Family and member details take turns: while one person is editing the directory and connected, others see it read-only. Emergency responses - welfare checks, tasks - never wait, from anyone, at any time.

When two devices have both changed the same ward out of each other's sight, Ward Planning merges what it can. Where it cannot, **File > Choose Between Two Versions** names the other person and offers to keep yours, keep theirs, or save yours as a copy so nothing is lost.

### Working offline

Everything works without the server: open the ward, edit it, record contacts, print reports. Changes save on this device and upload when the server is reached again. During an outage, responders see each other's work only when the server is back.

**Access must be granted before an emergency.** Granting, linking, and removing all need the server. A ward that has not been shared with everyone who will need it cannot be shared once the network is down - which is exactly when it matters. Share early.

### Reports and copies

Reports are plain PDFs, saved where you choose and readable by anyone who has the file. A ward copy saved with **Save a Copy** is encrypted and opens only on the devices the ward is sealed to.

### Updates

When a newer Ward Planning is published, a notice offers **Download**. **Not Now** keeps that version quiet; the next one shows again.

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New Ward | Ctrl+N |
| Open Ward | Ctrl+O |
| Save Now | Ctrl+S |
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |
| Quit | Ctrl+Q |

---

## Undo / Redo

Most actions support undo and redo: family edits, team changes, imports, and more. The Edit menu shows the name of the action that will be undone or redone.

Emergency response actions (contact status, tasks) are **not** part of the undo system: they are event logs. To correct a mistake, edit or delete the entry directly.

---

## Saving

- Every change is saved to this device a moment after you make it; **Ctrl+S** saves now
- A shared ward then uploads the saved version to the server, conditional on nothing having changed there meanwhile; the sync indicator shows where it stands
- Closing the app saves and releases the editing turn; nothing is lost and nothing is asked
