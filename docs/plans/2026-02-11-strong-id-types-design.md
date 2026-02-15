# Strong ID Types

## Overview

Replace all `QString`-based entity IDs with strongly-typed ID classes. A mechanical refactor with no behavioral changes.

## Motivation

All entity IDs (PersonId, FamilyId, TeamId, etc.) are currently plain `QString`. The compiler cannot prevent accidentally passing a `PersonId` where a `FamilyId` is expected. Correctness is enforced purely through naming conventions. Strong types make this a compile-time guarantee.

## ID Type System

### CRTP Base Class

A single file `src/models/Id.h` contains the base template and all ID classes.

```cpp
template<typename Derived>
class IdBase
{
    QString m_value;

protected:
    IdBase() = default;
    explicit IdBase(const QString& value) : m_value(value) {}

public:
    static Derived generate()
    {
        return Derived(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    static Derived fromString(const QString& value) { return Derived(value); }

    const QString& toString() const { return m_value; }

    friend bool operator==(const Derived& a, const Derived& b)
    {
        return a.m_value == b.m_value;
    }

    friend bool operator!=(const Derived& a, const Derived& b) { return !(a == b); }

    friend bool operator<(const Derived& a, const Derived& b)
    {
        return a.m_value < b.m_value;
    }

    friend size_t qHash(const Derived& id, size_t seed = 0)
    {
        return ::qHash(id.m_value, seed);
    }

    friend QDebug operator<<(QDebug dbg, const Derived& id)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace() << Derived::typeName() << '(' << id.m_value << ')';
        return dbg;
    }
};
```

The CRTP pattern ensures that `operator==`, `operator<`, and `qHash` only accept the same derived type. Comparing a `PersonId` to a `FamilyId` is a compile error. `operator<` enables use in ordered containers (`QMap`, `std::map`, `std::set`).

### Why CRTP Over Alternatives

- **vs. template + tag types** (`using PersonId = StrongId<PersonIdTag>`) -- CRTP gives real classes that are forward-declarable and show up as `PersonId` in debugger/compiler errors, not `StrongId<PersonIdTag>`.
- **vs. plain base class** -- A non-template base would allow `PersonId == FamilyId` to compile via implicit upcasting to `const StrongIdBase&`, defeating the purpose.
- **vs. macros** -- Macros are opaque to debuggers and IDEs.

### Derived Classes

Each ID class is 6 lines:

```cpp
class PersonId : public IdBase<PersonId>
{
    friend class IdBase<PersonId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "PersonId"; }
};
```

### The 7 ID Types

| Type | Used by |
|------|---------|
| `PersonId` | Person, Team members/leaders, MinisteringGroup ministers, EmergencyAsset people |
| `FamilyId` | Family, MinisteringGroup families, Tag family-level entities |
| `TeamId` | Team |
| `TagId` | Tag |
| `EmergencyAssetId` | EmergencyAsset |
| `MinisteringGroupId` | MinisteringGroup, MinisteringDistrict group sets |
| `MinisteringDistrictId` | MinisteringDistrict |

Ward/Stake unit numbers remain `QString` -- they are external identifiers from church systems, not internally-generated UUIDs.

### Constructor Visibility

The `QString` constructor is **protected**. Default constructor is also **protected** (needed for default construction of model structs like `Person() = default;`). Public API for creating IDs:

- `PersonId::generate()` -- new UUID
- `PersonId::fromString(str)` -- deserialization from JSON

There is no `null()` or `isNull()`. Optional ID fields use `std::optional<PersonId>` instead, which gives clear semantics (`if (opt)`) and avoids two competing null representations. This differs from the `QString` convention (where empty string doubles as "no value") because strong IDs are opaque types — an empty internal string is a construction artifact, not a meaningful domain state.

In debug builds, `toString()` asserts that the internal value is non-empty. This catches accidental use of default-constructed IDs (e.g., looking up `Person().id()` in a hash) without affecting release behavior:

```cpp
const QString& toString() const
{
    Q_ASSERT(!m_value.isEmpty());
    return m_value;
}
```

### Optional ID Fields

Fields where an ID may be absent use `std::optional<PersonId>` (or the appropriate ID type):

