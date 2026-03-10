# WardListDialog Improvements Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix multi-select UX (checkboxes instead of ctrl-click), fix cancel-vs-empty ambiguity, and add contextual dialog titles.

**Architecture:** Add checkbox support to PersonTreeModel and FamilyTreeModel via a checked-IDs set and `Qt::CheckStateRole`/`Qt::ItemIsUserCheckable`. Update WardListDialog convenience methods with new return types and title parameter. Update call sites.

**Tech Stack:** Qt 6 / C++17, QAbstractItemModel checkbox roles

---

### Task 1: Add checkbox support to PersonTreeModel

**Files:**
- Modify: `src/listmodels/PersonTreeModel.h`
- Modify: `src/listmodels/PersonTreeModel.cpp`

**Step 1: Add checkbox API to header**

In `PersonTreeModel.h`, add `#include <QSet>` to the includes.

Add to the public section, after `indexForPersonId`:
- A `setCheckable(bool)` method and `m_checkable` member (default `false`)
- A `setCheckedPersonIds(const QSet<PersonId>&)` method
- A `checkedPersonIds() const` getter returning `QSet<PersonId>`
- A `m_checkedIds` member of type `QSet<PersonId>`
- Override `flags()` and `setData()`

```cpp
// In public section, after indexForPersonId:

/// Enable checkbox mode for multi-select dialogs.
void setCheckable(bool checkable);

/// Set/get checked person IDs (only meaningful when checkable).
void setCheckedPersonIds(const QSet<PersonId>& ids);
QSet<PersonId> checkedPersonIds() const;

// QAbstractItemModel overrides for checkboxes
Qt::ItemFlags flags(const QModelIndex& index) const override;
bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
```

```cpp
// In private section, after m_filter:
bool m_checkable = false;
QSet<PersonId> m_checkedIds;
```

**Step 2: Implement checkbox methods in .cpp**

Add to `PersonTreeModel.cpp`:

```cpp
void PersonTreeModel::setCheckable(bool checkable)
{
    m_checkable = checkable;
}

void PersonTreeModel::setCheckedPersonIds(const QSet<PersonId>& ids)
{
    m_checkedIds = ids;
    // Refresh all check states
    if (!m_personNodes.isEmpty())
    {
        emit dataChanged(
            index(0, 0),
            index(m_personNodes.size() - 1, 0),
            {Qt::CheckStateRole});
    }
}

QSet<PersonId> PersonTreeModel::checkedPersonIds() const
{
    return m_checkedIds;
}

Qt::ItemFlags PersonTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = BaseTreeModel::flags(index);
    if (m_checkable && index.isValid())
    {
        TreeNode* node = nodeFromIndex(index);
        if (node && node->type == ItemType::Person)
        {
            f |= Qt::ItemIsUserCheckable;
        }
    }
    return f;
}

bool PersonTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_checkable || role != Qt::CheckStateRole || !index.isValid())
    {
        return false;
    }

    TreeNode* node = nodeFromIndex(index);
    if (!node || node->type != ItemType::Person)
    {
        return false;
    }

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    if (state == Qt::Checked)
    {
        m_checkedIds.insert(node->personId);
    }
    else
    {
        m_checkedIds.remove(node->personId);
    }

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}
```

**Step 3: Add CheckStateRole to data()**

In `PersonTreeModel::data()`, add a case for `Qt::CheckStateRole` in the switch:

```cpp
case Qt::CheckStateRole:
    if (m_checkable && node->type == ItemType::Person)
    {
        return m_checkedIds.contains(node->personId)
            ? Qt::Checked : Qt::Unchecked;
    }
    return QVariant();
```

**Step 4: Build and verify**

Build the project. No behavior change yet — checkable mode is off by default.

**Step 5: Commit**

```
feat: add checkbox support to PersonTreeModel
```

---

### Task 2: Add checkbox support to FamilyTreeModel

**Files:**
- Modify: `src/listmodels/FamilyTreeModel.h`
- Modify: `src/listmodels/FamilyTreeModel.cpp`

Same pattern as Task 1 but for families. Checkboxes appear on `RowType::Family` rows only.

**Step 1: Add checkbox API to header**

In `FamilyTreeModel.h`, add `#include <QSet>` to the includes.

Add to the public section, after `documentManager()`:

