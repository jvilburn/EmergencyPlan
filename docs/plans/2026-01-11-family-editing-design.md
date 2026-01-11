# Family Editing Design

## Overview

In-place family editing via a side panel in WardListView. Users click Edit on a family to open an edit panel on the right side, with a visual tie (shared highlight color) connecting the selected family row to the panel.

## Design Decisions

| Decision | Choice |
|----------|--------|
| Primary use case | Ongoing maintenance (full editing capability) |
| Layout approach | Side panel (master-detail) |
| Visual tie | Shared background highlight color |
| Panel visibility | Appears on Edit click only |
| Member editing | Inline accordion expansion |
| Save behavior | Explicit Save button (single UpdateFamilyCommand) |
| Panel close | Save, Cancel, or [X] button |
| Address field | Multi-line textarea |
| Callings field | Simple list with add/remove |
| Validation | None (accept any input) |
| Scroll behavior | Sticky row positioning with drop shadow |

## Layout

### Panel Structure

```
+-- Edit: Smith ------------------------ [X] --+
|                                              |
|  Address                                     |
|  +--------------------------------------+    |
|  | 123 Main St                          |    |
|  | Salt Lake City, UT 84101             |    |
|  +--------------------------------------+    |
|  Location               [Look Up Coordinates]|
|  Lat: [________]    Lon: [________]          |
|                                              |
|  --------------- Members --------------- [+] |
|  (accordion list - see below)                |
|                                              |
|                        [Cancel]  [Save]      |
+----------------------------------------------+
```

### Visual Connection

The edited family row in the tree and the edit panel share a matching highlight background color (e.g., light blue `#e3f2fd`). This creates visual continuity across the splitter.

### Full Layout (Side Panel Open)

```
+-----------------------------------+------------------------------------------+
| [Search families...______]        |                                          |
|###################################|## Edit: Smith ##########################|
|#> Smith, John & Jane #############|# Address                                #|
|#  +- John Smith                  #|# [123 Main St___________________]       #|
|#  |    555-1234                  #|# [Salt Lake City, UT 84101_____]        #|
|#  +- Jane Smith                  #|# Location          [Look Up Coordinates]#|
|#  |    555-5678                  #|# Lat: [40.7608__]  Lon: [-111.891_]     #|
|###################################|##########################################|
| > Johnson, Mike & Sarah           |                                          |
| > Williams, Bob                   | --------------- Members ------------ [+] |
|                                   | (member accordion here)                  |
|                                   |                                          |
|                                   |                      [Cancel]  [Save]    |
+-----------------------------------+------------------------------------------+
    ### = shared highlight color
```

## Member Editing (Accordion)

Each family member is shown in a collapsible row. Clicking expands to show editable fields.

### Collapsed State

```
+-- * John Smith (Parent) -----------------------+
|   Male - 15 Mar - 555-1234                     |
+------------------------------------------------+
```

### Expanded State

```
+-- * Jane Smith (Parent) ----------------- [v] -+
|   Female - 22 Aug - 555-5678                   |
+------------------------------------------------+
|  Name                                          |
|  Surname: [Smith_______] Given: [Jane____]     |
|                                                |
|  [x] Parent       Gender: [Female v]           |
|                                                |
|  Birthday                                      |
|  Day: [22]  Month: [Aug v]  Year: [____]       |
|  (leave year blank for adults)                 |
|                                                |
|  Contact                                       |
|  Phone: [555-5678________]                     |
|  Alt:   [________________]                     |
|  Email: [jane@email.com__]                     |
|                                                |
|  Callings                                      |
|  [Primary Teacher_____] [X]                    |
|  [RS Secretary________] [X]                    |
|               [+ Add Calling]                  |
|                                                |
|                          [Remove Member]       |
+------------------------------------------------+
```

Visual indicators:
- `*` (filled) = parent
- `o` (hollow) = child

## Behavior

### Opening the Panel

