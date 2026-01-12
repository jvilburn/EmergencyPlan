# DocumentChange Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add metadata to `documentChanged` signal so views can perform surgical updates instead of full rebuilds.

**Architecture:** Create `DocumentChange` struct with scope/action/entityId, add `documentChange()` virtual to Command, update all emit sites and receivers.

**Tech Stack:** Qt 6.10, C++17, MSVC 2022

**Design Document:** [2026-01-11-document-change-design.md](2026-01-11-document-change-design.md)

---

### Task 1: Create DocumentChange.h

**Files:**
- Create: `src/models/DocumentChange.h`

**Step 1: Write the header file**

```cpp
#pragma once
#include <QString>

enum class ChangeScope {
    Full,             // Entire document (open, new, bulk import)
    Family,
    Team,
    Tag,
    ResourceCategory,
    ResourceType,
    EqDistrict,
    EqGroup,
    RsDistrict,
    RsGroup,
    Metadata          // Wards/Stakes
};

enum class ChangeAction {
    BatchModified,    // Collection changed (import, set all)
    Updated,          // Single entity modified
    Added,            // Single entity added
    Removed           // Single entity removed
};

struct DocumentChange;

struct ScopeBuilder {
    ChangeScope scope;

    DocumentChange updated(const QString& id) const;
    DocumentChange added(const QString& id) const;
    DocumentChange removed(const QString& id) const;
    DocumentChange batchModified() const;
};

struct DocumentChange {
    ChangeScope scope = ChangeScope::Full;
    ChangeAction action = ChangeAction::BatchModified;
    QString entityId;

    // Full document change
    static DocumentChange full();

    // Scope selectors return builders
    static ScopeBuilder family();
    static ScopeBuilder team();
    static ScopeBuilder tag();
    static ScopeBuilder resourceCategory();
    static ScopeBuilder resourceType();
    static ScopeBuilder eqDistrict();
    static ScopeBuilder eqGroup();
    static ScopeBuilder rsDistrict();
    static ScopeBuilder rsGroup();
    static ScopeBuilder metadata();
};

// Inline implementations
inline DocumentChange ScopeBuilder::updated(const QString& id) const {
    return {scope, ChangeAction::Updated, id};
}

inline DocumentChange ScopeBuilder::added(const QString& id) const {
    return {scope, ChangeAction::Added, id};
}

inline DocumentChange ScopeBuilder::removed(const QString& id) const {
    return {scope, ChangeAction::Removed, id};
}

inline DocumentChange ScopeBuilder::batchModified() const {
    return {scope, ChangeAction::BatchModified, {}};
}

inline DocumentChange DocumentChange::full() {
    return {ChangeScope::Full, ChangeAction::BatchModified, {}};
}

inline ScopeBuilder DocumentChange::family() { return {ChangeScope::Family}; }
inline ScopeBuilder DocumentChange::team() { return {ChangeScope::Team}; }
inline ScopeBuilder DocumentChange::tag() { return {ChangeScope::Tag}; }
inline ScopeBuilder DocumentChange::resourceCategory() { return {ChangeScope::ResourceCategory}; }
inline ScopeBuilder DocumentChange::resourceType() { return {ChangeScope::ResourceType}; }
inline ScopeBuilder DocumentChange::eqDistrict() { return {ChangeScope::EqDistrict}; }
inline ScopeBuilder DocumentChange::eqGroup() { return {ChangeScope::EqGroup}; }
inline ScopeBuilder DocumentChange::rsDistrict() { return {ChangeScope::RsDistrict}; }
inline ScopeBuilder DocumentChange::rsGroup() { return {ChangeScope::RsGroup}; }
inline ScopeBuilder DocumentChange::metadata() { return {ChangeScope::Metadata}; }
```

**Step 2: Run build to verify compilation**

Run: `build.bat`
Expected: Build succeeds (header-only, no CMake changes needed)

**Step 3: Commit**

```bash
git add src/models/DocumentChange.h
git commit -m "feat: add DocumentChange struct with fluent builder pattern"
```

---

### Task 2: Update Command base class

**Files:**
- Modify: `src/commands/Command.h`

**Step 1: Add include and pure virtual method**

Add at top of file:
```cpp
#include "DocumentChange.h"
```

