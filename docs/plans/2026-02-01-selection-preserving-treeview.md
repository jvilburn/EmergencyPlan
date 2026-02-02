# SelectionPreservingTreeView Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Create a SelectionPreservingTreeView (inherits QTreeView) that preserves selection across model rebuilds, using a BaseTreeModel interface for type-aware selection.

**Architecture:** `BaseTreeModel` is an abstract base class that extends `QAbstractItemModel` with three pure virtual methods:
- `itemTypeAt()` - semantic type for view behavior (context menus, highlights)
- `selectionKeyAt()` - unique contextual key for selection preservation
- `idAt()` - entity ID for the item (used by views for highlights, commands)

Models inherit from `BaseTreeModel` instead of `QAbstractItemModel`. This provides compile-time guarantee that any model passed to `SelectionPreservingTreeView` is a valid Qt model.

`SelectionPreservingTreeView` takes a `BaseTreeModel*` in its constructor, saves/restores selection keys across model resets.

**Family association pattern:** All tree models provide `relatedFamiliesAt(QModelIndex)` returning a `FamilyAssociation` struct (defined in BaseTreeModel.h) with:
- `relatedFamilyIds` - families connected to the selected item
- `contactPointFamilyIds` - families of contact points (ministers, team leaders, etc.)

Tree views take model pointers (not org) and delegate highlight computation to models via one-liner: `return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds}`. This keeps views thin and eliminates the need for org parameters in tree view constructors.

**Tech Stack:** Qt 6, C++17

**Reviewer Notes:**
- **Include paths:** CMakeLists.txt already includes `src/listmodels` in target_include_directories (line 343), so includes like `#include "BaseTreeModel.h"` work correctly from widgets.
- **NeedsModel selection key:** Uses `personId` which is acceptable for now (one need per person). If multiple needs per person is added later, this would need a `needId`.
- **WardListView 2-phase init:** Confirmed correct - constructor creates layout, `setup()` creates tree after model is available.
- **UnassignedTreeView expansion:** Respects user preference - if user collapsed the header, it stays collapsed after model reset. First appearance starts collapsed; user must manually expand. This is intentional.

---

## Background

The current `SelectableTreeView` mixes two concerns:
1. Selection caching/preservation (intrinsically tied to QTreeView's selection model)
2. FamilyMarkerProvider interface (composite widget concern)

This refactoring separates them properly using a model interface approach.

**Key insight:** Selection keys must be contextually unique because the same entity can appear multiple times:
- Same person as minister in two companionships
- Same person assigned to multiple emergency resources
- Same person with multiple special needs

**Detail row behavior:** Detail rows (ContactDetail, Address, Phone, Actions, etc.) return their parent's selection key. This means after a model reset, if a detail row was selected, the parent row gets selected instead. This is intentional - detail rows are informational, not primary selection targets.

SelectionPreservingTreeView intercepts selection changes and redirects detail row clicks to their parent.

**Actions row note:** The Actions row contains Edit/Delete buttons with their own click handlers. When the row itself is clicked (not a button), selection redirects to the parent Family. The buttons' click handlers are unaffected by this selection behavior.

**Selection key formats by model:**

| Model | Row Type | selectionKeyAt() Format |
|-------|----------|-------------------------|
| **FamilyTreeModel** | Family | `{familyId}` |
| | Member | `{personId}` |
| | Detail rows | parent's key |
| **NeedsModel** | Need | `{personId}` |
| | ContactDetail | parent's key |
| **EmergencyResourceModel** | Resource | `{resourceId}` |
| | Person | `{resourceId}:{personId}` |
| | ContactDetail | parent's key |
| **MinisteringModel** | District | `{districtId}` |
| | Companionship | `{groupId}` |
| | SectionHeader | `{groupId}:ministers` / `{groupId}:ministered` |
| | Minister | `{groupId}:minister:{personId}` |
| | MinisteredFamily | `{groupId}:family:{familyId}` |
| | MinisteredSister | `{groupId}:sister:{personId}` |
| | ContactDetail | parent's key |
| **UnassignedMinisteringModel** | Header | `unassigned` |
| | Family | `{familyId}` |
| | Sister | `{personId}` |

**Models to update:**

| Model | Already has itemTypeAt()? | Change needed |
|-------|---------------------------|---------------|
| EmergencyResourceModel | ✓ | Inherit BaseTreeModel, add selectionKeyAt() |
| NeedsModel | ✓ | Inherit BaseTreeModel, add selectionKeyAt() |
| MinisteringModel | ✓ | Inherit BaseTreeModel, add selectionKeyAt() |
| UnassignedMinisteringModel | ✓ | Inherit BaseTreeModel, add selectionKeyAt() |
| FamilyTreeModel | ✗ | Inherit BaseTreeModel, add both methods |

---

## Task 1: Add Family to ItemType enum and MinisteringOrg enum

**Files:**
- Modify: `src/listmodels/ItemType.h`

**Step 1: Add Family type**

Add after `ContactDetail`:
```cpp
    Family,         // FamilyTreeModel - family group
```

**Step 2: Add MinisteringOrg enum**

Add after the ItemType enum:
```cpp
/// Organization type for ministering views.
/// Used instead of bool to clarify intent at call sites.
enum class MinisteringOrg
{
    EldersQuorum,
    ReliefSociety
};
```

**Step 3: Commit**

```bash
git add src/listmodels/ItemType.h
git commit -m "feat(ItemType): add Family type and MinisteringOrg enum

Family type for FamilyTreeModel.
MinisteringOrg enum replaces bool isEQ for clarity.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 2: Create BaseTreeModel interface

**Files:**
- Create: `src/listmodels/BaseTreeModel.h`

**Step 1: Create BaseTreeModel.h**

```cpp
#pragma once

#include "ItemType.h"

#include <QAbstractItemModel>
#include <QSet>

/// Abstract base class for tree models that provide typed item access.
/// Inherits from QAbstractItemModel, adding three pure virtual methods.
/// Models inheriting from this can be used with SelectionPreservingTreeView.
///
/// Selection key formats by model (for reference):
///
///   FamilyTreeModel:
///     Family       -> {familyId}
///     Member       -> {personId}
///     Detail rows  -> parent's key
///
///   NeedsModel:
///     Need         -> {personId}
///     ContactDetail-> parent's key
///
///   EmergencyResourceModel:
///     Resource     -> {resourceId}
///     Person       -> {resourceId}:{personId}
///     ContactDetail-> parent's key
///
///   MinisteringModel:
///     District         -> {districtId}
///     Companionship    -> {groupId}
///     SectionHeader    -> {groupId}:ministers or {groupId}:ministered
///     Minister         -> {groupId}:minister:{personId}
///     MinisteredFamily -> {groupId}:family:{familyId}
///     MinisteredSister -> {groupId}:sister:{personId}
///     ContactDetail    -> parent's key
///
///   UnassignedMinisteringModel:
///     Header  -> "unassigned"
///     Family  -> {familyId}
///     Sister  -> {personId}
///
class BaseTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    using QAbstractItemModel::QAbstractItemModel;

    /// Returns the ItemType for the given index.
    /// Used by views for context menus, highlights, double-click behavior.
    virtual ItemType itemTypeAt(const QModelIndex& index) const = 0;

    /// Returns a unique selection key for the given index.
    /// Used by SelectionPreservingTreeView to save/restore selection.
    /// Keys must be unique within the model - use contextual format
    /// when the same entity can appear multiple times.
    /// Detail rows should return their parent's selection key.
    virtual QString selectionKeyAt(const QModelIndex& index) const = 0;

    /// Returns the entity ID for the given index.
    /// Used by views for highlights and commands.
    /// Returns empty string for invalid indices.
    virtual QString idAt(const QModelIndex& index) const = 0;
};

