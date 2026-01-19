# Emergency Tab Design

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an Emergency tab to the sidebar for managing and quickly looking up emergency inventory (skills, equipment, special needs) with map visualization.

**Architecture:** New EmergencyView widget containing QTabWidget with sub-tabs. Each sub-tab uses QTreeWidget with three-level hierarchy. Implements MapHighlightProvider for map integration.

**Tech Stack:** Qt 6 Widgets, QTreeWidget, QTabWidget, existing WardListDialog for person/family selection

---

## Overview

The Emergency tab serves dual purposes:
1. **Inventory Management** - Add/edit/remove skills, equipment, and special needs
2. **Quick Lookup** - During emergencies, quickly find who has what skills or equipment

## UI Structure

### Tab Hierarchy

```
Sidebar Tabs
├── Families (existing)
├── Ministering (existing)
└── Emergency (NEW)
    ├── Teams (placeholder for future)
    ├── Skills
    ├── Equipment
    └── Needs
```

### Tree Structure (Skills/Equipment/Needs sub-tabs)

Three-level hierarchy with counts for quick scanning:

```
[Category Name] (total count)
├── [Item Name] (assigned count)
│   ├── Person/Family Name
│   ├── Person/Family Name
│   └── ...
└── [Item Name] (assigned count)
    └── ...
```

Example for Skills:
```
Medical (5)
├── CPR Certified (3)
│   ├── John Smith
│   ├── Mary Johnson
│   └── Robert Brown
├── Nurse (1)
│   └── Sarah Williams
└── Doctor (1)
    └── Dr. James Miller
```

## Interaction Model

### Terminology

Each sub-tab uses context-specific terms:
- **Skills tab**: Category contains Skills (e.g., "Medical" category → "CPR Certified" skill)
- **Equipment tab**: Category contains Equipment (e.g., "Power" category → "Generator" equipment)
- **Needs tab**: No categories; flat list of Special Needs

### Single-Click Behavior
- **On Category** → Selects; highlights all assigned people/families in that category on map
- **On Skill/Equipment/Need** → Selects; highlights assigned people/families on map
- **On Assigned person/family** → Selects; highlights just that person/family on map

Expand/collapse is exclusively via the arrow or double-click (standard tree behavior).

### Double-Click Behavior
- **On Category** → Expands/collapses (default tree behavior)
- **On Skill/Equipment/Need** → Expands/collapses (default tree behavior)
- **On Assigned person/family** → Expands/collapses to show contact details (address, phone, email)

### Right-Click Context Menu

On Category:
- **Rename...** → Opens rename dialog
- **Add [Category] Skill/Equipment...** → Dynamic label (e.g., "Add Medical Skill...", "Add Shelter Equipment...")
- **Delete** → Confirms and deletes category (and all contained skills/equipment)

On Skill/Equipment/Need:
- **Select People...** / **Select Families...** → Opens WardListDialog picker
- **Rename...** → Opens rename dialog
- **Delete** → Confirms and deletes

On Assigned person/family:
- Contact info section (disabled/non-clickable): address, phone, email
- ─────────── (separator)
- **Remove** → Removes this person/family from the skill/equipment/need

On empty area (Skills/Equipment tabs):
- **Add Category...** → Opens dialog to create new category

On empty area (Needs tab):
- **Add Special Need...** → Opens dialog to create new special need

### Menu Bar: Emergency Menu

Dedicated "Emergency" menu in the menu bar. Menu is always visible; items are disabled when not on Emergency tab.

```
Emergency
├── Add Category...
├── Add Skill/Equipment/Need...
├── ─────────
├── Select People...
├── Select Families...
├── ─────────
├── Rename...
└── Delete
```

Labels are dynamic based on active sub-tab (e.g., "Add Medical Skill..." when Medical category selected).

| Action | Enabled When |
|--------|--------------|
| Add Category... | Emergency tab active (Skills/Equipment sub-tabs) |
| Add Skill/Equipment/Need... | Category or Skill/Equipment/Need selected |
| Add Special Need... | Emergency tab active (Needs sub-tab) |
| Select People... | Skill/Equipment/Need selected |
| Select Families... | Skill/Equipment/Need selected |
| Rename... | Category or Skill/Equipment/Need selected |
| Delete | Category, Skill/Equipment/Need, or Assigned person selected |

### Map Highlighting

When a skill, equipment, or special need item is selected:
- Assigned families/persons are highlighted on the map
- Unassigned families are dimmed
- Follows same pattern as MinisteringView

## Files to Create

### Views
- `src/widgets/EmergencyView.h/.cpp` - Main tab container with QTabWidget
- `src/widgets/SkillsSubView.h/.cpp` - Skills tree with MapHighlightProvider
- `src/widgets/EquipmentSubView.h/.cpp` - Equipment tree with MapHighlightProvider
- `src/widgets/NeedsSubView.h/.cpp` - Special needs tree with MapHighlightProvider

### Pattern Reference
- Follow `MinisteringView` pattern for MapHighlightProvider implementation
- Follow existing tree widget patterns in codebase

## Data Flow

### Loading Data
1. EmergencyView receives DocumentManager pointer
2. Each sub-view queries relevant data from Document:
   - Skills: `document.skillCategories()`, `document.skills()`
   - Equipment: `document.equipmentCategories()`, `document.equipment()`
   - Needs: `document.specialNeeds()`

### Updating Assignments
1. User double-clicks or right-clicks item
2. WardListDialog opens with current assignments checked
3. On OK, diff changes and execute commands:
   - `AssignPersonToSkillCommand` / `RemovePersonFromSkillCommand`
   - `AssignFamilyToEquipmentCommand` / `RemoveFamilyFromEquipmentCommand`
   - Similar for special needs

### DocumentChange Integration
Listen for relevant DocumentChange scopes:
- `DocumentChange::Scope::SkillCategories`
- `DocumentChange::Scope::Skills`
- `DocumentChange::Scope::EquipmentCategories`
- `DocumentChange::Scope::Equipment`
- `DocumentChange::Scope::SpecialNeeds`
- `DocumentChange::Scope::Families` (for name updates)
- `DocumentChange::Scope::Persons` (for name updates)

## UI Details

### Tree Item Data Roles
```cpp
static constexpr int IdRole = Qt::UserRole;
static constexpr int TypeRole = Qt::UserRole + 1;
enum class ItemType { Category, Item, Assigned };
```

### Counts Display
- Category row: "Medical (5)" - total persons/families across all items
- Item row: "CPR Certified (3)" - count of assigned persons/families

### Empty States
- Empty category: Show category name with (0)
- Empty item: Show item name with (0), no children

## Future: Teams Sub-Tab

Teams sub-tab is placeholder for future implementation. Will have different structure:
- Team → Members hierarchy
- "Assign" terminology appropriate for teams
- Different commands: `AssignPersonToTeamCommand`, etc.