Add after `description()` in Command class:
```cpp
    // Returns the DocumentChange describing what this command modifies
    virtual DocumentChange documentChange() const = 0;
```

**Step 2: Run build to see all compilation errors**

Run: `build.bat`
Expected: FAIL - all command classes need documentChange() implemented

**Step 3: Commit**

```bash
git add src/commands/Command.h
git commit -m "feat(Command): add pure virtual documentChange() method

BREAKING: All Command subclasses must now implement documentChange()"
```

---

### Task 3: Implement documentChange() in FamilyCommands

**Files:**
- Modify: `src/commands/FamilyCommands.h`
- Modify: `src/commands/FamilyCommands.cpp`

**Step 1: Add declarations to header**

In `AddFamilyCommand`:
```cpp
    DocumentChange documentChange() const override;
```

In `UpdateFamilyCommand`:
```cpp
    DocumentChange documentChange() const override;
```

In `DeleteFamilyCommand`:
```cpp
    DocumentChange documentChange() const override;
```

In `SetFamiliesCommand`:
```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to cpp**

```cpp
DocumentChange AddFamilyCommand::documentChange() const
{
    return DocumentChange::family().added(m_family.id());
}

DocumentChange UpdateFamilyCommand::documentChange() const
{
    return DocumentChange::family().updated(m_newFamily.id());
}

DocumentChange DeleteFamilyCommand::documentChange() const
{
    return DocumentChange::family().removed(m_family.id());
}

DocumentChange SetFamiliesCommand::documentChange() const
{
    return DocumentChange::family().batchModified();
}
```

**Step 3: Run build to verify this file compiles**

Run: `build.bat`
Expected: FamilyCommands.cpp compiles (other files still fail)

**Step 4: Commit**

```bash
git add src/commands/FamilyCommands.h src/commands/FamilyCommands.cpp
git commit -m "feat(FamilyCommands): implement documentChange()"
```

---

### Task 4: Implement documentChange() in TeamCommands

**Files:**
- Modify: `src/commands/TeamCommands.h`
- Modify: `src/commands/TeamCommands.cpp`

**Step 1: Add declarations to header**

For all 5 classes (`AddTeamCommand`, `UpdateTeamCommand`, `DeleteTeamCommand`, `AddTeamMemberCommand`, `RemoveTeamMemberCommand`):
```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to cpp**

```cpp
DocumentChange AddTeamCommand::documentChange() const
{
    return DocumentChange::team().added(m_team.id());
}

DocumentChange UpdateTeamCommand::documentChange() const
{
    return DocumentChange::team().updated(m_newTeam.id());
}

DocumentChange DeleteTeamCommand::documentChange() const
{
    return DocumentChange::team().removed(m_team.id());
}

DocumentChange AddTeamMemberCommand::documentChange() const
{
    return DocumentChange::team().updated(m_teamId);
}

DocumentChange RemoveTeamMemberCommand::documentChange() const
{
    return DocumentChange::team().updated(m_teamId);
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: TeamCommands.cpp compiles

**Step 4: Commit**

```bash
git add src/commands/TeamCommands.h src/commands/TeamCommands.cpp
git commit -m "feat(TeamCommands): implement documentChange()"
```

---

### Task 5: Implement documentChange() in TagCommands

**Files:**
- Modify: `src/commands/TagCommands.h`
- Modify: `src/commands/TagCommands.cpp`

**Step 1: Add declarations to header**

For all 6 classes:
```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to cpp**

```cpp
DocumentChange AddTagCommand::documentChange() const
{
    return DocumentChange::tag().added(m_tag.id());
}

DocumentChange UpdateTagCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_newTag.id());
}

DocumentChange DeleteTagCommand::documentChange() const
{
    return DocumentChange::tag().removed(m_tag.id());
}

DocumentChange AssignTagToPersonCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

DocumentChange AssignTagToFamilyCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

DocumentChange UnassignTagFromPersonCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

DocumentChange UnassignTagFromFamilyCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: TagCommands.cpp compiles

**Step 4: Commit**

```bash
git add src/commands/TagCommands.h src/commands/TagCommands.cpp
git commit -m "feat(TagCommands): implement documentChange()"
```

---

### Task 6: Implement documentChange() in ResourceCommands

**Files:**
- Modify: `src/commands/ResourceCommands.h`
- Modify: `src/commands/ResourceCommands.cpp`

**Step 1: Add declarations to header**

For all 10 classes:
```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to cpp**