/// Domain data returned by models for family associations.
/// Views adapt this to HighlightInfo for the map.
struct FamilyAssociation
{
    QSet<QString> relatedFamilyIds;       // Families connected to this item
    QSet<QString> contactPointFamilyIds;  // Families of contact points (ministers, etc.)
};
```

**Step 2: Commit**

```bash
git add src/listmodels/BaseTreeModel.h
git commit -m "feat: add BaseTreeModel base class

Abstract base class extending QAbstractItemModel with:
- itemTypeAt(): semantic type for view behavior
- selectionKeyAt(): unique key for selection preservation
- idAt(): entity ID for highlights and commands

Inheriting from QAbstractItemModel provides compile-time
guarantee that models passed to SelectionPreservingTreeView
are valid Qt models.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 3: Create SelectionPreservingTreeView

**Files:**
- Rename: `src/widgets/SelectableTreeView.h` → `src/widgets/SelectionPreservingTreeView.h`
- Rename: `src/widgets/SelectableTreeView.cpp` → `src/widgets/SelectionPreservingTreeView.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Rename the files**

```bash
git mv src/widgets/SelectableTreeView.h src/widgets/SelectionPreservingTreeView.h
git mv src/widgets/SelectableTreeView.cpp src/widgets/SelectionPreservingTreeView.cpp
```

**Step 2: Update CMakeLists.txt**

Change:
```cmake
src/widgets/SelectableTreeView.cpp
```
To:
```cmake
src/widgets/SelectionPreservingTreeView.cpp
```

**Step 3: Rewrite SelectionPreservingTreeView.h**

```cpp
#pragma once

#include <QTreeView>

class BaseTreeModel;

/// QTreeView subclass that preserves selection across model rebuilds.
/// Saves selected item's key before model reset, restores after.
///
/// Detail row behavior: Clicking a detail row (ContactDetail, Address, etc.)
/// automatically selects the parent row instead. This is determined by
/// itemTypeAt() == ContactDetail check - detail rows are redirected to parent.
///
/// Requires a model that implements BaseTreeModel interface.
/// Views can query the model directly for itemTypeAt(currentIndex()).
class SelectionPreservingTreeView : public QTreeView
{
    Q_OBJECT

public:
    /// Construct with a model implementing BaseTreeModel.
    /// The model must also inherit from QAbstractItemModel.
    explicit SelectionPreservingTreeView(BaseTreeModel* model, QWidget* parent = nullptr);

signals:
    /// Emitted when selection changes (after any detail-to-parent redirect)
    void selectionChanged();

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onModelAboutToBeReset();
    void onModelReset();

private:
    /// Find index matching selection key, or invalid index if not found
    QModelIndex findIndex(const QString& key) const;

    /// Recursively search for matching index
    QModelIndex findIndexRecursive(const QModelIndex& parent, const QString& key) const;

    BaseTreeModel* m_typedModel;

    // Saved selection for restoration after model reset
    QString m_savedKey;
};
```

**Step 4: Rewrite SelectionPreservingTreeView.cpp**

```cpp
#include "SelectionPreservingTreeView.h"
#include "ItemType.h"
#include "BaseTreeModel.h"

#include <QItemSelectionModel>

SelectionPreservingTreeView::SelectionPreservingTreeView(BaseTreeModel* model, QWidget* parent)
    : QTreeView(parent)
    , m_typedModel(model)
{
    // BaseTreeModel inherits from QAbstractItemModel, so no cast needed
    QTreeView::setModel(model);

    connect(model, &QAbstractItemModel::modelAboutToBeReset,
            this, &SelectionPreservingTreeView::onModelAboutToBeReset);
    connect(model, &QAbstractItemModel::modelReset,
            this, &SelectionPreservingTreeView::onModelReset);
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SelectionPreservingTreeView::onCurrentChanged);
}

void SelectionPreservingTreeView::onCurrentChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (current.isValid()
        && m_typedModel->itemTypeAt(current) == ItemType::ContactDetail
        && current.parent().isValid())
    {
        // Redirect detail row clicks to parent
        setCurrentIndex(current.parent());
    }
    else
    {
        emit selectionChanged();
    }
}

void SelectionPreservingTreeView::onModelAboutToBeReset()
{
    // Save current selection before model clears
    QModelIndex current = currentIndex();
    if (current.isValid())
    {
        m_savedKey = m_typedModel->selectionKeyAt(current);
    }
    else
    {
        m_savedKey.clear();
    }
}

void SelectionPreservingTreeView::onModelReset()
{
    // Try to restore selection after model rebuild
    if (!m_savedKey.isEmpty())
    {
        QModelIndex index = findIndex(m_savedKey);
        if (index.isValid())
        {
            setCurrentIndex(index);
        }
    }

    // Clear saved state
    m_savedKey.clear();
}

QModelIndex SelectionPreservingTreeView::findIndex(const QString& key) const
{
    if (!model() || key.isEmpty())
    {
        return QModelIndex();
    }
    return findIndexRecursive(QModelIndex(), key);
}

QModelIndex SelectionPreservingTreeView::findIndexRecursive(const QModelIndex& parent, const QString& key) const
{
    int rowCount = model()->rowCount(parent);
    for (int row = 0; row < rowCount; ++row)
    {
        QModelIndex index = model()->index(row, 0, parent);
        if (m_typedModel->selectionKeyAt(index) == key)
        {
            return index;
        }

        // Search children
        QModelIndex found = findIndexRecursive(index, key);
        if (found.isValid())
        {
            return found;
        }
    }
    return QModelIndex();
}
```

**Step 5: Commit**

```bash
git add -A
git commit -m "refactor: create SelectionPreservingTreeView

Rename SelectableTreeView to SelectionPreservingTreeView. Takes
BaseTreeModel* in constructor, saves/restores selection key
across model resets. No inner wrapper classes needed.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 4: Update models to implement BaseTreeModel

