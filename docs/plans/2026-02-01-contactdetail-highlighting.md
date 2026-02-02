# Consistent Tree View Selection Pattern Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Standardize selection handling across all tree views with a unified ItemType enum, common base class, and consistent architecture where each view has one tree, one model, one selection state.

**Architecture:**
- Unified `ItemType` enum shared by all models
- `SelectableTreeView` base class handles selection caching and parent redirect
- All tree views derive from base class, implement `computeHighlight()`
- Container views (ResourcesView, MinisteringView) delegate to sub-views

**Tech Stack:** Qt 6 / C++17, QTreeView selection handling

---

## Completed (by user)

- ~~Rename NodeType → ItemType in MinisteringModel~~
- ~~Update UnassignedMinisteringModel for ItemType~~
- ~~Update MinisteringView for ItemType~~

---

## Part 1: Unified ItemType Enum

### Task 1: Create unified ItemType enum

**Files:**
- Create: `src/listmodels/ItemType.h`

**Step 1: Create header with unified enum**

```cpp
#pragma once

#include <QMetaType>

/// Unified item type enum for all tree models.
/// Each model uses a subset of these types.
enum class ItemType
{
    Invalid,
    ContactDetail,  // All models - expandable contact info

    // Emergency Resource types
    Resource,       // EmergencyResourceModel - resource group

    // Needs types
    Person,         // NeedsModel, EmergencyResourceModel - individual person

    // Ministering types
    District,           // MinisteringModel - top level grouping
    Companionship,      // MinisteringModel - minister group
    SectionHeader,      // MinisteringModel - "Ministers" / "Ministered" headers
    Minister,           // MinisteringModel - person who ministers
    MinisteredFamily,   // MinisteringModel, UnassignedModel - family being ministered
    MinisteredSister,   // MinisteringModel, UnassignedModel - sister being ministered (RS)
    UnassignedHeader    // UnassignedModel - "Unassigned (N)" header
};
Q_DECLARE_METATYPE(ItemType)
```

**Step 2: Build (expect errors - enum not yet used)**

Run: `build.bat`
Expected: Clean compile (new file, not referenced yet)

**Step 3: Commit**

```bash
git add src/listmodels/ItemType.h
git commit -m "feat: add unified ItemType enum for all tree models"
```

---

### Task 2: Update MinisteringModel to use unified ItemType

**Files:**
- Modify: `src/listmodels/MinisteringModel.h`
- Modify: `src/listmodels/MinisteringModel.cpp`

**Step 1: Update header**

Replace the enum class definition with include:

```cpp
#include "ItemType.h"

// Remove: enum class ItemType { ... };
// Remove: Q_ENUM(ItemType)
```

**Step 2: Update .cpp**

No changes needed if enum values match.

**Step 3: Build (expect errors in dependent files)**

Run: `build.bat`
Expected: Errors in UnassignedMinisteringModel, MinisteringView (they reference MinisteringModel::ItemType)

**Step 4: Commit**

```bash
git add src/listmodels/MinisteringModel.h src/listmodels/MinisteringModel.cpp
git commit -m "refactor(MinisteringModel): use unified ItemType enum"
```

---

### Task 3: Update UnassignedMinisteringModel

**Files:**
- Modify: `src/listmodels/UnassignedMinisteringModel.h`
- Modify: `src/listmodels/UnassignedMinisteringModel.cpp`

**Step 1: Update header**

```cpp
#include "ItemType.h"

// Remove: using ItemType = MinisteringModel::ItemType;
// Just use ItemType directly
```

**Step 2: Update accessor return type**

```cpp
ItemType itemTypeAt(const QModelIndex& index) const;  // was: nodeTypeAt
```

**Step 3: Update .cpp**

Rename `nodeTypeAt` to `itemTypeAt`, use `ItemType::` directly.

**Step 4: Build**

Run: `build.bat`
Expected: Errors in MinisteringView

**Step 5: Commit**

