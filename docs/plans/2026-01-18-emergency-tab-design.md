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

### Double-Click Behavior
- **On Skill/Equipment item** → Opens WardListDialog with current assignments pre-selected
- **On Category** → Expands/collapses (default tree behavior)
- **On Assigned person/family** → No special action

### Right-Click Context Menu

On Skill/Equipment/SpecialNeed item:
- **Select People...** / **Select Families...** → Opens WardListDialog picker
- **Rename...** → Opens rename dialog
- **Delete** → Confirms and deletes item

On Category:
- **Rename...** → Opens rename dialog
- **Delete** → Confirms and deletes category (and all items)

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
   - `AssignPersonToSkillCommand` / `UnassignPersonFromSkillCommand`
   - `AssignFamilyToEquipmentCommand` / `UnassignFamilyFromEquipmentCommand`
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
