# DocumentChange Design

## Problem

When `documentChanged()` is emitted, all views perform full rebuilds regardless of what actually changed. This causes:
- FamilyTreeModel to collapse all expanded items (the immediate bug)
- Unnecessary rebuilds when a change doesn't affect a view's scope
- Poor scalability as document size grows

The trigger: geocoding completes, emits `documentChanged()`, FamilyTreeModel rebuilds, user loses expanded state.

## Solution

Add metadata to `documentChanged` signal describing what changed. Views can then:
1. Ignore changes outside their scope
2. Perform surgical updates for single-entity changes
3. Fall back to full rebuild only when necessary

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Signal approach | Single signal with metadata | Less boilerplate than 48 separate signals, easier to extend |
| Backward compatibility | No - all views must update | Cleaner long-term, update all views in one pass |
| Ministering granularity | Four scopes (EqDistrict, EqGroup, RsDistrict, RsGroup) | Prevent same expanded-state bug in MinisteringView |
| Bulk operations | Scope-specific BatchModified | `setFamilies()` affects only Family scope, not entire document |
| Header location | `src/models/DocumentChange.h` | Pure data type, no dependencies |
| Factory pattern | Fluent builder | 15 methods instead of 44, readable call sites |
| Entity data | ID only | Views fetch entity from DocumentManager if needed |
| Bidirectional relationships | Views decide | Commands return what they directly modified; views handle secondary scopes based on their state |

## DocumentChange Structure

```cpp
// src/models/DocumentChange.h
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
```

### Usage Examples

```cpp
// Single family updated (geocoding, edit)
emit documentChanged(DocumentChange::family().updated(familyId));

// Bulk import
emit documentChanged(DocumentChange::family().batchModified());

// Full document reload (open, new)
emit documentChanged(DocumentChange::full());

// Ministering group added
emit documentChanged(DocumentChange::eqGroup().added(groupId));
```

## Command Interface Change

```cpp
// Command.h
class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Document& document) = 0;
    virtual void undo(Document& document) = 0;
    virtual QString description() const = 0;
    virtual DocumentChange documentChange() const = 0;  // NEW

    // DEPRECATED - see CLAUDE.md technical debt note
    virtual QString familyWithChangedAddress() const { return {}; }
};
```

### Command Implementation Examples

```cpp
// UpdateFamilyCommand
DocumentChange documentChange() const override {
    return DocumentChange::family().updated(m_newFamily.id());
}

// SetFamiliesCommand
DocumentChange documentChange() const override {
    return DocumentChange::family().batchModified();
}

// ImportWardDirectoryCommand (touches families + metadata)
DocumentChange documentChange() const override {
    return DocumentChange::full();
}
```

## DocumentManager Changes

```cpp
// Signal signature change
signals:
    void documentChanged(const DocumentChange& change);

// executeCommand
void DocumentManager::executeCommand(CommandPtr command) {
    DocumentChange change = command->documentChange();
    QString familyId = command->familyWithChangedAddress();  // TODO: remove

    m_commandHistory.execute(std::move(command), m_document);
    emit documentChanged(change);

    if (!familyId.isEmpty()) {
        queueFamilyForGeocoding(familyId);
    }
}

// undo/redo - need to get change from the command being undone/redone

// setDocument (open, new)
emit documentChanged(DocumentChange::full());

// onWardLookupComplete, onStakeLookupComplete
emit documentChanged(DocumentChange::metadata().updated(unitNumber));

// onFamilyGeocoded
emit documentChanged(DocumentChange::family().updated(id));
```

## View Handler Pattern

```cpp
void FamilyTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Only care about Family scope
    if (change.scope != ChangeScope::Full && change.scope != ChangeScope::Family) {
        return;
    }

    // Full reload needed
    if (change.scope == ChangeScope::Full
        || change.action == ChangeAction::BatchModified) {
        rebuild();
        return;
    }

    // Surgical updates - preserve expanded state
    switch (change.action) {
        case ChangeAction::Updated:
            updateFamilyRow(change.entityId);
            break;
        case ChangeAction::Added:
            insertFamilyRow(change.entityId);
            break;
        case ChangeAction::Removed:
            removeFamilyRow(change.entityId);
            break;
    }
}
```

## Handling Bidirectional Relationships

Some entities have bidirectional relationships. For example:
- **Team ↔ Person**: Team stores `memberIds`; Person view shows "teams this person is on"
- **Tag ↔ Person**: Tags can be assigned to persons; views may filter by tag

**Principle:** Commands return what they directly modified. Views decide how to handle secondary scopes.

### Example: AddTeamMemberCommand

The command modifies Team data, so it returns:
```cpp
DocumentChange documentChange() const override {
    return DocumentChange::team().updated(m_teamId);
}
```

Views handle this based on their state:

**FamilyTreeModel (with expanded person showing team membership):**
```cpp
void FamilyTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Primary scope
    if (change.scope == ChangeScope::Full || change.scope == ChangeScope::Family) {
        // ... handle as before
        return;
    }

    // Secondary scope: Team changes may affect expanded persons
    if (change.scope == ChangeScope::Team) {
        // Option A: Refresh all expanded person details (simple, slightly wasteful)
        refreshExpandedPersonDetails();
        // Option B: Check if any expanded person is on this team (more precise)
        return;
    }
}
```

**WardListView (filtering by team):**
```cpp
void WardListView::onDocumentChanged(const DocumentChange& change)
{
    if (change.scope == ChangeScope::Team && isFilteringByTeams()) {
        // Team membership changed - reapply filter
        applyCurrentFilter();
    }
}
```

### Design Trade-off

Views may occasionally refresh more than strictly necessary, but:
- Commands stay simple (no view knowledge)
- No complex change-expansion system
- Views control their own refresh logic based on current state

## Views Requiring Update

| File | Current Handler | Primary Scope | Secondary Scopes | Notes |
|------|-----------------|---------------|------------------|-------|
| FamilyTreeModel | `rebuild()` | Family | Team, Tag, ResourceType | Surgical updates; secondary scopes affect expanded details |
| FamilyListModel | `rebuild()` | Family | Tag, ResourceType | Secondary scopes affect filter results |
| PersonListModel | `rebuild()` | Family | Tag, ResourceType | Secondary scopes affect filter results |
| MapWidget | `updateButtonPositions()` | Family | | Coordinates for markers |
| MapViewModel | `onDocumentChanged()` | Family | | |
| MinisteringView | `onDocumentChanged()` | EqDistrict, EqGroup, RsDistrict, RsGroup | | |
| MainWindow | `onDocumentChanged()` | Full (status bar) | | Simple count update |

## Emit Sites in DocumentManager

| Method | DocumentChange |
|--------|---------------|
| `executeCommand()` | `command->documentChange()` |
| `undo()` | command's documentChange() |
| `redo()` | command's documentChange() |
| `setDocument()` | `full()` |
| `onWardLookupComplete()` | `metadata().updated(unitNumber)` |
| `onStakeLookupComplete()` | `metadata().updated(unitNumber)` |
| `onFamilyGeocoded()` | `family().updated(id)` |

## Future Work

- Remove `Command::familyWithChangedAddress()` - see CLAUDE.md technical debt section
- Geocoding should observe `family().updated()` changes and check if address differs
- Consider surgical updates for other views (FamilyListModel, PersonListModel) if performance becomes an issue