```bash
git add src/listmodels/UnassignedMinisteringModel.h src/listmodels/UnassignedMinisteringModel.cpp
git commit -m "refactor(UnassignedMinisteringModel): use unified ItemType enum"
```

---

### Task 4: Update EmergencyResourceModel

**Files:**
- Modify: `src/listmodels/EmergencyResourceModel.h`
- Modify: `src/listmodels/EmergencyResourceModel.cpp`

**Step 1: Update header**

```cpp
#include "ItemType.h"

// Remove: enum class ItemType { Invalid, Resource, Person, ContactDetail };
// Remove: Q_ENUM(ItemType)
```

**Step 2: Update .cpp**

Replace `ItemType::` references. Note: `Person` is now in unified enum.

**Step 3: Build**

Run: `build.bat`
Expected: Errors in EmergencyResourceView

**Step 4: Commit**

```bash
git add src/listmodels/EmergencyResourceModel.h src/listmodels/EmergencyResourceModel.cpp
git commit -m "refactor(EmergencyResourceModel): use unified ItemType enum"
```

---

### Task 5: Update NeedsModel

**Files:**
- Modify: `src/listmodels/NeedsModel.h`
- Modify: `src/listmodels/NeedsModel.cpp`

**Step 1: Update header**

```cpp
#include "ItemType.h"

// Remove: enum class ItemType { Invalid, Person, ContactDetail };
// Remove: Q_ENUM(ItemType)
```

**Step 2: Rename accessor**

```cpp
QString idAt(const QModelIndex& index) const;  // was: personIdAt
```

**Step 3: Update .cpp**

Rename `personIdAt` to `idAt`, use `ItemType::` directly.

**Step 4: Build**

Run: `build.bat`
Expected: Errors in NeedsSubView

**Step 5: Commit**

```bash
git add src/listmodels/NeedsModel.h src/listmodels/NeedsModel.cpp
git commit -m "refactor(NeedsModel): use unified ItemType enum, rename personIdAt to idAt"
```

---

## Part 2: SelectableTreeView Base Class

### Task 6: Create SelectableTreeView base class

**Files:**
- Create: `src/widgets/SelectableTreeView.h`
- Create: `src/widgets/SelectableTreeView.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header**

```cpp
#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class QTreeView;
class QAbstractItemModel;

/// Base class for tree views with selection caching and parent redirect.
/// Subclasses implement model-specific highlight computation.
class SelectableTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit SelectableTreeView(QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override final;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

protected:
    // Subclasses must implement
    virtual ItemType itemTypeAt(const QModelIndex& index) const = 0;
    virtual QString idAt(const QModelIndex& index) const = 0;
    virtual HighlightInfo computeHighlight() const = 0;

    // Subclasses call this to set up the tree
    void initTree(QTreeView* tree);

    // Selection state (accessible to subclasses for computeHighlight)
    ItemType m_selectedType = ItemType::Invalid;
    QString m_selectedId;

    QTreeView* m_tree = nullptr;

private slots:
    void onSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
};
```

**Step 2: Create .cpp**

```cpp
#include "SelectableTreeView.h"

#include <QTreeView>
#include <QItemSelectionModel>

SelectableTreeView::SelectableTreeView(QWidget* parent)
    : QWidget(parent)
{
}

void SelectableTreeView::initTree(QTreeView* tree)
{
    m_tree = tree;
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SelectableTreeView::onSelectionChanged);
}

void SelectableTreeView::onSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid())
    {
        m_selectedType = ItemType::Invalid;
        m_selectedId.clear();
        emit highlightChanged();
        return;
    }

    QModelIndex nodeToUse = current;
    if (itemTypeAt(current) == ItemType::ContactDetail)
    {
        nodeToUse = current.parent();
    }

    m_selectedType = itemTypeAt(nodeToUse);
    m_selectedId = idAt(nodeToUse);
    emit highlightChanged();
}