```cpp
// Member declaration
std::optional<PersonId> m_leaderId;

// Check + access
if (m_leaderId)
{
    doSomething(*m_leaderId);
}

// Clear
m_leaderId = std::nullopt;

// Set
m_leaderId = somePersonId;
```

This applies to:
- `Team::m_leaderId` — currently bare `QString`, becomes `std::optional<PersonId>`
- `MinisteringGroup::m_presidencyMemberId` — currently `std::optional<QString>`, becomes `std::optional<PersonId>`
- `MinisteringDistrict::m_presidencyMemberId` — same

Note: The `std::optional<QString>` fields above already violate CODING_STYLE.md ("Do not use `std::optional<QString>`"). Converting them to `std::optional<PersonId>` fixes this pre-existing violation — strong IDs are opaque types where `std::optional` is the correct nullable representation.

Lookup methods that may fail (e.g. `Document::familyIdForPerson`) return `std::optional<FamilyId>` instead of a default-constructed ID.

## SelectionKey Type

`SelectionPreservingTreeView` saves and restores selection across model resets using string keys returned by `BaseTreeModel::selectionKeyAt()`. These keys are currently `QString`, making them indistinguishable from entity IDs at the type level.

Selection keys are **not** entity IDs — they are synthetic, opaque comparison tokens. Simple models use a single entity ID as the key, but complex models build composites like `{groupId}:minister:{personId}`. A `SelectionKey` newtype prevents confusion:

```cpp
class SelectionKey
{
    QString m_value;
    explicit SelectionKey(const QString& value) : m_value(value) {}

public:
    template<typename Id>
    static SelectionKey from(const Id& id) { return SelectionKey(id.toString()); }

    static SelectionKey literal(const QString& value) { return SelectionKey(value); }

    friend bool operator==(const SelectionKey& a, const SelectionKey& b) { return a.m_value == b.m_value; }
    friend bool operator!=(const SelectionKey& a, const SelectionKey& b) { return !(a == b); }
};
```

`SelectionKey` is immutable — no default constructor, no `clear()`. Storage that may or may not have a key uses `std::optional<SelectionKey>` (e.g., `m_savedKey`). This is consistent with how optional entity IDs are handled throughout the design.

The `from` template is unconstrained — `id.toString()` only compiles for `IdBase<>` subtypes, which provides implicit compile-time safety without requiring C++20 concepts.

Composite keys are built using `QString` formatting at the call site, then wrapped:

```cpp
// Simple — single entity ID
return SelectionKey::from(node->assetId);

// Composite — formatted then wrapped
return SelectionKey::literal(QString("%1:minister:%2").arg(groupId.toString(), personId.toString()));

// Section header — literal string
return SelectionKey::literal(groupId.toString() + ":ministers");
```

`SelectionPreservingTreeView::m_savedKey` becomes `SelectionKey`. The `findIndex` / `findIndexRecursive` methods accept `const SelectionKey&`.

`BaseTreeModel::selectionKeyAt()` return type changes from `QString` to `SelectionKey`.

## Tag EntityIds Split

`Tag::m_entityIds` (a `QSet<QString>` holding either PersonIds or FamilyIds depending on tag level) splits into two typed sets:

- `QSet<PersonId> m_personIds`
- `QSet<FamilyId> m_familyIds`

Only one is populated based on tag level. This mirrors how `MinisteringGroup` already works with separate `m_ministerIds` / `m_familyIds` / `m_ministeredPersonIds`.

The generic `addEntity/removeEntity/hasEntity` methods become typed: `addPerson/removePerson/addFamily/removeFamily`. The Document-level API (`addPersonToTag`, `addFamilyToTag`) already has this separation.

## DocumentChange Redesign

The polymorphic `entityId` (`QString`) and `ChangeScope` enum are replaced by typed optional fields. Which field is set tells you the scope — no enum needed.

### ChangeAction

`BatchModified` is absorbed into `Full`. Every consumer already treats `BatchModified` as "rebuild everything" regardless of scope, making it functionally identical to `Full`.