**Files:**
- Modify: `src/listmodels/EmergencyResourceModel.h`
- Modify: `src/listmodels/EmergencyResourceModel.cpp`
- Modify: `src/listmodels/NeedsModel.h`
- Modify: `src/listmodels/NeedsModel.cpp`
- Modify: `src/listmodels/MinisteringModel.h`
- Modify: `src/listmodels/MinisteringModel.cpp`
- Modify: `src/listmodels/UnassignedMinisteringModel.h`
- Modify: `src/listmodels/UnassignedMinisteringModel.cpp`
- Modify: `src/listmodels/FamilyTreeModel.h`
- Modify: `src/listmodels/FamilyTreeModel.cpp`

**Step 1: Update EmergencyResourceModel.h**

Add include:
```cpp
#include "BaseTreeModel.h"
```

Change class declaration from:
```cpp
class EmergencyResourceModel : public QAbstractItemModel
```
To:
```cpp
class EmergencyResourceModel : public BaseTreeModel
```

Add `override` to existing methods and add new methods:
```cpp
    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;
```

**Step 2: Update EmergencyResourceModel.cpp**

Add selectionKeyAt() and relatedFamiliesAt() implementations:
```cpp
QString EmergencyResourceModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::Resource:
        return node->id;
    case ItemType::Person:
        return QString("%1:%2").arg(node->resourceId, node->id);
    case ItemType::ContactDetail:
        // Delegate to parent
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}

FamilyAssociation EmergencyResourceModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    const Document& doc = m_documentManager->document();
    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    switch (type)
    {
    case ItemType::Resource:
    {
        std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(id);
        if (resourceOpt)
        {
            for (const QString& personId : resourceOpt->personIds())
            {
                QString familyId = doc.familyIdForPerson(personId);
                if (!familyId.isEmpty())
                {
                    assoc.relatedFamilyIds.insert(familyId);
                }
            }
        }
        break;
    }
    case ItemType::Person:
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
        }
        break;
    }
    default:
        break;
    }

    // No contact points in emergency resources
    return assoc;
}
```

**Step 3: Update NeedsModel.h**

Add include:
```cpp
#include "BaseTreeModel.h"
```

Change class declaration from:
```cpp
class NeedsModel : public QAbstractItemModel
```
To:
```cpp
class NeedsModel : public BaseTreeModel
```

Add `override` to existing methods and add new methods:
```cpp
    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;
```

**Step 4: Update NeedsModel.cpp**

Add selectionKeyAt() and relatedFamiliesAt() implementations:
```cpp
QString NeedsModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::Person:
        // Currently one need per person (stored as Person.specialNeedNote).
        // If multiple needs per person is added later, this would need a needId.
        return node->personId;
    case ItemType::ContactDetail:
        // Delegate to parent (selecting detail row selects parent)
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}

FamilyAssociation NeedsModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    const Document& doc = m_documentManager->document();
    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    if (type == ItemType::Person)
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
        }
    }

    // No contact points in needs view
    return assoc;
}
```

**Step 5: Update MinisteringModel.h**

Add include:
```cpp
#include "BaseTreeModel.h"
```

Change class declaration from:
```cpp
class MinisteringModel : public QAbstractItemModel
```
To:
```cpp
class MinisteringModel : public BaseTreeModel
```

Add `override` to existing methods and add new methods:
```cpp
    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    /// Used by views to compute map highlights.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;
```

**Step 6: Update MinisteringModel.cpp**

Add selectionKeyAt() implementation (uses existing `companionshipIdAt()` method):
```cpp
QString MinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::District:
        return node->id;
    case ItemType::Companionship:
        return node->id;  // groupId stored in node->id
    case ItemType::SectionHeader:
        return node->id;  // Already formatted as "{groupId}:ministers" etc.
    case ItemType::Minister:
        return QString("%1:minister:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::MinisteredFamily:
        return QString("%1:family:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::MinisteredSister:
        return QString("%1:sister:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::ContactDetail:
        // Delegate to parent (selecting detail row selects parent)
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}
```

Add relatedFamiliesAt() implementation (move highlight logic from view to model):
```cpp
FamilyAssociation MinisteringModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    const Document& doc = m_documentManager->document();
    const auto& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    switch (type)
    {
    case ItemType::District:
        if (districts.contains(id))
        {
            const MinisteringDistrict& district = districts[id];
            for (const QString& groupId : district.groupIds())
            {
                if (groups.contains(groupId))
                {
                    const MinisteringGroup& group = groups[groupId];
                    if (isEQ())
                    {
                        assoc.relatedFamilyIds.unite(group.familyIds());
                    }
                    else
                    {
                        assoc.relatedFamilyIds.unite(familyIdsForPersons(group.ministeredPersonIds()));
                    }
                    assoc.contactPointFamilyIds.unite(familyIdsForPersons(group.ministerIds()));
                }
            }
        }
        break;

    case ItemType::Companionship:
        if (groups.contains(id))
        {
            const MinisteringGroup& group = groups[id];
            if (isEQ())
            {
                assoc.relatedFamilyIds = group.familyIds();
            }
            else
            {
                assoc.relatedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
            }
            assoc.contactPointFamilyIds = familyIdsForPersons(group.ministerIds());
        }
        break;

    case ItemType::SectionHeader:
    {
        int colonPos = id.lastIndexOf(':');
        if (colonPos > 0)
        {
            QString compId = id.left(colonPos);
            QString sectionType = id.mid(colonPos + 1);

            if (groups.contains(compId))
            {
                const MinisteringGroup& group = groups[compId];
                if (sectionType == "ministers")
                {
                    QSet<QString> ministerFamilies = familyIdsForPersons(group.ministerIds());
                    assoc.relatedFamilyIds = ministerFamilies;
                    assoc.contactPointFamilyIds = ministerFamilies;
                }
                else
                {
                    if (isEQ())
                    {
                        assoc.relatedFamilyIds = group.familyIds();
                    }
                    else
                    {
                        assoc.relatedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
                    }
                }
            }
        }
        break;
    }

    case ItemType::Minister:
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
            assoc.contactPointFamilyIds.insert(familyId);
        }
        break;
    }

    case ItemType::MinisteredFamily:
        assoc.relatedFamilyIds.insert(id);
        break;

    case ItemType::MinisteredSister:
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
        }
        break;
    }

    default:
        break;
    }

    return assoc;
}
```

Note: This also requires:
1. Changing constructor parameter from `bool isEQ` to `MinisteringOrg org`
2. Changing member from `m_isEQ` to `m_org`
3. Adding private helper methods:
```cpp
private:
    bool isEQ() const { return m_org == MinisteringOrg::EldersQuorum; }
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;
```

