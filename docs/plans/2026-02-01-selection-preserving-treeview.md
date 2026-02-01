# SelectionPreservingTreeView Design

## Problem

SelectableTreeView currently inherits from QWidget and takes a QTreeView* in its constructor. This mixes two concerns:
1. Selection caching/preservation (belongs on the tree)
2. Composite widget layout (belongs on the container)

The selection state is intrinsically tied to the tree's selection model, so it should live on a QTreeView subclass.

## Solution

Refactor SelectableTreeView into SelectionPreservingTreeView, which inherits from QTreeView instead of QWidget.

### SelectionPreservingTreeView : QTreeView

A QTreeView subclass that preserves selection across model rebuilds.

**Responsibilities:**
- Store `m_selectedType` and `m_selectedId` from current selection
- On `modelReset`, restore selection by finding the matching item
- Redirect ContactDetail selections to parent (store parent's type/id instead)
- Provide `selectedType()` and `selectedId()` accessors

**Pure virtual methods (subclass implements):**
- `itemTypeAt(index)` - returns ItemType for index
- `idAt(index)` - returns ID for index

**What it does NOT do:**
- Implement FamilyMarkerProvider
- Know about highlight computation
- Know about DocumentManager or domain models

### Container Views : QWidget, FamilyMarkerProvider

Each tree view becomes a composite widget containing:
- A SelectionPreservingTreeView-derived inner tree class
- Optional toolbar buttons or other UI

**Responsibilities:**
- Create and own the tree
- Implement FamilyMarkerProvider by querying tree's `selectedType()`/`selectedId()`
- Handle business logic (add/edit/delete, context menus, etc.)

## Files Changed

| File | Change |
|------|--------|
| `SelectableTreeView.h/cpp` | Rename to `SelectionPreservingTreeView`, inherit from `QTreeView` |
| `EmergencyResourceView.h/cpp` | Create inner `EmergencyResourceTree`, inherit QWidget + FamilyMarkerProvider |
| `MinisteringView.h/cpp` | Create inner `MinisteringTree` using SelectionPreservingTreeView |
| `NeedsSubView.h/cpp` | Create inner `NeedsTree` using SelectionPreservingTreeView |
| `WardListView.h/cpp` | Create inner `WardListTree` using SelectionPreservingTreeView |
| `CMakeLists.txt` | Update filename |

Each inner tree class implements `itemTypeAt()` and `idAt()` by delegating to its model. Inner classes are defined in the .cpp file (private implementation detail).