```cpp
enum class ChangeAction
{
    Full,       // Entire document or collection changed (replaces old Full + BatchModified)
    Updated,    // Single entity modified
    Added,      // Single entity added
    Removed     // Single entity removed
};
```

### DocumentChange Struct

```cpp
struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // At most one is set — which field is set determines the scope
    std::optional<FamilyId> familyId;
    std::optional<TeamId> teamId;
    std::optional<TagId> tagId;
    std::optional<EmergencyAssetId> assetId;
    std::optional<MinisteringDistrictId> eqDistrictId;
    std::optional<MinisteringDistrictId> rsDistrictId;
    std::optional<MinisteringGroupId> eqGroupId;
    std::optional<MinisteringGroupId> rsGroupId;
    QString metadataValue;  // ward/stake unit numbers (external identifiers)

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders — each returns a concrete type that only accepts the right ID
    static FamilyScopeBuilder family();
    static TeamScopeBuilder team();
    static TagScopeBuilder tag();
    static AssetScopeBuilder emergencyAsset();
    static EqDistrictScopeBuilder eqDistrict();
    static RsDistrictScopeBuilder rsDistrict();
    static EqGroupScopeBuilder eqGroup();
    static RsGroupScopeBuilder rsGroup();
    static MetadataScopeBuilder metadata();
};
```

`action == Full` with no ID field set means rebuild everything. `action == Updated` with `familyId` set means one family changed.

### Scope Builders

Each scope builder is a concrete type that only accepts the right ID type. Compile-time enforcement — `DocumentChange::family().updated(personId)` won't compile:

```cpp
struct FamilyScopeBuilder
{
    DocumentChange updated(const FamilyId& id) const;
    DocumentChange added(const FamilyId& id) const;
    DocumentChange removed(const FamilyId& id) const;
    DocumentChange full() const;  // family collection changed (import)
};

// TeamScopeBuilder accepts TeamId, TagScopeBuilder accepts TagId, etc.
// MetadataScopeBuilder accepts const QString& (ward/stake unit numbers)
```

Existing `scope.batchModified()` calls become `scope.full()` — scoped `full()` means the entire collection of that type changed (e.g., after import).

### Consumer Pattern

Consumers check which field is set instead of comparing scope enums:

```cpp
// FamilyTreeModel::onDocumentChanged
if (change.action == ChangeAction::Full)
{
    rebuild();
    return;
}
if (!change.familyId)
{
    return;
}
switch (change.action)
{
    case ChangeAction::Updated:  updateFamilyRow(*change.familyId);  break;
    case ChangeAction::Added:    insertFamilyRow(*change.familyId);  break;
    case ChangeAction::Removed:  removeFamilyRow(*change.familyId);  break;
}

// MinisteringModel — ministering structure changes trigger rebuild
if (change.eqDistrictId || change.eqGroupId
    || change.rsDistrictId || change.rsGroupId)
{
    rebuild();
    return;
}
if (change.familyId && change.action == ChangeAction::Updated)
{
    refreshFamilyDisplayText(*change.familyId);
    return;
}
```

## Impact Summary

### Models

| Model | Changes |
|-------|---------|
| Person | `QString m_id` -> `PersonId m_id`; `createWithId(const QString&, ...)` -> `createWithId(const PersonId&, ...)` |
| Family | `QString m_id` -> `FamilyId m_id`; `createWithId(const QString&, ...)` -> `createWithId(const FamilyId&, ...)` |
| Team | `TeamId m_id`, `std::optional<PersonId> m_leaderId`, `QSet<PersonId> m_memberIds` |
| Tag | `TagId m_id`, split `m_entityIds` into `QSet<PersonId>` + `QSet<FamilyId>` |
| EmergencyAsset | `EmergencyAssetId m_id`, `QSet<PersonId> m_personIds` |
| MinisteringGroup | `MinisteringGroupId m_id`, `QSet<PersonId> m_ministerIds`, `QSet<FamilyId> m_familyIds`, `QSet<PersonId> m_ministeredPersonIds`, `std::optional<PersonId> m_presidencyMemberId` |
| MinisteringDistrict | `MinisteringDistrictId m_id`, `std::optional<PersonId> m_presidencyMemberId`, `QSet<MinisteringGroupId> m_groupIds` |
| Document | All `QHash<QString, Model>` -> `QHash<TypedId, Model>`, `m_personToFamily` -> `QHash<PersonId, FamilyId>`, `m_personResponseAreas` -> `QHash<PersonId, QSet<ResponseArea>>` |
| DocumentChange | `ChangeScope` enum removed; `entityId` replaced by typed optional fields; `BatchModified` merged into `Full`; concrete scope builders (see above) |