```cpp
// In public section, after documentManager():

/// Enable checkbox mode for multi-select dialogs.
void setCheckable(bool checkable);

/// Set/get checked family IDs (only meaningful when checkable).
void setCheckedFamilyIds(const QSet<FamilyId>& ids);
QSet<FamilyId> checkedFamilyIds() const;

// QAbstractItemModel overrides for checkboxes
Qt::ItemFlags flags(const QModelIndex& index) const override;
bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
```

```cpp
// In private section, after m_filter:
bool m_checkable = false;
QSet<FamilyId> m_checkedIds;
```

**Step 2: Implement checkbox methods in .cpp**

```cpp
void FamilyTreeModel::setCheckable(bool checkable)
{
    m_checkable = checkable;
}

void FamilyTreeModel::setCheckedFamilyIds(const QSet<FamilyId>& ids)
{
    m_checkedIds = ids;
    if (!m_familyNodes.isEmpty())
    {
        emit dataChanged(
            index(0, 0),
            index(m_familyNodes.size() - 1, 0),
            {Qt::CheckStateRole});
    }
}

QSet<FamilyId> FamilyTreeModel::checkedFamilyIds() const
{
    return m_checkedIds;
}

Qt::ItemFlags FamilyTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = BaseTreeModel::flags(index);
    if (m_checkable && index.isValid())
    {
        TreeNode* node = nodeFromIndex(index);
        if (node && node->type == RowType::Family)
        {
            f |= Qt::ItemIsUserCheckable;
        }
    }
    return f;
}

bool FamilyTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_checkable || role != Qt::CheckStateRole || !index.isValid())
    {
        return false;
    }

    TreeNode* node = nodeFromIndex(index);
    if (!node || node->type != RowType::Family)
    {
        return false;
    }

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    FamilyId familyId = m_familyIds.at(node->familyIndex);
    if (state == Qt::Checked)
    {
        m_checkedIds.insert(familyId);
    }
    else
    {
        m_checkedIds.remove(familyId);
    }

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}
```

**Step 3: Add CheckStateRole to data()**

In `FamilyTreeModel::data()`, add a case in the switch:

```cpp
case Qt::CheckStateRole:
    if (m_checkable && node->type == RowType::Family)
    {
        FamilyId familyId = m_familyIds.at(node->familyIndex);
        return m_checkedIds.contains(familyId)
            ? Qt::Checked : Qt::Unchecked;
    }
    return QVariant();
```

**Step 4: Build and verify**

Build the project. No behavior change yet.

**Step 5: Commit**

```
feat: add checkbox support to FamilyTreeModel
```

---

### Task 3: Update WardListDialog API and implementation

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1: Update the header**

Remove `SelectionMode` enum and `setSelectionMode()`. Update multi-select convenience method signatures. Add `m_checkable` member. Update `highlightInfo()` and `familyIdForCurrentSelection()` to a new `highlightedFamilyIds()` that returns all checked families in checkbox mode.

Replace the `SelectionMode` enum and `setSelectionMode` declaration with nothing (just delete them).

In the private section, add `bool m_checkable = false;`.

Replace the `familyIdForCurrentSelection()` private helper declaration with:
```cpp
QSet<FamilyId> highlightedFamilyIds() const;
```

Update `selectFamilies` and `selectPersons` signatures:

```cpp
/// Show dialog to select multiple families. Returns nullopt if cancelled.
static std::optional<QList<FamilyId>> selectFamilies(
    DocumentManager* documentManager,
    const QString& title,
    const QList<FamilyId>& initialIds = {},
    QWidget* parent = nullptr);

/// Show dialog to select multiple persons. Returns nullopt if cancelled.
static std::optional<QList<PersonId>> selectPersons(
    DocumentManager* documentManager,
    const QString& title,
    const QList<PersonId>& initialIds = {},
    QWidget* parent = nullptr);
```

Remove `setPreselectedFamilyIds` and `setPreselectedPersonIds` from the public section — they become private helpers or are inlined into the static methods.

**Step 2: Update setupUi()**

Remove the hardcoded title setting from `setupUi()`. The title will be set by each static convenience method after construction.

In `setupUi()`, remove:
```cpp
// Set dialog title based on mode
if (m_mode == FamilyMode)
{
    setWindowTitle(tr("Select Family"));
}
else
{
    setWindowTitle(tr("Select Person"));
}
```