1. User clicks [Edit] button on family row
2. Panel slides in from right (via QSplitter)
3. Family row gets highlight background
4. Panel header matches highlight color
5. Focus moves to first field in panel

### Browsing While Editing

- User can expand/collapse other families in tree
- Tree selection can change without affecting edit panel
- Only the edited family keeps highlight color
- Panel stays attached to its family

### Sticky Row Behavior

When the edited family row would scroll out of view:

- **Scrolling up** (family would go above viewport): Row pins to top of tree view with bottom drop shadow
- **Scrolling down** (family would go below viewport): Row pins to bottom of tree view with top drop shadow
- **In natural position**: No pinning, no shadow

The drop shadow indicates "list continues behind this row."

### Collapse Behavior

If user collapses the edited family in the tree:
- Family row collapses but highlight remains
- Panel stays open
- Sticky behavior still applies to collapsed row

### Closing the Panel

| Action | Behavior |
|--------|----------|
| [Save] | Commit changes via UpdateFamilyCommand, close panel, remove highlight |
| [Cancel] | Discard changes, close panel, remove highlight |
| [X] | If unsaved changes: prompt "Save changes to [Family]?" with Save/Discard/Cancel. If no changes: close immediately |

### Unsaved Changes Detection

Compare current form state to original family data. Any field difference = unsaved changes. Show asterisk in panel title when dirty (e.g., "Edit: Smith *").

## Components

### New Widgets

| Widget | Purpose |
|--------|---------|
| `FamilyEditPanel` | Main edit panel (QFrame) with address, location, members, save/cancel |
| `MemberAccordion` | Collapsible container for member list |
| `MemberEditor` | Expanded view with all person fields |
| `CallingsEditor` | List of calling text fields with add/remove buttons |

### Modified Widgets

| Widget | Changes |
|--------|---------|
| `WardListView` | Add QSplitter, manage FamilyEditPanel, handle sticky row overlay, emit signals |
| `FamilyTreeModel` | Add "editing" role for highlight styling (optional) |

### Existing Classes Used

- `UpdateFamilyCommand` - handles undo/redo, triggers geocoding on address change
- `GeocodingService` - looks up coordinates from address
- `DocumentManager` - executes commands, emits documentChanged

## Data Flow

```
1. User clicks [Edit] on family row
   |
   v
2. WardListView::editFamilyRequested(familyId) signal
   |
   v
3. WardListView creates/shows FamilyEditPanel with Family data
   |
   v
4. User edits fields (local state in panel)
   |
   v
5. User clicks [Save]
   |
   v
6. FamilyEditPanel builds new Family from form fields
   |
   v
7. WardListView creates UpdateFamilyCommand(oldFamily, newFamily)
   |
   v
8. DocumentManager::executeCommand() runs command
   |
   v
9. DocumentManager emits documentChanged()
   |
   v
10. FamilyTreeModel updates, panel closes, highlight removed
```

## Styling

- **Highlight color**: Shared constant (e.g., `#e3f2fd` light blue)
- **Drop shadow**: `QGraphicsDropShadowEffect` on sticky overlay widget
- **Tree delegate**: Custom painting to extend highlight to row edges
- **Panel border**: Matches highlight color for visual continuity

## Field Details

### Address

Multi-line QTextEdit, 3-4 rows visible. Split on newlines when saving to Address model.

### Location

- Two QLineEdit fields (Latitude, Longitude)
- [Look Up Coordinates] button triggers GeocodingService
- Button disabled while lookup in progress, shows spinner or "Looking up..."

### Birthday

- Day: QSpinBox (1-31)
- Month: QComboBox (Jan-Dec)
- Year: QLineEdit (4 digits, optional - blank for adults)

### Gender

QComboBox with Male/Female options, plus empty option for unspecified.

### Parent Checkbox

QCheckBox labeled "Parent" - controls isParent flag on Person model.

### Callings

Vertical list of QLineEdit with [X] remove button each. [+ Add Calling] button appends new empty field.