### Document Method Signatures

All `Document` methods that accept or return entity IDs become typed:

| Method | Change |
|--------|--------|
| `setFamilies(const QHash<QString, Family>&)` | `setFamilies(const QHash<FamilyId, Family>&)` |
| `removeFamily(const QString&)` | `removeFamily(const FamilyId&)` |
| `removeTeam(const QString&)` | `removeTeam(const TeamId&)` |
| `removeTag(const QString&)` | `removeTag(const TagId&)` |
| `addMemberToTeam(QString, QString)` | `addMemberToTeam(const TeamId&, const PersonId&)` |
| `removeMemberFromTeam(QString, QString)` | `removeMemberFromTeam(const TeamId&, const PersonId&)` |
| `addPersonToTag(QString, QString)` | `addPersonToTag(const TagId&, const PersonId&)` |
| `removePersonFromTag(QString, QString)` | `removePersonFromTag(const TagId&, const PersonId&)` |
| `addFamilyToTag(QString, QString)` | `addFamilyToTag(const TagId&, const FamilyId&)` |
| `removeFamilyFromTag(QString, QString)` | `removeFamilyFromTag(const TagId&, const FamilyId&)` |
| `removeEmergencyAsset(const QString&)` | `removeEmergencyAsset(const EmergencyAssetId&)` |
| `findEmergencyAssetById(const QString&)` | `findEmergencyAssetById(const EmergencyAssetId&)` |
| `removeEqDistrict/RsDistrict(const QString&)` | `...(const MinisteringDistrictId&)` |
| `removeEqGroup/RsGroup(const QString&)` | `...(const MinisteringGroupId&)` |
| `setEqDistricts/setRsDistricts(const QHash<QString, MinisteringDistrict>&)` | `...(const QHash<MinisteringDistrictId, MinisteringDistrict>&)` |
| `setEqGroups/setRsGroups(const QHash<QString, MinisteringGroup>&)` | `...(const QHash<MinisteringGroupId, MinisteringGroup>&)` |
| `cleanupPersonReferences(const QString&)` | `cleanupPersonReferences(const PersonId&)` |
| `findFamilyById(const QString&)` | `findFamilyById(const FamilyId&)` |
| `findPersonById(const QString&)` | `findPersonById(const PersonId&)` |
| `findTeamById(const QString&)` | `findTeamById(const TeamId&)` |
| `findTagById(const QString&)` | `findTagById(const TagId&)` |
| `familyIdForPerson(const QString&) -> QString` | `familyIdForPerson(const PersonId&) -> std::optional<FamilyId>` |
| `personResponseAreas(const QString&)` | `personResponseAreas(const PersonId&)` |

`familiesInStake(const QString&)` and `familiesInWard(const QString&)` remain `QString` — those take ward/stake unit numbers (external identifiers).

### Filter

| Field/Method | Change |
|--------------|--------|
| `m_tagIds` | `QSet<QString>` -> `QSet<TagId>` |
| `m_teamIds` | `QSet<QString>` -> `QSet<TeamId>` |
| `m_assetTypeIds` | `QSet<QString>` -> `QSet<EmergencyAssetId>` |
| `tagIds()` / `setTagIds(...)` | `QSet<TagId>` / `const QSet<TagId>&` |
| `teamIds()` / `setTeamIds(...)` | `QSet<TeamId>` / `const QSet<TeamId>&` |
| `assetTypeIds()` / `setAssetTypeIds(...)` | `QSet<EmergencyAssetId>` / `const QSet<EmergencyAssetId>&` |
| `hasFamilyLevelTag(..., const QString& familyId)` | `..., const FamilyId&` |
| `hasPersonLevelTag(..., const QString& personId)` | `..., const PersonId&` |
| `isOnTeam(..., const QString& personId)` | `..., const PersonId&` |
| `hasResponseArea(..., const QString& personId)` | `..., const PersonId&` |