Implementation of familyIdsForPersons:
```cpp
QSet<QString> MinisteringModel::familyIdsForPersons(const QSet<QString>& personIds) const
{
    QSet<QString> familyIds;
    const Document& doc = m_documentManager->document();
    for (const QString& personId : personIds)
    {
        QString familyId = doc.familyIdForPerson(personId);
        if (!familyId.isEmpty())
        {
            familyIds.insert(familyId);
        }
    }
    return familyIds;
}
```

**Step 7: Update UnassignedMinisteringModel.h**

Add include:
```cpp
#include "BaseTreeModel.h"
```

Change class declaration from:
```cpp
class UnassignedMinisteringModel : public QAbstractItemModel
```
To:
```cpp
class UnassignedMinisteringModel : public BaseTreeModel
```

Add `override` to existing methods and add new methods:
```cpp
    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;
```

**Step 8: Update UnassignedMinisteringModel.cpp**

Add selectionKeyAt() implementation:
```cpp
QString UnassignedMinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::UnassignedHeader:
        return QStringLiteral("unassigned");
    case ItemType::MinisteredFamily:
        return node->id;  // Family ID - only appears once in unassigned list
    case ItemType::MinisteredSister:
        return node->id;  // Person ID - only appears once in unassigned list
    default:
        return QString();
    }
}
```

Add relatedFamiliesAt() implementation:
```cpp
FamilyAssociation UnassignedMinisteringModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    switch (type)
    {
    case ItemType::UnassignedHeader:
        if (isEQ())
        {
            assoc.relatedFamilyIds = unassignedFamilyIds();
        }
        else
        {
            assoc.relatedFamilyIds = familyIdsForPersons(unassignedSisterIds());
        }
        break;
    case ItemType::MinisteredFamily:
        assoc.relatedFamilyIds.insert(id);
        break;
    case ItemType::MinisteredSister:
    {
        QString familyId = m_documentManager->document().familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
        }
        break;
    }
    default:
        break;
    }

    // No contact points in unassigned list
    return assoc;
}
```

Note: This also requires:
1. Changing constructor parameter from `bool isEQ` to `MinisteringOrg org`
2. Changing member from `m_isEQ` to `m_org`
3. Adding private helper methods:
```cpp
private:
    bool isEQ() const { return m_org == MinisteringOrg::EldersQuorum; }
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;
    QSet<QString> unassignedFamilyIds() const;
    QSet<QString> unassignedSisterIds() const;
```

Implementation of helper methods:
```cpp
QSet<QString> UnassignedMinisteringModel::familyIdsForPersons(const QSet<QString>& personIds) const
{
    QSet<QString> familyIds;
    const Document& doc = m_documentManager->document();
    for (const QString& personId : personIds)
    {
        QString familyId = doc.familyIdForPerson(personId);
        if (!familyId.isEmpty())
        {
            familyIds.insert(familyId);
        }
    }
    return familyIds;
}

QSet<QString> UnassignedMinisteringModel::unassignedFamilyIds() const
{
    // Collect all family IDs from tree nodes
    QSet<QString> familyIds;
    if (!m_headerNode)
    {
        return familyIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredFamily)
        {
            familyIds.insert(child->id);
        }
    }
    return familyIds;
}

QSet<QString> UnassignedMinisteringModel::unassignedSisterIds() const
{
    // Collect all person IDs from tree nodes
    QSet<QString> personIds;
    if (!m_headerNode)
    {
        return personIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredSister)
        {
            personIds.insert(child->id);
        }
    }
    return personIds;
}
```

**Step 9: Update FamilyTreeModel.h**

Add includes:
```cpp
#include "BaseTreeModel.h"
```

Change class declaration from:
```cpp
class FamilyTreeModel : public QAbstractItemModel
```
To:
```cpp
class FamilyTreeModel : public BaseTreeModel
```

Add new method declarations:
```cpp
    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;
```

**Step 10: Update FamilyTreeModel.cpp**

Add implementations:
```cpp
ItemType FamilyTreeModel::itemTypeAt(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return ItemType::Invalid;
    }

    switch (rowTypeAt(index))
    {
    case RowType::Family:
        return ItemType::Family;
    case RowType::Member:
        return ItemType::Person;
    case RowType::MemberDetail:
    case RowType::Address:
    case RowType::Phone:
    case RowType::Actions:
        return ItemType::ContactDetail;
    default:
        return ItemType::Invalid;
    }
}

QString FamilyTreeModel::selectionKeyAt(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QString();
    }

    switch (rowTypeAt(index))
    {
    case RowType::Family:
    case RowType::Member:
        return idAt(index);
    case RowType::MemberDetail:
    case RowType::Address:
    case RowType::Phone:
    case RowType::Actions:
        // Delegate to parent
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}

QString FamilyTreeModel::idAt(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QString();
    }

    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case RowType::Family:
        return familyIdAt(index);
    case RowType::Member:
    {
        // Get family ID and member index
        QString familyId = familyIdAt(index);
        if (familyId.isEmpty() || node->memberIndex < 0)
        {
            return QString();
        }

        // Look up the person
        const auto& families = m_documentManager->document().families();
        if (!families.contains(familyId))
        {
            return QString();
        }

        const Family& family = families[familyId];
        if (node->memberIndex >= family.members().size())
        {
            return QString();
        }

        return family.members().at(node->memberIndex).id();
    }
    default:
        return QString();
    }
}
```

**Step 11: Build and verify**

Run: `build.bat`
Expected: Build fails (views still reference old SelectableTreeView)

**Step 12: Commit**

```bash
git add src/listmodels/*.h src/listmodels/*.cpp
git commit -m "feat: models inherit from BaseTreeModel

Change base class from QAbstractItemModel to BaseTreeModel.
BaseTreeModel extends QAbstractItemModel with:
- itemTypeAt(): semantic type for view behavior
- selectionKeyAt(): contextual unique key for selection

Selection keys use contextual format to handle cases where
the same entity appears multiple times (e.g., minister in
two companionships uses '{groupId}:minister:{personId}').

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 5: Convert EmergencyResourceView

**Goal:** EmergencyResourceView takes model pointer. Parent (ResourcesView) creates model with ResponseArea.

**Files:**
- Modify: `src/widgets/EmergencyResourceView.h`
- Modify: `src/widgets/EmergencyResourceView.cpp`
- Modify: `src/widgets/ResourcesView.h`
- Modify: `src/widgets/ResourcesView.cpp`

**Step 1: Update EmergencyResourceView.h**

Replace the entire file:

```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class EmergencyResourceModel;
class QModelIndex;
class QPushButton;
class SelectionPreservingTreeView;