```cpp
// Category commands
DocumentChange AddCategoryCommand::documentChange() const
{
    return DocumentChange::resourceCategory().added(m_category.id());
}

DocumentChange UpdateCategoryCommand::documentChange() const
{
    return DocumentChange::resourceCategory().updated(m_newCategory.id());
}

DocumentChange DeleteCategoryCommand::documentChange() const
{
    return DocumentChange::resourceCategory().removed(m_category.id());
}

// ResourceType commands
DocumentChange AddResourceTypeCommand::documentChange() const
{
    return DocumentChange::resourceType().added(m_resourceType.id());
}

DocumentChange UpdateResourceTypeCommand::documentChange() const
{
    return DocumentChange::resourceType().updated(m_newType.id());
}

DocumentChange DeleteResourceTypeCommand::documentChange() const
{
    return DocumentChange::resourceType().removed(m_resourceType.id());
}

// Assignment commands
DocumentChange AssignResourceToPersonCommand::documentChange() const
{
    return DocumentChange::resourceType().updated(m_resourceTypeId);
}

DocumentChange AssignResourceToFamilyCommand::documentChange() const
{
    return DocumentChange::resourceType().updated(m_resourceTypeId);
}

DocumentChange UnassignResourceFromPersonCommand::documentChange() const
{
    return DocumentChange::resourceType().updated(m_resourceTypeId);
}

DocumentChange UnassignResourceFromFamilyCommand::documentChange() const
{
    return DocumentChange::resourceType().updated(m_resourceTypeId);
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: ResourceCommands.cpp compiles

**Step 4: Commit**

```bash
git add src/commands/ResourceCommands.h src/commands/ResourceCommands.cpp
git commit -m "feat(ResourceCommands): implement documentChange()"
```

---

### Task 7: Implement documentChange() in MinisteringCommands

**Files:**
- Modify: `src/commands/MinisteringCommands.h`
- Modify: `src/commands/MinisteringCommands.cpp`

**Step 1: Add declarations to header**

For all 12 classes:
```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to cpp**

```cpp
// EQ District
DocumentChange AddEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().added(m_district.id());
}

DocumentChange UpdateEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().updated(m_newDistrict.id());
}

DocumentChange DeleteEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().removed(m_district.id());
}

// EQ Group
DocumentChange AddEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().added(m_group.id());
}

DocumentChange UpdateEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().updated(m_newGroup.id());
}

DocumentChange DeleteEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().removed(m_group.id());
}

// RS District
DocumentChange AddRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().added(m_district.id());
}

DocumentChange UpdateRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().updated(m_newDistrict.id());
}

DocumentChange DeleteRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().removed(m_district.id());
}

// RS Group
DocumentChange AddRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().added(m_group.id());
}

DocumentChange UpdateRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().updated(m_newGroup.id());
}

DocumentChange DeleteRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().removed(m_group.id());
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: MinisteringCommands.cpp compiles

**Step 4: Commit**

```bash
git add src/commands/MinisteringCommands.h src/commands/MinisteringCommands.cpp
git commit -m "feat(MinisteringCommands): implement documentChange()"
```

---

### Task 8: Implement documentChange() in Import commands

**Files:**
- Modify: `src/commands/ImportWardDirectoryCommand.h`
- Modify: `src/commands/ImportWardDirectoryCommand.cpp`
- Modify: `src/commands/ImportEQMinisteringCommand.h`
- Modify: `src/commands/ImportEQMinisteringCommand.cpp`
- Modify: `src/commands/ImportRSMinisteringCommand.h`
- Modify: `src/commands/ImportRSMinisteringCommand.cpp`

**Step 1: Add declarations to all three headers**

```cpp
    DocumentChange documentChange() const override;