`m_callings` and `m_specialNeeds` remain `QSet<QString>` — those are display strings, not entity IDs.

### BaseTreeModel and SelectionPreservingTreeView

| Change |
|--------|
| `BaseTreeModel::selectionKeyAt()` returns `SelectionKey` instead of `QString` |
| `BaseTreeModel::idAt()` removed — replaced by concrete-typed accessors on each model (see below) |
| `FamilyAssociation::relatedFamilyIds` -> `QSet<FamilyId>` |
| `FamilyAssociation::contactPointFamilyIds` -> `QSet<FamilyId>` |
| `SelectionPreservingTreeView::m_savedKey` -> `SelectionKey` |
| `SelectionPreservingTreeView::findIndex/findIndexRecursive` accept `const SelectionKey&` |

**Removing `idAt()`:** The base class `idAt()` returned a polymorphic `QString` whose actual type depended on `itemTypeAt()`. With strong IDs, a `QVariant` return would just push the type-safety problem downstream — callers would need `value.value<PersonId>()` with no compile-time guarantee they picked the right type. Every existing call site either knows the concrete type (e.g., NeedsSubView always gets a PersonId) or switches on `itemTypeAt()` first (e.g., `relatedFamiliesAt()`). Both patterns are better served by concrete-typed accessors (`personIdAt()`, `assetIdAt()`, etc.) on each model. No widget code queries `data(IdRole)`, so the `data()` roles are unaffected.

### TreeNode Structures

Models with **named, unambiguous** ID fields get direct conversion:

| Model | Field | Change |
|-------|-------|--------|
| PersonTreeModel::TreeNode | `personId`, `familyId` | `PersonId`, `FamilyId` |
| PersonTreeModel | `m_personData` | `QList<QPair<PersonId, FamilyId>>` |
| NeedsModel::TreeNode | `personId`, `familyId` | `PersonId`, `FamilyId` |
| FamilyTreeModel | `m_familyIds` | `QList<FamilyId>` |

Models with **polymorphic** `id` fields split into typed optional fields per `ItemType`:

**EmergencyAssetModel::TreeNode:**
```cpp
struct TreeNode
{
    ItemType type;
    EmergencyAssetId assetId;                // Always set (parent asset for Person nodes)
    std::optional<PersonId> personId;        // Set for Person nodes only
    // ...
};
```
For Asset nodes, `assetId` is the asset's own ID (the old `id` field is redundant with `assetId`). For Person nodes, `assetId` is the parent and `personId` identifies the person.

**MinisteringModel::TreeNode:**
```cpp
struct TreeNode
{
    ItemType type;
    std::optional<MinisteringDistrictId> districtId;   // District nodes
    std::optional<MinisteringGroupId> groupId;         // Companionship, SectionHeader nodes
    std::optional<PersonId> personId;                  // Minister, MinisteredSister nodes
    std::optional<FamilyId> familyId;                  // MinisteredFamily nodes
    // ...
};
```
Each node type populates only the relevant field. `secondaryId` (used by ContactDetail for the person/family whose contact info is shown) splits the same way.

**UnassignedMinisteringModel::TreeNode:**
```cpp
struct TreeNode
{
    ItemType type;
    std::optional<PersonId> personId;        // Sister nodes
    std::optional<FamilyId> familyId;        // Family nodes
    // ...
};
```
Header nodes have neither set. `secondaryId` splits the same way.

### Commands