/// EmergencyResourceView displays a 3-level tree of resources.
/// Takes model pointer - parent creates model with ResponseArea.
/// Includes a toolbar with Add, Edit, Delete buttons.
class EmergencyResourceView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit EmergencyResourceView(EmergencyResourceModel* model,
                                    DocumentManager* documentManager,
                                    QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandResources();
    void selectPeopleFromContextMenu();
    void removePersonFromContextMenu();

private:
    void updateButtonStates();

    void addResource();
    void editResource();
    void deleteResource();
    void showSelectPeopleDialog(const QString& resourceId);
    void removePersonFromResource(const QString& resourceId, const QString& personId);

    QString selectedResourceId() const;

    DocumentManager* m_documentManager;  // Needed for commands
    EmergencyResourceModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    QString m_contextResourceId;
    QString m_contextPersonId;
};
```

**Step 2: Update EmergencyResourceView.cpp**

Remove include of `SelectableTreeView.h`, add:
```cpp
#include "SelectionPreservingTreeView.h"
```

Update constructor signature and body:
```cpp
EmergencyResourceView::EmergencyResourceView(EmergencyResourceModel* model,
                                              DocumentManager* documentManager,
                                              QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    // ... rest of tree setup
}
```

Replace selection connection:
```cpp
    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &EmergencyResourceView::onSelectionChanged);
```

Update onSelectionChanged - remove parameters and update body:
```cpp
void EmergencyResourceView::onSelectionChanged()
{
    updateButtonStates();
    emit highlightChanged();
}
```

Remove `itemTypeAt()` and `idAt()` method implementations (model provides these now via BaseTreeModel interface).

**Step 3: Update ResourcesView.h**

Add forward declaration and model members:
```cpp
class EmergencyResourceModel;

// In private section, add model pointers:
    EmergencyResourceModel* m_medicalModel;
    EmergencyResourceModel* m_commsModel;
    EmergencyResourceModel* m_recoveryModel;
```

**Step 4: Update ResourcesView.cpp**

Add include:
```cpp
#include "EmergencyResourceModel.h"
```

Update constructor to create models, then pass to views:
```cpp
// Create models with ResponseArea
m_medicalModel = new EmergencyResourceModel(documentManager, ResponseArea::Medical, this);
m_commsModel = new EmergencyResourceModel(documentManager, ResponseArea::Communications, this);
m_recoveryModel = new EmergencyResourceModel(documentManager, ResponseArea::Recovery, this);

// Pass models to views
m_medicalView = new EmergencyResourceView(m_medicalModel, documentManager, this);
m_commsView = new EmergencyResourceView(m_commsModel, documentManager, this);
m_recoveryView = new EmergencyResourceView(m_recoveryModel, documentManager, this);
```

**Step 5: Update EmergencyResourceView.cpp highlightInfo()**

Delegate to model:
```cpp
HighlightInfo EmergencyResourceView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}
```

**Step 6: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 7: Commit**

```bash
git add src/widgets/EmergencyResourceView.h src/widgets/EmergencyResourceView.cpp \
        src/widgets/ResourcesView.h src/widgets/ResourcesView.cpp
git commit -m "refactor(EmergencyResourceView): take model pointer

EmergencyResourceView takes EmergencyResourceModel* in constructor.
ResourcesView creates models with ResponseArea, passes to views.
Uses SelectionPreservingTreeView for selection preservation.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 6: Convert NeedsSubView

**Goal:** NeedsSubView takes model pointer. Parent (NeedsView) creates model.

**Files:**
- Modify: `src/widgets/NeedsSubView.h`
- Modify: `src/widgets/NeedsSubView.cpp`
- Modify: `src/widgets/NeedsView.h` (parent creates model)
- Modify: `src/widgets/NeedsView.cpp`

**Step 1: Update NeedsSubView.h**

Update constructor and members:
```cpp
class NeedsModel;
class SelectionPreservingTreeView;

class NeedsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    /// Takes model pointer - parent creates model.
    explicit NeedsSubView(NeedsModel* model,
                           DocumentManager* documentManager,
                           QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;
    // ...

private:
    DocumentManager* m_documentManager;  // Needed for commands
    NeedsModel* m_model;
    SelectionPreservingTreeView* m_tree;
};
```

**Step 2: Update NeedsSubView.cpp**

Add include:
```cpp
#include "SelectionPreservingTreeView.h"
```

Update constructor:
```cpp
NeedsSubView::NeedsSubView(NeedsModel* model,
                            DocumentManager* documentManager,
                            QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    // ... rest of tree setup
}
```

Replace selection connection:
```cpp
    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &NeedsSubView::onSelectionChanged);
```

Update onSelectionChanged:
```cpp
void NeedsSubView::onSelectionChanged()
{
    emit highlightChanged();
}
```

Add highlightInfo() that delegates to model:
```cpp
HighlightInfo NeedsSubView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}
```

**Step 3: Update NeedsView (parent)**

In NeedsView, create the model and pass to NeedsSubView:
```cpp
// NeedsView creates model
m_model = new NeedsModel(documentManager, this);
m_subView = new NeedsSubView(m_model, documentManager, this);
```

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/widgets/NeedsSubView.h src/widgets/NeedsSubView.cpp \
        src/widgets/NeedsView.h src/widgets/NeedsView.cpp
git commit -m "refactor(NeedsSubView): take model pointer

NeedsSubView takes NeedsModel* - parent creates model.
Selection is now preserved across model rebuilds.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 7a: Create MinisteringTreeView and UnassignedTreeView

**Goal:** Create the two leaf tree view widgets. Each takes a model pointer (not org) and delegates highlight computation to the model.

**Architecture (this task creates the leaf widgets):**
```
MinisteringView (container, delegates to current tab)         ← Task 7b
├── MinisteringTabView (EQ, creates models, owns tree views)  ← Task 7b
│   ├── MinisteringTreeView (thin: tree + delegate to model)  ← THIS TASK
│   └── UnassignedTreeView (thin: tree + delegate to model)   ← THIS TASK
└── MinisteringTabView (RS, creates models, owns tree views)  ← Task 7b
    ├── MinisteringTreeView (thin: tree + delegate to model)  ← THIS TASK
    └── UnassignedTreeView (thin: tree + delegate to model)   ← THIS TASK
```

**Files:**
- Create: `src/widgets/MinisteringTreeView.h`
- Create: `src/widgets/MinisteringTreeView.cpp`
- Create: `src/widgets/UnassignedTreeView.h`
- Create: `src/widgets/UnassignedTreeView.cpp`
- Modify: `CMakeLists.txt`

### Step 1: Create MinisteringTreeView

Takes a `MinisteringModel*` (not org). Delegates `highlightInfo()` to `m_model->relatedFamiliesAt()`.

**MinisteringTreeView.h:**
```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class MinisteringModel;
class SelectionPreservingTreeView;
class QModelIndex;

/// Tree view for assigned ministering (districts, companionships, ministers, families).
/// Takes model pointer - delegates highlightInfo() to model's relatedFamiliesAt().
class MinisteringTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringTreeView(MinisteringModel* model, QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    void expandDistricts();

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);

private:
    MinisteringModel* m_model;
    SelectionPreservingTreeView* m_tree;
};
```