```

**Step 2: Add implementations to all three cpp files**

All import commands return `full()` because they touch multiple scopes:

ImportWardDirectoryCommand.cpp:
```cpp
DocumentChange ImportWardDirectoryCommand::documentChange() const
{
    return DocumentChange::full();
}
```

ImportEQMinisteringCommand.cpp:
```cpp
DocumentChange ImportEQMinisteringCommand::documentChange() const
{
    return DocumentChange::full();
}
```

ImportRSMinisteringCommand.cpp:
```cpp
DocumentChange ImportRSMinisteringCommand::documentChange() const
{
    return DocumentChange::full();
}
```

**Step 3: Run build to verify all commands compile**

Run: `build.bat`
Expected: BUILD SUCCESS - all command classes now have documentChange()

**Step 4: Commit**

```bash
git add src/commands/Import*.h src/commands/Import*.cpp
git commit -m "feat(ImportCommands): implement documentChange() returning full()"
```

---

### Task 9: Update DocumentManager signal and emit sites

**Files:**
- Modify: `src/services/DocumentManager.h`
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Add include and change signal signature in header**

Add at top:
```cpp
#include "DocumentChange.h"
```

Change signal from:
```cpp
    void documentChanged();
```
to:
```cpp
    void documentChanged(const DocumentChange& change);
```

**Step 2: Update all emit sites in cpp**

Add include:
```cpp
#include "DocumentChange.h"
```

In `executeCommand()`:
```cpp
    DocumentChange change = command->documentChange();
    m_commandHistory.execute(std::move(command), m_document);
    emit documentChanged(change);
```

In `undo()`:
```cpp
    // Get the command being undone BEFORE calling undo
    const Command* cmd = m_commandHistory.currentUndoCommand();
    DocumentChange change = cmd ? cmd->documentChange() : DocumentChange::full();
    m_commandHistory.undo(m_document);
    emit documentChanged(change);
```

In `redo()`:
```cpp
    // Get the command being redone BEFORE calling redo
    const Command* cmd = m_commandHistory.currentRedoCommand();
    DocumentChange change = cmd ? cmd->documentChange() : DocumentChange::full();
    m_commandHistory.redo(m_document);
    emit documentChanged(change);
```

In `setDocument()`:
```cpp
    emit documentChanged(DocumentChange::full());
```

In `onWardLookupComplete()`:
```cpp
    emit documentChanged(DocumentChange::metadata().updated(wardUnitNumber));
```

In `onStakeLookupComplete()`:
```cpp
    emit documentChanged(DocumentChange::metadata().updated(stakeUnitNumber));
```

In `onFamilyGeocoded()`:
```cpp
    emit documentChanged(DocumentChange::family().updated(id));
```

**Step 3: Add CommandHistory accessor methods if needed**

Check if `CommandHistory` has `currentUndoCommand()` and `currentRedoCommand()`. If not, add them to `CommandHistory.h`:

```cpp
    const Command* currentUndoCommand() const;
    const Command* currentRedoCommand() const;
```

And implement in `CommandHistory.cpp`:
```cpp
const Command* CommandHistory::currentUndoCommand() const
{
    if (m_currentIndex > 0)
    {
        return m_commands[m_currentIndex - 1].get();
    }
    return nullptr;
}

const Command* CommandHistory::currentRedoCommand() const
{
    if (m_currentIndex < m_commands.size())
    {
        return m_commands[m_currentIndex].get();
    }
    return nullptr;
}
```

**Step 4: Run build**

Run: `build.bat`
Expected: FAIL - all signal receivers need updated slot signatures

**Step 5: Commit**

```bash
git add src/services/DocumentManager.h src/services/DocumentManager.cpp src/commands/CommandHistory.h src/commands/CommandHistory.cpp
git commit -m "feat(DocumentManager): emit DocumentChange with signal

BREAKING: All documentChanged() slot receivers must update signature"
```

---

### Task 10: Update FamilyTreeModel

**Files:**
- Modify: `src/listmodels/FamilyTreeModel.h`
- Modify: `src/listmodels/FamilyTreeModel.cpp`

**Step 1: Update header**

Add include:
```cpp
#include "DocumentChange.h"
```

Change slot declaration from:
```cpp
    void rebuild();
```
to:
```cpp
    void onDocumentChanged(const DocumentChange& change);
```

Add private methods:
```cpp
    void rebuild();
    void updateFamilyRow(const QString& familyId);
    void refreshExpandedDetails();  // For secondary scope changes
```

**Step 2: Update cpp**

Add include:
```cpp
#include "DocumentChange.h"
```

Find the connect() call for documentChanged and update it:
```cpp
connect(m_documentManager, &DocumentManager::documentChanged,
        this, &FamilyTreeModel::onDocumentChanged);