| File | Changes |
|------|---------|
| **TagCommands.h** | `AssignTagToPersonCommand`: `m_tagId` -> `TagId`, `m_personId` -> `PersonId` |
| | `AssignTagToFamilyCommand`: `m_tagId` -> `TagId`, `m_familyId` -> `FamilyId` |
| | `UnassignTagFromPersonCommand`: same pattern |
| | `UnassignTagFromFamilyCommand`: same pattern |
| **TeamCommands.h** | `AddTeamMemberCommand`: `m_teamId` -> `TeamId`, `m_memberId` -> `PersonId` |
| | `RemoveTeamMemberCommand`: same pattern |
| **EmergencyAssetCommands.h** | `AssignEmergencyAssetToPersonCommand`: `m_assetId` -> `EmergencyAssetId`, `m_personId` -> `PersonId` |
| | `UnassignEmergencyAssetFromPersonCommand`: same pattern |
| | `Add`/`Update`/`Delete` commands hold model structs (typed ID is internal) |
| **ImportWardDirectoryCommand.h** | `m_removedFamilyIds` -> `QSet<FamilyId>` |
| | `m_removedFamilies` key -> `QHash<FamilyId, Family>` |
| | `cleanupRemovedFamily(const QString&)` -> `...(const FamilyId&)` |

| **FamilyCommands.h** | `SetFamiliesCommand`: `m_newFamilies` / `m_oldFamilies` -> `QHash<FamilyId, Family>` |
| **ImportEQMinisteringCommand.h** | `m_newDistricts` / `m_previousDistricts` -> `QHash<MinisteringDistrictId, MinisteringDistrict>` |
| | `m_newGroups` / `m_previousGroups` -> `QHash<MinisteringGroupId, MinisteringGroup>` |
| | `m_newFamilies` / `m_previousFamilies` -> `QHash<FamilyId, Family>` |
| **ImportRSMinisteringCommand.h** | Same pattern as ImportEQMinisteringCommand |

Family/Team/Tag/MinisteringDistrict/MinisteringGroup `Add`/`Update`/`Delete` commands hold the model struct directly — the typed ID propagates through the struct.

### List Models

| File | Changes |
|------|---------|
| **FamilyTreeModel.h** | `m_familyIds` -> `QList<FamilyId>` |
| | `familyIdAt()` returns `FamilyId` |
| | `personIdAt()` returns `std::optional<PersonId>` (Member rows only; replaces `idAt()` for Member case) |
| | `indexForFamilyId(const FamilyId&)` |
| | `familyIds()` returns `QList<FamilyId>` |
| | `updateFamilyRow/insertFamilyRow/removeFamilyRow(const FamilyId&)` |
| **PersonTreeModel.h** | `m_personData` -> `QList<QPair<PersonId, FamilyId>>` |
| | `personIdAt()` returns `PersonId` (replaces `idAt()`), `familyIdAt()` returns `FamilyId` |
| | `indexForPersonId(const PersonId&)` |
| **NeedsModel.h** | `personIdAt()` returns `PersonId` (replaces `idAt()`) |
| | `familyIdAt()` returns `FamilyId` |
| | `indexForPersonId(const PersonId&)` |
| **TeamListModel.h** | `m_teamIds` -> `QList<TeamId>` |
| | `setTeamIds(const QList<TeamId>&)`, `teamIds()` returns `QList<TeamId>` |
| | `teamIdAt()` returns `TeamId` (rename from `idAt()`), `rowForId(const TeamId&)` |
| **TagListModel.h** | `m_tagIds` -> `QList<TagId>` |
| | `setTagIds(const QList<TagId>&)`, `tagIds()` returns `QList<TagId>` |
| | `tagIdAt()` returns `TagId` (rename from `idAt()`), `rowForId(const TagId&)` |
| **EmergencyAssetModel.h** | TreeNode fields (see above) |
| | `assetIdAt()` returns `EmergencyAssetId` |
| | `personIdAt()` returns `std::optional<PersonId>` |
| | `refreshFamilyDisplayText(const FamilyId&)` |
| **MinisteringModel.h** | TreeNode fields (see above) |
| | `districtIdAt()` returns `std::optional<MinisteringDistrictId>` |
| | `companionshipIdAt()` returns `std::optional<MinisteringGroupId>` |
| | `personIdAt()` returns `std::optional<PersonId>` |
| | `familyIdAt()` returns `std::optional<FamilyId>` |
| | `refreshFamilyDisplayText(const FamilyId&)` |
| | `familyIdsForPersons(const QSet<PersonId>&)` returns `QSet<FamilyId>` |
| **UnassignedMinisteringModel.h** | TreeNode fields (see above) |
| | `personIdAt()` returns `std::optional<PersonId>` |
| | `familyIdAt()` returns `std::optional<FamilyId>` |
| | `refreshFamilyDisplayText(const FamilyId&)` |
| | `familyIdsForPersons(const QSet<PersonId>&)` returns `QSet<FamilyId>` |
| | `unassignedFamilyIds()` returns `QSet<FamilyId>` |
| | `unassignedSisterIds()` returns `QSet<PersonId>` |