**MinisteringTreeView.cpp:**
```cpp
#include "MinisteringTreeView.h"
#include "ItemType.h"
#include "MinisteringModel.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>

MinisteringTreeView::MinisteringTreeView(MinisteringModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(16);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &MinisteringTreeView::onSelectionChanged);
    connect(m_tree, &QTreeView::expanded,
            this, &MinisteringTreeView::onTreeExpanded);
}

void MinisteringTreeView::onSelectionChanged()
{
    emit highlightChanged();
}

void MinisteringTreeView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::Companionship)
    {
        // Expand section headers when companionship is expanded
        int rowCount = m_model->rowCount(index);
        for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex childIndex = m_model->index(i, 0, index);
            ItemType childType = m_model->itemTypeAt(childIndex);
            if (childType == ItemType::SectionHeader)
            {
                m_tree->expand(childIndex);
            }
        }
    }
    else if (type == ItemType::Minister
             || type == ItemType::MinisteredFamily
             || type == ItemType::MinisteredSister)
    {
        // Load contact info on expand
        m_model->loadContactDetails(index);
    }
}

void MinisteringTreeView::expandDistricts()
{
    m_tree->expandToDepth(0);
}

HighlightInfo MinisteringTreeView::highlightInfo() const
{
    // Delegate to model - it computes family associations
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<QString> MinisteringTreeView::visibleFamilyIds() const
{
    return {};  // Show all
}
```

### Step 2: Create UnassignedTreeView

Takes an `UnassignedMinisteringModel*` (not org). Delegates `highlightInfo()` to `m_model->relatedFamiliesAt()`.
**NOTE: HEIGHT IS HANDLED BY TAB VIEW.** This class emits `headerExpansionChanged(bool)` for the tab view to manage height.

**UnassignedTreeView.h:**
```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class UnassignedMinisteringModel;
class SelectionPreservingTreeView;
class QModelIndex;

/// Tree view for unassigned families/sisters.
/// Takes model pointer - delegates highlightInfo() to model's relatedFamiliesAt().
/// Preserves tree node expansion state across model resets.
/// NOTE: Widget HEIGHT is managed by MinisteringTabView via headerExpansionChanged signal.
class UnassignedTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit UnassignedTreeView(UnassignedMinisteringModel* model, QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    bool hasUnassigned() const;

signals:
    void highlightChanged();
    void unassignedCountChanged();  // Emitted after model reset (for visibility updates)
    void headerExpansionChanged(bool isExpanded);  // For tab view to manage height

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);
    void onTreeCollapsed(const QModelIndex& index);
    void onModelAboutToBeReset();
    void onModelReset();

private:
    UnassignedMinisteringModel* m_model;
    SelectionPreservingTreeView* m_tree;
    bool m_wasExpanded = false;  // Track tree node expansion state for preservation
};
```

**UnassignedTreeView.cpp:**
```cpp
#include "UnassignedTreeView.h"
#include "ItemType.h"
#include "UnassignedMinisteringModel.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>

UnassignedTreeView::UnassignedTreeView(UnassignedMinisteringModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(16);
    // NOTE: Height is managed by MinisteringTabView via headerExpansionChanged signal

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &UnassignedTreeView::onSelectionChanged);
    connect(m_tree, &QTreeView::expanded,
            this, &UnassignedTreeView::onTreeExpanded);
    connect(m_tree, &QTreeView::collapsed,
            this, &UnassignedTreeView::onTreeCollapsed);
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset,
            this, &UnassignedTreeView::onModelAboutToBeReset);
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &UnassignedTreeView::onModelReset);
}

void UnassignedTreeView::onSelectionChanged()
{
    emit highlightChanged();
}

void UnassignedTreeView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::UnassignedHeader)
    {
        emit headerExpansionChanged(true);
    }
    else if (type == ItemType::MinisteredFamily || type == ItemType::MinisteredSister)
    {
        m_model->loadContactDetails(index);
    }
}

void UnassignedTreeView::onTreeCollapsed(const QModelIndex& index)
{
    if (m_model->itemTypeAt(index) == ItemType::UnassignedHeader)
    {
        emit headerExpansionChanged(false);
    }
}

void UnassignedTreeView::onModelAboutToBeReset()
{
    // Save tree node expansion state - header is row 0 at root
    QModelIndex headerIndex = m_model->index(0, 0);
    m_wasExpanded = headerIndex.isValid() && m_tree->isExpanded(headerIndex);
}

void UnassignedTreeView::onModelReset()
{
    // Restore tree node expansion state
    QModelIndex headerIndex = m_model->index(0, 0);
    if (headerIndex.isValid() && m_wasExpanded)
    {
        m_tree->expand(headerIndex);
        // expand() triggers onTreeExpanded which emits headerExpansionChanged
    }

    // Notify parent that unassigned count may have changed (for visibility updates)
    emit unassignedCountChanged();
}

bool UnassignedTreeView::hasUnassigned() const
{
    return m_model->hasUnassigned();
}

HighlightInfo UnassignedTreeView::highlightInfo() const
{
    // Delegate to model - it computes family associations
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<QString> UnassignedTreeView::visibleFamilyIds() const
{
    return {};  // Show all
}
```

### Step 3: Update CMakeLists.txt

Add:
```cmake
src/widgets/MinisteringTreeView.cpp
src/widgets/UnassignedTreeView.cpp
```

### Step 4: Build and verify