HighlightInfo SelectableTreeView::highlightInfo() const
{
    if (m_selectedType == ItemType::Invalid || m_selectedId.isEmpty())
    {
        return {};
    }
    return computeHighlight();
}

QSet<QString> SelectableTreeView::visibleFamilyIds() const
{
    return {};  // Show all by default
}
```

**Step 3: Add to CMakeLists.txt**

Add `src/widgets/SelectableTreeView.cpp` to sources.

**Step 4: Build**

Run: `build.bat`
Expected: Clean compile

**Step 5: Commit**

```bash
git add src/widgets/SelectableTreeView.h src/widgets/SelectableTreeView.cpp CMakeLists.txt
git commit -m "feat(SelectableTreeView): add base class for tree views with selection caching"
```

---

## Part 3: Update Existing Views

### Task 7: Update EmergencyResourceView to use base class

**Files:**
- Modify: `src/widgets/EmergencyResourceView.h`
- Modify: `src/widgets/EmergencyResourceView.cpp`

**Step 1: Update header**

```cpp
#include "SelectableTreeView.h"

class EmergencyResourceView : public SelectableTreeView
{
    Q_OBJECT

public:
    explicit EmergencyResourceView(DocumentManager* documentManager,
                                   ResponseArea area,
                                   QWidget* parent = nullptr);

protected:
    // SelectableTreeView interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;
    HighlightInfo computeHighlight() const override;

private slots:
    void onTreeExpanded(const QModelIndex& index);
    // ... other slots

private:
    void setupUi();
    void updateButtonStates();
    // ... other members

    DocumentManager* m_documentManager;
    EmergencyResourceModel* m_model = nullptr;
    // m_tree is in base class
    // m_selectedType, m_selectedId are in base class
};
```

**Step 2: Update .cpp**

- Remove `onSelectionChanged` (handled by base)
- Remove `highlightInfo` (handled by base)
- Implement `itemTypeAt`, `idAt`, `computeHighlight`
- Call `initTree(m_tree)` after creating tree

**Step 3: Build**

Run: `build.bat`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/widgets/EmergencyResourceView.h src/widgets/EmergencyResourceView.cpp
git commit -m "refactor(EmergencyResourceView): derive from SelectableTreeView"
```

---

### Task 8: Update NeedsSubView to use base class

**Files:**
- Modify: `src/widgets/NeedsSubView.h`
- Modify: `src/widgets/NeedsSubView.cpp`

**Step 1: Update header**

```cpp
#include "SelectableTreeView.h"

class NeedsSubView : public SelectableTreeView
{
    // Similar pattern to EmergencyResourceView
};
```

**Step 2: Update .cpp**

- Implement `itemTypeAt`, `idAt`, `computeHighlight`
- Call `initTree(m_tree)` after creating tree
- Update `personIdAt` calls to `idAt`

**Step 3: Build**

Run: `build.bat`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/widgets/NeedsSubView.h src/widgets/NeedsSubView.cpp
git commit -m "refactor(NeedsSubView): derive from SelectableTreeView"
```

---

## Part 4: MinisteringView Refactoring

### Task 9: Create MainMinisteringView class

**Files:**
- Create: `src/widgets/MainMinisteringView.h`
- Create: `src/widgets/MainMinisteringView.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header**

```cpp
#pragma once

#include "SelectableTreeView.h"

class DocumentManager;
class MinisteringModel;

/// View for main ministering tree (districts/companionships) for one org.
class MainMinisteringView : public SelectableTreeView
{
    Q_OBJECT

public:
    explicit MainMinisteringView(DocumentManager* docManager, bool isEQ, QWidget* parent = nullptr);

protected:
    // SelectableTreeView interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;
    HighlightInfo computeHighlight() const override;

private slots:
    void onTreeExpanded(const QModelIndex& index);

private:
    void setupUi();
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;

    DocumentManager* m_documentManager;
    bool m_isEQ;
    MinisteringModel* m_model = nullptr;
};
```

**Step 2: Create .cpp**