### Widgets

| File | Changes |
|------|---------|
| **MainWindow.h** | `onEditFamilyRequested(const FamilyId&)` |
| | `onDeleteFamilyRequested(const FamilyId&)` |
| | `openEditPanel(const FamilyId&)` |
| **WardListView.h** | `selectedFamilyId()` returns `FamilyId` |
| | `setSelectedFamilyId(const FamilyId&)` |
| | `visibleFamilyIdsList()` returns `QList<FamilyId>` |
| | `editFamilyRequested(const FamilyId&)` signal |
| | `deleteFamilyRequested(const FamilyId&)` signal |
| | `detachActionButtons(const FamilyId&)` |
| | `m_actionWidgets` -> `QHash<FamilyId, ActionButtonsWidget*>` |
| **WardListDialog.h** | `selectFamily()` returns `std::optional<FamilyId>` (empty string -> nullopt) |
| | `selectFamilies()` returns `QList<FamilyId>` |
| | `selectPerson()` returns `std::optional<PersonId>` |
| | `selectPersons()` returns `QList<PersonId>` |
| | `setPreselectedFamilyIds(const QList<FamilyId>&)` / `setPreselectedPersonIds(const QList<PersonId>&)` (splits the generic `setPreselectedIds`) |
| | `selectedFamilyIds()` / `selectedPersonIds()` (splits `selectedIds()`) |
| | `onMapFamilyClicked(const FamilyId&)` |
| | `familyIdForCurrentSelection()` returns `std::optional<FamilyId>` |
| **FamilyEditPanel.h** | `m_familyId` -> `FamilyId` |
| | `familyId()` returns `FamilyId` |
| **MemberEditor.h** | `m_personId` -> `PersonId` |
| | `personId()` returns `PersonId` |
| **ActionButtonsWidget.h** | Constructor takes `const FamilyId&` |
| | `m_familyId` -> `FamilyId` |
| | `familyId()` returns `FamilyId` |
| | `editRequested(const FamilyId&)` / `deleteRequested(const FamilyId&)` signals |
| **EmergencyAssetView.h** | `m_contextAssetId` -> `EmergencyAssetId` |
| | `m_contextPersonId` -> `PersonId` |
| | `showSelectPeopleDialog(const EmergencyAssetId&)` |
| | `removePersonFromAsset(const EmergencyAssetId&, const PersonId&)` |
| | `selectedAssetId()` returns `EmergencyAssetId` |
| **NeedsSubView.h** | `showEditNeedDialog(const PersonId&, const FamilyId&)` |
| | `deleteNeed(const PersonId&, const FamilyId&)` |
| **MapWidget.h** | `familyClicked(const FamilyId&)` signal |
| | `markerAtPoint()` returns `std::optional<FamilyId>` (empty string -> nullopt) |
| | `ensureVisible(const QSet<FamilyId>&)` |
| **UnmappedPanel.h** | `familyClicked(const FamilyId&)` signal |
| | `MarkerLayout::familyId` -> `FamilyId` |
| | `markerAtPoint()` returns `std::optional<FamilyId>` |
| **MarkerRenderer.h** | `computeFamilyIcons(const FamilyId&, ...)` |
| **FamilyMarkerProvider.h** | `HighlightInfo::highlightedFamilyIds` -> `QSet<FamilyId>` |
| | `HighlightInfo::contactPointFamilyIds` -> `QSet<FamilyId>` |
| | `HighlightInfo::allHighlightedIds()` returns `QSet<FamilyId>` |
| | `visibleFamilyIds()` returns `QSet<FamilyId>` |
| | `familyStatusIcon(const FamilyId&)` |
| | All implementors update their overrides: WardListView, WardListDialog, PlaceholderView (in MainWindow.cpp), EmergencyAssetView, NeedsSubView, MinisteringTreeView, MinisteringView, MinisteringTabView, UnassignedTreeView |
| **SelectionPreservingTreeView.h** | `m_savedKey` -> `SelectionKey` |
| | `findIndex(const SelectionKey&)` / `findIndexRecursive(..., const SelectionKey&)` |