**Step 3: Update selectPersons()**

```cpp
std::optional<QList<PersonId>> WardListDialog::selectPersons(
    DocumentManager* documentManager,
    const QString& title,
    const QList<PersonId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setWindowTitle(tr("Select People") + QString::fromUtf8(" \u2014 ") + title);
    dialog.m_personModel->setCheckable(true);
    if (!initialIds.isEmpty())
    {
        QSet<PersonId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_personModel->setCheckedPersonIds(idSet);

        // Scroll to first checked person
        QModelIndex firstIndex = dialog.m_personModel->indexForPersonId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<PersonId> checked = dialog.m_personModel->checkedPersonIds();
    return QList<PersonId>(checked.begin(), checked.end());
}
```

**Step 4: Update selectFamilies()**

```cpp
std::optional<QList<FamilyId>> WardListDialog::selectFamilies(
    DocumentManager* documentManager,
    const QString& title,
    const QList<FamilyId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setWindowTitle(tr("Select Families") + QString::fromUtf8(" \u2014 ") + title);
    dialog.m_familyModel->setCheckable(true);
    if (!initialIds.isEmpty())
    {
        QSet<FamilyId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_familyModel->setCheckedFamilyIds(idSet);

        // Scroll to first checked family
        QModelIndex firstIndex = dialog.m_familyModel->indexForFamilyId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<FamilyId> checked = dialog.m_familyModel->checkedFamilyIds();
    return QList<FamilyId>(checked.begin(), checked.end());
}
```

**Step 5: Update selectPerson()**

Set the title directly:

```cpp
std::optional<PersonId> WardListDialog::selectPerson(DocumentManager* documentManager,
                                     const std::optional<PersonId>& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setWindowTitle(tr("Select Person"));
    if (initialId)
    {
        dialog.setPreselectedPersonIds({*initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QList<PersonId> ids = dialog.selectedPersonIds();
        return ids.isEmpty() ? std::nullopt : std::optional<PersonId>(ids.first());
    }
    return std::nullopt;
}
```

**Step 6: Update selectFamily()**

```cpp
std::optional<FamilyId> WardListDialog::selectFamily(DocumentManager* documentManager,
                                     const std::optional<FamilyId>& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setWindowTitle(tr("Select Family"));
    if (initialId)
    {
        dialog.setPreselectedFamilyIds({*initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QList<FamilyId> ids = dialog.selectedFamilyIds();
        return ids.isEmpty() ? std::nullopt : std::optional<FamilyId>(ids.first());
    }
    return std::nullopt;
}
```

**Step 7: Remove setSelectionMode()**

Delete the `setSelectionMode()` implementation from the .cpp file. The single-select methods still use `SingleSelection` mode (the default for QTreeView), and multi-select methods use checkboxes instead of `ExtendedSelection`.

**Step 8: Update map highlighting for checkbox mode**

Replace `familyIdForCurrentSelection()` with `highlightedFamilyIds()` that returns all checked families in checkbox mode, or the single selected family in single-select mode:

```cpp
QSet<FamilyId> WardListDialog::highlightedFamilyIds() const
{
    if (m_checkable)
    {
        // In checkbox mode, highlight all checked items' families
        QSet<FamilyId> familyIds;
        if (m_mode == FamilyMode && m_familyModel)
        {
            familyIds = m_familyModel->checkedFamilyIds();
        }
        else if (m_personModel)
        {
            const Document& doc = m_documentManager->document();
            for (const PersonId& personId : m_personModel->checkedPersonIds())
            {
                auto familyId = doc.familyIdForPerson(personId);
                if (familyId)
                {
                    familyIds.insert(*familyId);
                }
            }
        }
        return familyIds;
    }

    // Single-select: highlight current selection
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return {};
    }

    if (m_mode == FamilyMode && m_familyModel)
    {
        auto id = m_familyModel->familyIdAt(current);
        if (id)
        {
            return {*id};
        }
    }
    else if (m_personModel)
    {
        auto personId = m_personModel->personIdAt(current);
        if (personId)
        {
            auto familyId = m_documentManager->document().familyIdForPerson(*personId);
            if (familyId)
            {
                return {*familyId};
            }
        }
    }
    return {};
}
```

Update `highlightInfo()` to use the new method:

```cpp
HighlightInfo WardListDialog::highlightInfo() const
{
    HighlightInfo info;
    info.highlightedFamilyIds = highlightedFamilyIds();
    return info;
}
```

Wire up map updates on check state changes. In the multi-select convenience methods (`selectPersons`, `selectFamilies`), after `setCheckable(true)`, connect the model's `dataChanged` signal to update the map:

```cpp
// In selectPersons, after dialog.m_personModel->setCheckable(true):
connect(dialog.m_personModel, &PersonTreeModel::dataChanged,
        dialog.m_mapWidget, &MapWidget::updateHighlights);

// In selectFamilies, after dialog.m_familyModel->setCheckable(true):
connect(dialog.m_familyModel, &FamilyTreeModel::dataChanged,
        dialog.m_mapWidget, &MapWidget::updateHighlights);
```

Also set `m_checkable = true` on the dialog itself so `highlightedFamilyIds()` knows the mode. Add this line in both `selectPersons` and `selectFamilies`, after setting checkable on the model:
```cpp
dialog.m_checkable = true;
```

**Step 9: Build and verify**

Build the project. Expect compile errors in call sites (next task).

**Step 10: Commit**

```
feat: update WardListDialog API with checkboxes, titles, and optional return
```

---

### Task 4: Update call sites

**Files:**
- Modify: `src/widgets/EmergencyAssetView.cpp`
- Modify: `src/widgets/TeamsView.cpp`

**Step 1: Update EmergencyAssetView**

Replace `showSelectPeopleDialog` body (lines 346-373 of `EmergencyAssetView.cpp`):

```cpp
void EmergencyAssetView::showSelectPeopleDialog(const EmergencyAssetId& assetId)
{
    const Document& doc = m_documentManager->document();
    std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(assetId);
    if (!assetOpt)
    {
        return;
    }

    QList<PersonId> currentIds = assetOpt->personIds().values();

    auto result = WardListDialog::selectPersons(
        m_documentManager, assetOpt->name(), currentIds, this);

    if (!result)
    {
        return;  // Cancelled
    }

    QSet<PersonId> newSet(result->begin(), result->end());
    if (newSet == assetOpt->personIds())
    {
        return;  // No change
    }

    EmergencyAsset updated = *assetOpt;
    updated.setPersonIds(newSet);
    m_documentManager->executeCommand(
        std::make_unique<UpdateEmergencyAssetCommand>(*assetOpt, updated));
}
```

**Step 2: Update TeamsView**

Replace `showSelectMembersDialog` body (lines 375-417 of `TeamsView.cpp`):

```cpp
void TeamsView::showSelectMembersDialog(const TeamId& teamId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    QList<PersonId> currentIds = teamOpt->memberIds().values();

    auto result = WardListDialog::selectPersons(
        m_documentManager, teamOpt->name(), currentIds, this);

    if (!result)
    {
        return;  // Cancelled
    }

    QSet<PersonId> newSet(result->begin(), result->end());
    if (newSet == teamOpt->memberIds())
    {
        return;  // No change
    }

    Team updated = *teamOpt;
    updated.setMemberIds(newSet);

    // Clear leader if they were removed
    if (updated.leaderId() && !newSet.contains(*updated.leaderId()))
    {
        updated.setLeaderId(std::nullopt);
    }

    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}
```

**Step 3: Build and verify**

Build the project. All compile errors should be resolved.

**Step 4: Commit**

```
feat: update EmergencyAssetView and TeamsView to use improved selectPersons
```

---

### Task 5: Clean up unused code

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1: Review what's still needed**

After Tasks 3-4, check if `setPreselectedFamilyIds`, `setPreselectedPersonIds`, `selectedFamilyIds`, `selectedPersonIds` are still used:
- `setPreselectedFamilyIds` / `setPreselectedPersonIds` — used by single-select convenience methods, keep
- `selectedFamilyIds` / `selectedPersonIds` — used by single-select convenience methods, keep

Check if `SelectionMode` enum is fully removed. Check that no code references `setSelectionMode`. Verify the `onSelectionChanged` slot is still needed (used for map highlighting in single-select mode).

**Step 2: Build final clean**

Build and verify everything compiles cleanly.

**Step 3: Commit**

```
chore: clean up unused WardListDialog code
```