```

Add new handler:
```cpp
void FamilyTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Primary scope: Family
    if (change.scope == ChangeScope::Full)
    {
        rebuild();
        return;
    }

    if (change.scope == ChangeScope::Family)
    {
        if (change.action == ChangeAction::BatchModified
            || change.action == ChangeAction::Added
            || change.action == ChangeAction::Removed)
        {
            rebuild();
            return;
        }

        // Surgical update for Updated action - preserve expanded state
        updateFamilyRow(change.entityId);
        return;
    }

    // Secondary scopes: Team, Tag, ResourceType
    // These may affect expanded person details or filter results
    if (change.scope == ChangeScope::Team
        || change.scope == ChangeScope::Tag
        || change.scope == ChangeScope::ResourceType)
    {
        // For now, refresh any expanded items that show derived data
        // Future optimization: check if expanded persons are affected
        refreshExpandedDetails();
        return;
    }

    // Other scopes (Metadata, Ministering, etc.) - ignore
}

void FamilyTreeModel::updateFamilyRow(const QString& familyId)
{
    // Find the family node
    QModelIndex familyIndex = indexForFamilyId(familyId);
    if (!familyIndex.isValid())
    {
        return;
    }

    // Emit dataChanged for the family row and all its children
    TreeNode* node = nodeFromIndex(familyIndex);
    if (!node)
    {
        return;
    }

    // Update the family row itself
    emit dataChanged(familyIndex, familyIndex);

    // For a complete refresh of children, we need to rebuild just this node's children
    // Since display data might have changed (address, members, etc.)
    // The safest approach is to remove and re-add children
    int childCount = node->children.size();
    if (childCount > 0)
    {
        beginRemoveRows(familyIndex, 0, childCount - 1);
        qDeleteAll(node->children);
        node->children.clear();
        endRemoveRows();
    }

    // Rebuild children for this family
    int familyIndex_ = node->familyIndex;
    buildFamilyNode(familyIndex_);

    // Note: buildFamilyNode appends to m_familyNodes, but we need to update existing node
    // This needs adjustment - we should update node->children directly instead

    // Alternative: just emit dataChanged for all descendants
    // This is simpler but may miss structural changes
}
```

**Note:** The updateFamilyRow implementation above may need refinement. A simpler initial approach:

```cpp
void FamilyTreeModel::updateFamilyRow(const QString& familyId)
{
    // Find the family in our list
    int idx = m_familyIds.indexOf(familyId);
    if (idx < 0 || idx >= m_familyNodes.size())
    {
        return;
    }

    QModelIndex familyIndex = createIndex(idx, 0, m_familyNodes[idx]);

    // For now, just signal that data changed for the whole subtree
    // This preserves expanded state while refreshing display
    emit dataChanged(familyIndex, familyIndex, {Qt::DisplayRole});

    // Also update child rows
    TreeNode* node = m_familyNodes[idx];
    for (int i = 0; i < node->children.size(); ++i)
    {
        QModelIndex childIndex = createIndex(i, 0, node->children[i]);
        emit dataChanged(childIndex, childIndex, {Qt::DisplayRole});
    }
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: FamilyTreeModel compiles

**Step 4: Commit**

```bash
git add src/listmodels/FamilyTreeModel.h src/listmodels/FamilyTreeModel.cpp
git commit -m "feat(FamilyTreeModel): handle DocumentChange with surgical updates"
```

---

### Task 11: Update FamilyListModel

**Files:**
- Modify: `src/listmodels/FamilyListModel.h`
- Modify: `src/listmodels/FamilyListModel.cpp`

**Step 1: Update header**

Add include:
```cpp
#include "DocumentChange.h"
```

Find the slot connected to documentChanged (likely `rebuild()` or similar) and change signature:
```cpp
    void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Update cpp**

Add include and update handler:
```cpp
#include "DocumentChange.h"

void FamilyListModel::onDocumentChanged(const DocumentChange& change)
{
    // Primary scope: Family
    if (change.scope == ChangeScope::Full)
    {
        rebuild();
        return;
    }

    if (change.scope == ChangeScope::Family)
    {
        if (change.action == ChangeAction::BatchModified
            || change.action == ChangeAction::Added
            || change.action == ChangeAction::Removed)
        {
            rebuild();
            return;
        }

        // Updated - surgical update to just that row
        updateFamilyRow(change.entityId);
        return;
    }

    // Secondary scopes: Tag, ResourceType (affect filter results)
    if (change.scope == ChangeScope::Tag
        || change.scope == ChangeScope::ResourceType)
    {
        // If currently filtering by tags/resources, reapply filter
        if (hasActiveTagOrResourceFilter())
        {
            rebuild();
        }
        return;
    }

    // Other scopes - ignore
}
```

Update connect() call.

**Step 3: Run build**

Run: `build.bat`
Expected: FamilyListModel compiles

**Step 4: Commit**

```bash
git add src/listmodels/FamilyListModel.h src/listmodels/FamilyListModel.cpp
git commit -m "feat(FamilyListModel): handle DocumentChange with secondary scopes"
```

---

### Task 12: Update PersonListModel

**Files:**
- Modify: `src/listmodels/PersonListModel.h`
- Modify: `src/listmodels/PersonListModel.cpp`

**Step 1: Update header**

Add include and change slot signature:
```cpp
#include "DocumentChange.h"
// ...
    void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Update cpp**

```cpp
#include "DocumentChange.h"

void PersonListModel::onDocumentChanged(const DocumentChange& change)
{
    // Primary scope: Family (persons are in families)
    if (change.scope == ChangeScope::Full)
    {
        rebuild();
        return;
    }

    if (change.scope == ChangeScope::Family)
    {
        if (change.action == ChangeAction::BatchModified
            || change.action == ChangeAction::Added
            || change.action == ChangeAction::Removed)
        {
            rebuild();
            return;
        }

        // Updated - surgical update for persons in that family
        updatePersonsInFamily(change.entityId);
        return;
    }

    // Secondary scopes: Tag, ResourceType (affect filter results)
    if (change.scope == ChangeScope::Tag
        || change.scope == ChangeScope::ResourceType)
    {
        // If currently filtering by tags/resources, reapply filter
        if (hasActiveTagOrResourceFilter())
        {
            rebuild();
        }
        return;
    }

    // Other scopes - ignore
}
```

Update connect() call.

**Step 3: Run build**

Run: `build.bat`
Expected: PersonListModel compiles

**Step 4: Commit**

```bash
git add src/listmodels/PersonListModel.h src/listmodels/PersonListModel.cpp
git commit -m "feat(PersonListModel): handle DocumentChange"
```

---

### Task 13: Update MapWidget

**Files:**
- Modify: `src/widgets/MapWidget.h`
- Modify: `src/widgets/MapWidget.cpp`

**Step 1: Update header**

Add include and check slot signature (likely `updateButtonPositions()` or similar):
```cpp
#include "DocumentChange.h"
```

**Step 2: Update cpp if directly connected to documentChanged**

If MapWidget connects directly to documentChanged, update the slot. If it goes through MapViewModel, skip this task.

Check the connect() in MapWidget constructor - if it connects to DocumentManager::documentChanged, update.

**Step 3: Run build**

Run: `build.bat`
Expected: MapWidget compiles

**Step 4: Commit**

```bash
git add src/widgets/MapWidget.h src/widgets/MapWidget.cpp
git commit -m "feat(MapWidget): handle DocumentChange"
```

---

### Task 14: Update MapViewModel

**Files:**
- Modify: `src/viewmodels/MapViewModel.h`
- Modify: `src/viewmodels/MapViewModel.cpp`

**Step 1: Update header**

```cpp
#include "DocumentChange.h"
// ...
    void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Update cpp**

```cpp
#include "DocumentChange.h"

void MapViewModel::onDocumentChanged(const DocumentChange& change)
{
    // Only care about Family scope
    if (change.scope != ChangeScope::Full && change.scope != ChangeScope::Family)
    {
        return;
    }

    updateFamilies();
}
```

Update connect() call.

**Step 3: Run build**

Run: `build.bat`
Expected: MapViewModel compiles

**Step 4: Commit**

```bash
git add src/viewmodels/MapViewModel.h src/viewmodels/MapViewModel.cpp
git commit -m "feat(MapViewModel): handle DocumentChange"
```

---

### Task 15: Update MinisteringView

**Files:**
- Modify: `src/widgets/MinisteringView.h`
- Modify: `src/widgets/MinisteringView.cpp`

**Step 1: Update header**

```cpp
#include "DocumentChange.h"
// ...
    void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Update cpp**

```cpp
#include "DocumentChange.h"

void MinisteringView::onDocumentChanged(const DocumentChange& change)
{
    // Care about EQ/RS District and Group scopes
    switch (change.scope)
    {
        case ChangeScope::Full:
        case ChangeScope::EqDistrict:
        case ChangeScope::EqGroup:
        case ChangeScope::RsDistrict:
        case ChangeScope::RsGroup:
            regenerateColors();
            rebuildTree();
            break;
        default:
            // Ignore Family, Team, Tag, etc.
            break;
    }
}
```

Update connect() call.

**Step 3: Run build**

Run: `build.bat`
Expected: MinisteringView compiles

**Step 4: Commit**

```bash
git add src/widgets/MinisteringView.h src/widgets/MinisteringView.cpp
git commit -m "feat(MinisteringView): handle DocumentChange for ministering scopes"
```

---

### Task 16: Update MainWindow

**Files:**
- Modify: `src/widgets/MainWindow.h`
- Modify: `src/widgets/MainWindow.cpp`

**Step 1: Update header**

```cpp
#include "DocumentChange.h"
// ...
    void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Update cpp**

```cpp
#include "DocumentChange.h"

void MainWindow::onDocumentChanged(const DocumentChange& change)
{
    // Status bar always updates family count
    // Could optimize to only update on Family/Full scope, but count is cheap
    int count = m_documentManager->document().families().size();
    m_statusLabel->setText(tr("%1 families").arg(count));
}
```

Update connect() call.

**Step 3: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS - all views updated

**Step 4: Run application and test**

Test cases:
1. Open a document - verify all views refresh
2. Edit a family - verify FamilyTreeModel doesn't collapse expanded items
3. Let geocoding complete - verify expanded state preserved
4. Import ward directory - verify full refresh occurs

**Step 5: Commit**

```bash
git add src/widgets/MainWindow.h src/widgets/MainWindow.cpp
git commit -m "feat(MainWindow): handle DocumentChange"
```

---

### Task 17: Final verification and cleanup

**Step 1: Run full build**

Run: `build.bat`
Expected: BUILD SUCCESS

**Step 2: Run tests if available**

Run: `ctest --test-dir build`
Expected: All tests pass

**Step 3: Test the fix**

1. Open a ward document with families
2. Expand a family in the tree view
3. Wait for geocoding to complete (or manually trigger it)
4. Verify the expanded family stays expanded

**Step 4: Commit**

```bash
git add -A
git commit -m "feat: complete DocumentChange implementation

- Added DocumentChange struct with fluent builder pattern
- All commands implement documentChange()
- DocumentManager emits change metadata with signal
- FamilyTreeModel does surgical updates to preserve expanded state
- Other views updated to filter by scope

Fixes: expanded state lost when geocoding completes"
```

---

### Task 18: Refactor geocoding to use address cache

**Files:**
- Modify: `src/services/BackgroundGeocodingService.h`
- Modify: `src/services/BackgroundGeocodingService.cpp`
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Add geocode cache to BackgroundGeocodingService**

In header, add:
```cpp
#include "DocumentChange.h"
#include <QHash>
#include <QPointF>

// In private section:
QHash<QString, QPointF> m_geocodeCache;  // address → {lat, lng}

// Add slot:
void onDocumentChanged(const DocumentChange& change);
```

**Step 2: Implement cache-based geocoding**

In cpp:
```cpp
void BackgroundGeocodingService::onDocumentChanged(const DocumentChange& change)
{
    if (change.scope == ChangeScope::Full)
    {
        // Seed cache from families with existing coords
        for (const auto& family : m_documentManager->document().families())
        {
            if (!family.address().isEmpty() && family.isMapped())
            {
                m_geocodeCache[family.address()] =
                    QPointF(family.latitude(), family.longitude());
            }
        }

        // Queue unmapped families (check cache first)
        for (const auto& family : m_documentManager->document().families())
        {
            if (!family.address().isEmpty() && !family.isMapped())
            {
                checkAndQueueFamily(family);
            }
        }
        return;
    }

    if (change.scope == ChangeScope::Family
        && change.action == ChangeAction::Updated)
    {
        auto family = m_documentManager->document().findFamilyById(change.entityId);
        if (family && !family->address().isEmpty())
        {
            checkAndQueueFamily(*family);
        }
    }
}

void BackgroundGeocodingService::checkAndQueueFamily(const Family& family)
{
    QString address = family.address();

    if (m_geocodeCache.contains(address))
    {
        QPointF cached = m_geocodeCache[address];
        // Apply if different from current
        if (family.latitude() != cached.x() || family.longitude() != cached.y())
        {
            emit familyGeocoded(family.id(), cached.x(), cached.y());
        }
        return;
    }

    // Not in cache - queue for API
    queueFamily(family);
}
```

**Step 3: Update API completion to populate cache**

```cpp
void BackgroundGeocodingService::onApiComplete(const QString& familyId,
                                                const QString& address,
                                                double lat, double lng)
{
    // Add to cache
    m_geocodeCache[address] = QPointF(lat, lng);

    // Emit result
    emit familyGeocoded(familyId, lat, lng);
}
```

**Step 4: Connect to documentChanged signal**

In DocumentManager or where BackgroundGeocodingService is created:
```cpp
connect(m_documentManager, &DocumentManager::documentChanged,
        m_geocodingService, &BackgroundGeocodingService::onDocumentChanged);
```

**Step 5: Run build**

Run: `build.bat`
Expected: Compiles

**Step 6: Commit**

```bash
git add src/services/BackgroundGeocodingService.h src/services/BackgroundGeocodingService.cpp
git commit -m "feat(geocoding): use address→coords cache instead of familyWithChangedAddress

- Cache seeded from existing family coords on document load
- Cache populated when API returns results
- Duplicate addresses share cached coords (no extra API calls)
- Undo/redo uses cache for instant coord restoration"
```

---

### Task 19: Remove familyWithChangedAddress from Command

**Files:**
- Modify: `src/commands/Command.h`
- Modify: `src/commands/FamilyCommands.h`
- Modify: `src/commands/FamilyCommands.cpp`
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Remove from Command.h**

Delete:
```cpp
    // Returns ID of family whose address was changed by this command.
    // Empty string if no address changed. Used to trigger background geocoding.
    virtual QString familyWithChangedAddress() const { return {}; }
```

**Step 2: Remove override from FamilyCommands.h**

Delete from UpdateFamilyCommand:
```cpp
    QString familyWithChangedAddress() const override;
```

**Step 3: Remove implementation from FamilyCommands.cpp**

Delete:
```cpp
QString UpdateFamilyCommand::familyWithChangedAddress() const
{
    if (m_oldFamily.address() != m_newFamily.address())
    {
        return m_newFamily.id();
    }
    return {};
}
```

**Step 4: Remove usage from DocumentManager.cpp**

In `executeCommand()`, remove:
```cpp
    // Check before moving - trigger geocoding if address changed
    QString familyId = command->familyWithChangedAddress();
```

And remove:
```cpp
    if (!familyId.isEmpty())
    {
        queueFamilyForGeocoding(familyId);
    }
```

**Step 5: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

**Step 6: Update CLAUDE.md**

Remove the technical debt note about familyWithChangedAddress since it's now fixed.

**Step 7: Commit**

```bash
git add src/commands/Command.h src/commands/FamilyCommands.h src/commands/FamilyCommands.cpp src/services/DocumentManager.cpp CLAUDE.md
git commit -m "refactor: remove familyWithChangedAddress() from Command

Technical debt resolved - geocoding now uses DocumentChange + address cache"
```

---

### Task 20: Final verification

**Step 1: Test geocoding flow**

1. Open document with unmapped families
2. Verify geocoding starts automatically
3. Edit a family's address
4. Verify new address gets geocoded
5. Undo the edit
6. Verify old address coords restored from cache (no API call)

**Step 2: Test cache sharing**

1. Import ward directory with duplicate addresses
2. Verify only one API call per unique address
3. All families at same address get coords

**Step 3: Final commit**

```bash
git add -A
git commit -m "feat: complete geocoding refactor with address cache

Removes technical debt (familyWithChangedAddress) and adds address→coords cache"
```