### Services

| File | Changes |
|------|---------|
| **BackgroundGeocodingService.h** | `familyGeocoded(const QString& id, ...)` signal -> `familyGeocoded(const FamilyId&, ...)` |
| | `m_familyAddresses` -> `QHash<FamilyId, QString>` (family ID -> address) |
| | `m_addressToFamilyId` -> `QHash<QString, FamilyId>` (address -> family ID) |
| | `m_queuedIds` -> `QSet<FamilyId>` |
| **DocumentManager.h** | `onFamilyGeocoded(const QString& id, ...)` -> `onFamilyGeocoded(const FamilyId&, ...)` |
| | `m_pendingWardLookups` / `m_pendingStakeLookups` remain `QSet<QString>` (ward/stake unit numbers) |
| **PersonMatching.h** | `PersonMatchScore::personId` -> `PersonId` |
| | `FamilyMemberMatchResult::familyId` -> `FamilyId` |
| | `FamilyMemberMatchResult::personIdMapping` -> `QHash<PersonId, PersonId>` |
| | `FamilyReplacementResult::replacedFamilyId` -> `FamilyId` |
| | `FamilyReplacementResult::personIdMapping` -> `QHash<PersonId, PersonId>` |
| | `findFamilyByMembers(...)` target param -> `const QHash<FamilyId, Family>&` |
| | `findReplacedFamily(...)` target param -> `const QHash<FamilyId, Family>&` |
| **MinisteringImportService.h** | `MinisteringImportResult::districts` -> `QHash<MinisteringDistrictId, MinisteringDistrict>` |
| | `MinisteringImportResult::groups` -> `QHash<MinisteringGroupId, MinisteringGroup>` |
| | `MinisteringImportResult::families` -> `QHash<FamilyId, Family>` |
| | `importFromPdf(...)` existing families param -> `const QHash<FamilyId, Family>&` |
| | `mergeFamilies(...)` -> typed hash params |
| | `mergeFamilyMembers(...)` personIdMapping -> `QHash<PersonId, PersonId>&` |
| **WardDirectoryImportService.h** | `WardDirectoryImportResult::families` -> `QHash<FamilyId, Family>` |
| | `WardDirectoryImportResult::removedFamilyIds` -> `QSet<FamilyId>` |
| | `importFromPdf(...)` existing families param -> `const QHash<FamilyId, Family>&` |

### ViewModels

| File | Changes |
|------|---------|
| **MapViewModel.h** | `selectedFamilyId()` / `setSelectedFamilyId(const QString&)` -> `FamilyId` / `const FamilyId&` |
| | `selectFamily(const QString&)` / `centerOnFamily(const QString&)` -> `const FamilyId&` |
| | `familyIcons(const QString&)` -> `familyIcons(const FamilyId&)` |
| | `familyClicked(const QString&)` signal -> `familyClicked(const FamilyId&)` |
| | `m_familyIcons` -> `QHash<FamilyId, MarkerRenderer::MarkerIcons>` |
| | `m_selectedId` -> `FamilyId` |
| | Remove `m_highlightedIds` (`QVariantMap`), `highlightedIds` Q_PROPERTY, `setHighlighting()`, `clearHighlighting()` — dead code; MapWidget reads highlights from `FamilyMarkerProvider::highlightInfo()`, not from the view model. Keep `highlightingChanged` signal (triggers repaint). |

## JSON Serialization

No format change. IDs remain strings in JSON. At the boundary:

```cpp
// Writing
json["id"] = m_id.toString();

// Reading
person.m_id = PersonId::fromString(json["id"].toString());
```

Existing save files load without migration.

## What Does Not Change

- JSON schema
- Application behavior
- Ward/Stake unit number types (remain QString)
- UUID generation strategy
- Any user-facing functionality