Run: `build.bat`
Expected: Build fails (MinisteringTabView doesn't exist yet, MinisteringView still references old structure)

### Step 5: Commit

```bash
git add src/widgets/MinisteringTreeView.h src/widgets/MinisteringTreeView.cpp \
        src/widgets/UnassignedTreeView.h src/widgets/UnassignedTreeView.cpp \
        CMakeLists.txt
git commit -m "feat: add MinisteringTreeView and UnassignedTreeView

New leaf widgets that own SelectionPreservingTreeView + model.
Each implements FamilyMarkerProvider with type-specific highlight logic.

UnassignedTreeView preserves header expansion state across model resets
and emits headerExpansionChanged for parent to manage height.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 7b: Create MinisteringTabView and refactor MinisteringView

**Goal:** Create the tab view that creates models and passes them to tree views, then simplify MinisteringView to own two tab views.

**Files:**
- Create: `src/widgets/MinisteringTabView.h`
- Create: `src/widgets/MinisteringTabView.cpp`
- Modify: `src/widgets/MinisteringView.h`
- Modify: `src/widgets/MinisteringView.cpp`
- Modify: `CMakeLists.txt`

### Step 1: Create MinisteringTabView

Creates models with org, passes to tree views. Delegates FamilyMarkerProvider to whichever sub-view last emitted highlightChanged.
**IMPORTANT: HEIGHT IS HANDLED BY TAB VIEW.** This class manages the height of UnassignedTreeView by connecting to its `headerExpansionChanged` signal.

**MinisteringTabView.h:**
```cpp
#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class DocumentManager;
class MinisteringModel;
class UnassignedMinisteringModel;
class MinisteringTreeView;
class UnassignedTreeView;

/// Tab content - creates models, owns tree views for one org.
/// Delegates highlightInfo() to whichever tree view has selection.
/// IMPORTANT: Manages UnassignedTreeView height (connects to headerExpansionChanged).
class MinisteringTabView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringTabView(DocumentManager* documentManager,
                                 MinisteringOrg org,
                                 QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    void expandDistricts();

signals:
    void highlightChanged();

private slots:
    void onMainHighlightChanged();
    void onUnassignedHighlightChanged();
    void updateUnassignedVisibility();
    void onHeaderExpansionChanged(bool isExpanded);

private:
    int collapsedTreeHeight() const;
    int expandedTreeHeight() const;

    // Models owned by tab view
    MinisteringModel* m_mainModel;
    UnassignedMinisteringModel* m_unassignedModel;

    // Views take model pointers
    MinisteringTreeView* m_mainView;
    UnassignedTreeView* m_unassignedView;
    FamilyMarkerProvider* m_activeProvider = nullptr;
};
```

**MinisteringTabView.cpp:**
```cpp
#include "MinisteringTabView.h"
#include "MinisteringModel.h"
#include "UnassignedMinisteringModel.h"
#include "MinisteringTreeView.h"
#include "UnassignedTreeView.h"

#include <QFontMetrics>
#include <QVBoxLayout>

MinisteringTabView::MinisteringTabView(DocumentManager* documentManager,
                                         MinisteringOrg org,
                                         QWidget* parent)
    : QWidget(parent)
{
    // Create models with org - models own the org context
    m_mainModel = new MinisteringModel(documentManager, org, this);
    m_unassignedModel = new UnassignedMinisteringModel(documentManager, org, this);

    // Pass models to tree views - views don't need org
    m_mainView = new MinisteringTreeView(m_mainModel, this);
    m_unassignedView = new UnassignedTreeView(m_unassignedModel, this);

    // Set initial collapsed height
    m_unassignedView->setFixedHeight(collapsedTreeHeight());

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(m_unassignedView);
    layout->addWidget(m_mainView, 1);

    // Highlight delegation
    connect(m_mainView, &MinisteringTreeView::highlightChanged,
            this, &MinisteringTabView::onMainHighlightChanged);
    connect(m_unassignedView, &UnassignedTreeView::highlightChanged,
            this, &MinisteringTabView::onUnassignedHighlightChanged);

    // Visibility (hide when count == 0)
    connect(m_unassignedView, &UnassignedTreeView::unassignedCountChanged,
            this, &MinisteringTabView::updateUnassignedVisibility);

    // HEIGHT IS HANDLED BY TAB VIEW
    connect(m_unassignedView, &UnassignedTreeView::headerExpansionChanged,
            this, &MinisteringTabView::onHeaderExpansionChanged);

    updateUnassignedVisibility();
}

void MinisteringTabView::onMainHighlightChanged()
{
    m_activeProvider = m_mainView;
    emit highlightChanged();
}

void MinisteringTabView::onUnassignedHighlightChanged()
{
    m_activeProvider = m_unassignedView;
    emit highlightChanged();
}

void MinisteringTabView::onHeaderExpansionChanged(bool isExpanded)
{
    m_unassignedView->setFixedHeight(isExpanded ? expandedTreeHeight() : collapsedTreeHeight());
}

int MinisteringTabView::collapsedTreeHeight() const
{
    // Height for single row (header only) + margins
    // Using font metrics rather than sizeHintForRow() since tree may be empty
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;  // Text height + tree item padding
    return rowHeight + 16;  // Widget margins
}

int MinisteringTabView::expandedTreeHeight() const
{
    // Limit to ~8 rows to avoid taking too much vertical space from main tree.
    // Using font metrics rather than sizeHintForRow() because we want a fixed
    // maximum regardless of actual content count.
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;
    return rowHeight * 8 + 16;
}

HighlightInfo MinisteringTabView::highlightInfo() const
{
    if (m_activeProvider)
    {
        return m_activeProvider->highlightInfo();
    }
    return {};
}

QSet<QString> MinisteringTabView::visibleFamilyIds() const
{
    return {};  // Show all
}

void MinisteringTabView::expandDistricts()
{
    m_mainView->expandDistricts();
}

void MinisteringTabView::updateUnassignedVisibility()
{
    m_unassignedView->setVisible(m_unassignedView->hasUnassigned());
}
```

### Step 2: Update MinisteringView

Simplify to container that owns two MinisteringTabViews.

**MinisteringView.h:**
```cpp
#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class DocumentManager;
class MinisteringTabView;
class QStackedWidget;
class QTabBar;

/// Container for EQ/RS ministering tabs. Delegates to current tab.
class MinisteringView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringView(DocumentManager* docManager, QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onTabChanged(int index);

private:
    MinisteringTabView* currentTabView() const;

    QTabBar* m_tabBar;
    QStackedWidget* m_stack;
    MinisteringTabView* m_eqView;
    MinisteringTabView* m_rsView;
    MinisteringOrg m_currentOrg = MinisteringOrg::EldersQuorum;
};
```

**MinisteringView.cpp:**
```cpp
#include "MinisteringView.h"
#include "MinisteringTabView.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

MinisteringView::MinisteringView(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Tab bar
    m_tabBar = new QTabBar();
    m_tabBar->addTab(tr("Elders Quorum"));
    m_tabBar->addTab(tr("Relief Society"));
    layout->addWidget(m_tabBar);

    // Tab views
    m_eqView = new MinisteringTabView(docManager, MinisteringOrg::EldersQuorum, this);
    m_rsView = new MinisteringTabView(docManager, MinisteringOrg::ReliefSociety, this);

    // Stack for switching
    m_stack = new QStackedWidget();
    m_stack->addWidget(m_eqView);
    m_stack->addWidget(m_rsView);
    layout->addWidget(m_stack, 1);

    // Connections
    connect(m_tabBar, &QTabBar::currentChanged,
            this, &MinisteringView::onTabChanged);

    connect(m_eqView, &MinisteringTabView::highlightChanged,
            this, &MinisteringView::highlightChanged);
    connect(m_rsView, &MinisteringTabView::highlightChanged,
            this, &MinisteringView::highlightChanged);

    // Initial state
    m_eqView->expandDistricts();
    m_rsView->expandDistricts();
}

void MinisteringView::onTabChanged(int index)
{
    m_currentOrg = (index == 0) ? MinisteringOrg::EldersQuorum : MinisteringOrg::ReliefSociety;
    m_stack->setCurrentIndex(index);
    emit highlightChanged();
}

HighlightInfo MinisteringView::highlightInfo() const
{
    return currentTabView()->highlightInfo();
}

QSet<QString> MinisteringView::visibleFamilyIds() const
{
    return {};  // Show all
}

MinisteringTabView* MinisteringView::currentTabView() const
{
    return (m_currentOrg == MinisteringOrg::EldersQuorum) ? m_eqView : m_rsView;
}
```

### Step 3: Update CMakeLists.txt

Add:
```cmake
src/widgets/MinisteringTabView.cpp
```

### Step 4: Build and verify

Run: `build.bat`
Expected: Build succeeds

### Step 5: Commit

```bash
git add src/widgets/MinisteringTabView.h src/widgets/MinisteringTabView.cpp \
        src/widgets/MinisteringView.h src/widgets/MinisteringView.cpp CMakeLists.txt
git commit -m "refactor(MinisteringView): add MinisteringTabView, simplify container

MinisteringTabView composes MinisteringTreeView + UnassignedTreeView,
delegates highlightInfo() to active tree view, manages unassigned
widget height via headerExpansionChanged signal.

MinisteringView simplified to own two tab views (EQ/RS), delegates
to current tab.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 8: Convert WardListView

**Goal:** WardListView takes model pointer. SearchField and Filter move to parent.

The parent widget (e.g., sidebar or main window):
1. Creates SearchField
2. Creates Filter (connected to SearchField)
3. Creates FamilyTreeModel with Filter
4. Passes model to WardListView

WardListView becomes a thin tree view wrapper like the ministering views.

**Files:**
- Modify: `src/widgets/WardListView.h`
- Modify: `src/widgets/WardListView.cpp`
- Modify: Parent widget that owns WardListView

**Step 1: Update WardListView.h**

Simplify to take model pointer:
```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class FamilyTreeModel;
class SelectionPreservingTreeView;
class QModelIndex;

/// Tree view for ward family list.
/// Takes model pointer - parent creates model with Filter.
class WardListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit WardListView(FamilyTreeModel* model,
                           DocumentManager* documentManager,
                           QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    QString selectedFamilyId() const;
    void setSelectedFamilyId(const QString& id);

signals:
    void highlightChanged();
    void visibleFamiliesChanged(const QStringList& familyIds);
    void editFamilyRequested(const QString& familyId);
    void deleteFamilyRequested(const QString& familyId);

private slots:
    void onSelectionChanged();
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);
    void onModelReset();
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onRowsInserted(const QModelIndex& parent, int first, int last);

private:
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const QString& familyId);

    DocumentManager* m_documentManager;
    FamilyTreeModel* m_model;
    SelectionPreservingTreeView* m_treeView;
    QHash<QString, QWidget*> m_actionWidgets;
};
```

**Step 2: Update WardListView.cpp**

Remove SearchField, Filter creation. Take model in constructor:
```cpp
#include "WardListView.h"
#include "SelectionPreservingTreeView.h"
#include "FamilyTreeModel.h"
// ... other includes

WardListView::WardListView(FamilyTreeModel* model,
                            DocumentManager* documentManager,
                            QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_model(model)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Tree view (no search field - parent owns that)
    m_treeView = new SelectionPreservingTreeView(m_model, this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAnimated(true);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_treeView, 1);

    connect(m_treeView, &SelectionPreservingTreeView::selectionChanged,
            this, &WardListView::onSelectionChanged);
    // ... other connections
}
```

Remove `setup()` method - no longer needed.

**Step 3: Update parent widget**

Parent creates SearchField, Filter, Model, and WardListView:
```cpp
// Parent owns search field + filter + model
m_searchField = new SearchField(this);
m_searchField->setPlaceholderText(tr("Search families..."));
layout->addWidget(m_searchField);

m_filter = new Filter(this);
connect(m_searchField, &SearchField::searchTextChanged,
        m_filter, &Filter::setSearchText);

m_familyModel = new FamilyTreeModel(documentManager, m_filter, this);
m_wardListView = new WardListView(m_familyModel, documentManager, this);
layout->addWidget(m_wardListView, 1);
```

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/widgets/WardListView.h src/widgets/WardListView.cpp
git commit -m "refactor(WardListView): take model pointer

WardListView takes FamilyTreeModel* - parent creates SearchField,
Filter, and model. Selection preserved across filter changes.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 9: Cleanup

**Step 1: Verify file rename completed**

Confirm Task 3's `git mv` succeeded - `SelectableTreeView.h/.cpp` should no longer exist, only `SelectionPreservingTreeView.h/.cpp`.

**Step 2: Search for old references**

Search codebase for any remaining references to:
- `SelectableTreeView`
- Old selection patterns

**Step 3: Fix any remaining references**

Update to use new patterns.

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit if needed**

---

## Task 10: Final Verification

**Step 1: Full rebuild**

Run: `build.bat`
Expected: Build succeeds

**Step 2: Manual testing**

1. **Emergency Resources** - Select items, edit, verify selection preserved
2. **Special Needs** - Select, add/edit/delete, verify selection preserved
3. **Ministering** - Switch EQ/RS, select items, verify highlights and preservation
4. **Ward List** - Select families, search, verify selection preserved

**Step 3: Commit any fixes**

---

## Summary

This refactoring:

1. **BaseTreeModel interface** - Three methods:
   - `itemTypeAt()` - semantic type for view behavior (query model directly)
   - `selectionKeyAt()` - contextual unique key for selection preservation
   - `idAt()` - entity ID for highlights and commands
2. **FamilyAssociation struct** - Domain data from models:
   - `relatedFamilyIds` - families connected to selected item
   - `contactPointFamilyIds` - families of contact points (ministers, team leaders)
   - Models provide `relatedFamiliesAt()`, views adapt to `HighlightInfo`
3. **Contextual selection keys** - Handle same entity appearing multiple times
4. **SelectionPreservingTreeView** - Minimal: saves/restores selection key, emits signal
   - No type caching - views query `m_model->itemTypeAt(m_tree->currentIndex())`
5. **Views use it directly** - `new SelectionPreservingTreeView(m_model, this)`
6. **No inner wrapper classes** - Cleaner, less boilerplate
7. **MinisteringOrg enum** - Replaces `bool isEQ` for clarity at call sites
8. **Views take model pointers** - All tree views take model pointers:
   - EmergencyResourceView, NeedsSubView, WardListView, MinisteringTreeView, UnassignedTreeView
   - Parents create models with configuration (ResponseArea, Filter, MinisteringOrg)
9. **SearchField moves to parent** - WardListView's SearchField+Filter move to parent, allowing consistent model-pointer pattern