Extract from MinisteringView, implement base class interface.

**Step 3: Add to CMakeLists.txt**

**Step 4: Build**

Run: `build.bat`
Expected: Clean compile

**Step 5: Commit**

```bash
git add src/widgets/MainMinisteringView.h src/widgets/MainMinisteringView.cpp CMakeLists.txt
git commit -m "feat(MainMinisteringView): extract view for main ministering tree"
```

---

### Task 10: Create UnassignedMinisteringView class

**Files:**
- Create: `src/widgets/UnassignedMinisteringView.h`
- Create: `src/widgets/UnassignedMinisteringView.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header**

```cpp
#pragma once

#include "SelectableTreeView.h"

class DocumentManager;
class UnassignedMinisteringModel;

/// View for unassigned ministering tree for one org.
class UnassignedMinisteringView : public SelectableTreeView
{
    Q_OBJECT

public:
    explicit UnassignedMinisteringView(DocumentManager* docManager, bool isEQ, QWidget* parent = nullptr);

    bool hasUnassigned() const;

signals:
    void visibilityChanged();

protected:
    // SelectableTreeView interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;
    HighlightInfo computeHighlight() const override;

private slots:
    void onTreeExpanded(const QModelIndex& index);
    void onTreeCollapsed(const QModelIndex& index);
    void onModelReset();

private:
    void setupUi();

    DocumentManager* m_documentManager;
    bool m_isEQ;
    UnassignedMinisteringModel* m_model = nullptr;
};
```

**Step 2: Create .cpp**

Extract from MinisteringView, implement base class interface.

**Step 3: Add to CMakeLists.txt**

**Step 4: Build**

Run: `build.bat`
Expected: Clean compile

**Step 5: Commit**

```bash
git add src/widgets/UnassignedMinisteringView.h src/widgets/UnassignedMinisteringView.cpp CMakeLists.txt
git commit -m "feat(UnassignedMinisteringView): extract view for unassigned ministering tree"
```

---

### Task 11: Refactor MinisteringView to use sub-views

**Files:**
- Modify: `src/widgets/MinisteringView.h`
- Modify: `src/widgets/MinisteringView.cpp`

**Step 1: Update header**

```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class MainMinisteringView;
class UnassignedMinisteringView;
class QTabBar;

/// Container view for ministering with EQ/RS tabs.
/// Delegates to 4 sub-view instances.
class MinisteringView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringView(DocumentManager* docManager, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onOrgToggled(int id);
    void onUnassignedVisibilityChanged();

private:
    void setupUi();

    DocumentManager* m_documentManager;
    QTabBar* m_orgTabs = nullptr;

    // Sub-views (4 instances)
    MainMinisteringView* m_eqMainView = nullptr;
    UnassignedMinisteringView* m_eqUnassignedView = nullptr;
    MainMinisteringView* m_rsMainView = nullptr;
    UnassignedMinisteringView* m_rsUnassignedView = nullptr;

    bool m_isEQ = true;
};
```

**Step 2: Update .cpp**

Create 4 sub-views, connect signals, delegate highlightInfo.

**Step 3: Build**

Run: `build.bat`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/widgets/MinisteringView.h src/widgets/MinisteringView.cpp
git commit -m "refactor(MinisteringView): delegate to 4 sub-view instances"
```

---

## Manual Testing Checklist

After all tasks complete:

1. **MainMinisteringView** - Click ContactDetail → parent highlights on map
2. **MainMinisteringView** - Click District → all families in district highlight
3. **UnassignedMinisteringView** - Click ContactDetail → parent highlights on map
4. **MinisteringView** - Switch EQ/RS tabs → correct views show
5. **EmergencyResourceView** - Click ContactDetail → parent's family highlights
6. **EmergencyResourceView** - Click Resource → all families in resource highlight
7. **NeedsSubView** - Click ContactDetail → parent's family highlights
8. **Tab switching** - Select item, switch main tabs, return → highlight preserved
