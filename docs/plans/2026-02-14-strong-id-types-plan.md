# Strong ID Types Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace all `QString`-based entity IDs with strongly-typed ID classes for compile-time safety.

**Architecture:** CRTP base class `IdBase<Derived>` in a single header, 7 derived ID types, a `SelectionKey` newtype, and a redesigned `DocumentChange` struct. Mechanical refactor, no behavioral changes.

**Tech Stack:** C++17, Qt 6, MSVC 2022

**Design Reference:** [2026-02-11-strong-id-types-design.md](2026-02-11-strong-id-types-design.md) — the authoritative specification for all type changes, API signatures, and design rationale.

**Build Note:** Do NOT attempt to build from Claude. The user builds manually via `build.bat`. Code is expected to compile only after ALL tasks are complete. Commit after each task for tracking.

---

## Task 1: Create Id.h — Foundation Types

**Files:**
- Create: `src/models/Id.h`
- Modify: `CMakeLists.txt:103` (add to MODEL_HEADERS)

**Step 1: Create `src/models/Id.h`**

```cpp
#pragma once

#include <QString>
#include <QUuid>
#include <QDebug>
#include <QMetaType>

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

    const QString& toString() const
    {
        Q_ASSERT(!m_value.isEmpty());
        return m_value;
    }

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

// === Derived ID types ===

class PersonId : public IdBase<PersonId>
{
    friend class IdBase<PersonId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "PersonId"; }
};

class FamilyId : public IdBase<FamilyId>
{
    friend class IdBase<FamilyId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "FamilyId"; }
};

class TeamId : public IdBase<TeamId>
{
    friend class IdBase<TeamId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TeamId"; }
};

class TagId : public IdBase<TagId>
{
    friend class IdBase<TagId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TagId"; }
};

class EmergencyAssetId : public IdBase<EmergencyAssetId>
{
    friend class IdBase<EmergencyAssetId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "EmergencyAssetId"; }
};

class MinisteringGroupId : public IdBase<MinisteringGroupId>
{
    friend class IdBase<MinisteringGroupId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "MinisteringGroupId"; }
};

class MinisteringDistrictId : public IdBase<MinisteringDistrictId>
{
    friend class IdBase<MinisteringDistrictId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "MinisteringDistrictId"; }
};

// === SelectionKey ===

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

// === Qt metatype registration ===
// Required so ID types can be stored in QVariant (used by data() roles in tree models)
// and passed through signal/slot connections.

Q_DECLARE_METATYPE(PersonId)
Q_DECLARE_METATYPE(FamilyId)
Q_DECLARE_METATYPE(TeamId)
Q_DECLARE_METATYPE(TagId)
Q_DECLARE_METATYPE(EmergencyAssetId)
Q_DECLARE_METATYPE(MinisteringGroupId)
Q_DECLARE_METATYPE(MinisteringDistrictId)
```

**Design note on `isNull()`:** The design doc says "There is no `null()` or `isNull()`." However, several patterns in the existing code check `m_leaderId.isEmpty()` on bare QStrings. These become `std::optional<PersonId>` (checked with `.has_value()`), so `isNull()` on IdBase is actually unnecessary. **Remove `isNull()` from IdBase** — it would create two competing null representations, which the design explicitly warns against. Default-constructed IDs should only exist as temporaries in model structs (e.g., `Person() = default`), and the `Q_ASSERT(!m_value.isEmpty())` in `toString()` catches accidental use.

**Step 2: Add to CMakeLists.txt**

In MODEL_HEADERS (after line 123, before the closing paren), add:

```
    src/models/Id.h
```

**Step 3: Commit**

```
git add src/models/Id.h CMakeLists.txt
git commit -m "feat: add strongly-typed ID classes (Id.h)"
```

---

## Task 2: Rewrite DocumentChange

**Files:**
- Modify: `src/models/DocumentChange.h`
- Modify: `src/models/DocumentChange.cpp`

This is a complete rewrite. The new design removes `ChangeScope` enum and `ScopeBuilder`, replacing them with typed optional fields and concrete scope builders.

**Step 1: Rewrite `DocumentChange.h`**

Replace entire contents with:

```cpp
#pragma once

#include "Id.h"
#include <QString>
#include <optional>

enum class ChangeAction
{
    Full,       // Entire document or collection changed (replaces old Full + BatchModified)
    Updated,    // Single entity modified
    Added,      // Single entity added
    Removed     // Single entity removed
};

struct DocumentChange;

// === Concrete scope builders ===

struct FamilyScopeBuilder
{
    DocumentChange updated(const FamilyId& id) const;
    DocumentChange added(const FamilyId& id) const;
    DocumentChange removed(const FamilyId& id) const;
    DocumentChange full() const;
};

struct TeamScopeBuilder
{
    DocumentChange updated(const TeamId& id) const;
    DocumentChange added(const TeamId& id) const;
    DocumentChange removed(const TeamId& id) const;
    DocumentChange full() const;
};

struct TagScopeBuilder
{
    DocumentChange updated(const TagId& id) const;
    DocumentChange added(const TagId& id) const;
    DocumentChange removed(const TagId& id) const;
    DocumentChange full() const;
};

struct AssetScopeBuilder
{
    DocumentChange updated(const EmergencyAssetId& id) const;
    DocumentChange added(const EmergencyAssetId& id) const;
    DocumentChange removed(const EmergencyAssetId& id) const;
    DocumentChange full() const;
};

struct EqDistrictScopeBuilder
{
    DocumentChange updated(const MinisteringDistrictId& id) const;
    DocumentChange added(const MinisteringDistrictId& id) const;
    DocumentChange removed(const MinisteringDistrictId& id) const;
    DocumentChange full() const;
};

struct RsDistrictScopeBuilder
{
    DocumentChange updated(const MinisteringDistrictId& id) const;
    DocumentChange added(const MinisteringDistrictId& id) const;
    DocumentChange removed(const MinisteringDistrictId& id) const;
    DocumentChange full() const;
};

struct EqGroupScopeBuilder
{
    DocumentChange updated(const MinisteringGroupId& id) const;
    DocumentChange added(const MinisteringGroupId& id) const;
    DocumentChange removed(const MinisteringGroupId& id) const;
    DocumentChange full() const;
};

struct RsGroupScopeBuilder
{
    DocumentChange updated(const MinisteringGroupId& id) const;
    DocumentChange added(const MinisteringGroupId& id) const;
    DocumentChange removed(const MinisteringGroupId& id) const;
    DocumentChange full() const;
};

struct MetadataScopeBuilder
{
    DocumentChange updated(const QString& value) const;
    DocumentChange full() const;
};

// === DocumentChange struct ===

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
    QString metadataValue;

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
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

**Step 2: Rewrite `DocumentChange.cpp`**

Replace entire contents. Each scope builder's methods create a `DocumentChange` with the appropriate action and ID field set. For example:

```cpp
#include "DocumentChange.h"

// FamilyScopeBuilder
DocumentChange FamilyScopeBuilder::updated(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::added(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::removed(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::full() const
{
    return DocumentChange::full();
}

// ... same pattern for Team, Tag, Asset, EqDistrict, RsDistrict, EqGroup, RsGroup

// MetadataScopeBuilder
DocumentChange MetadataScopeBuilder::updated(const QString& value) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.metadataValue = value;
    return c;
}
DocumentChange MetadataScopeBuilder::full() const
{
    return DocumentChange::full();
}

// DocumentChange factory methods
DocumentChange DocumentChange::full()
{
    return DocumentChange{ChangeAction::Full};
}
FamilyScopeBuilder DocumentChange::family() { return FamilyScopeBuilder{}; }
TeamScopeBuilder DocumentChange::team() { return TeamScopeBuilder{}; }
TagScopeBuilder DocumentChange::tag() { return TagScopeBuilder{}; }
AssetScopeBuilder DocumentChange::emergencyAsset() { return AssetScopeBuilder{}; }
EqDistrictScopeBuilder DocumentChange::eqDistrict() { return EqDistrictScopeBuilder{}; }
RsDistrictScopeBuilder DocumentChange::rsDistrict() { return RsDistrictScopeBuilder{}; }
EqGroupScopeBuilder DocumentChange::eqGroup() { return EqGroupScopeBuilder{}; }
RsGroupScopeBuilder DocumentChange::rsGroup() { return RsGroupScopeBuilder{}; }
MetadataScopeBuilder DocumentChange::metadata() { return MetadataScopeBuilder{}; }
```

**Scoped `full()` note:** Each scope builder's `full()` simply returns `DocumentChange::full()` (a global full). This is correct because every consumer already treats `action == Full` as "rebuild everything" — no consumer distinguishes "all families changed in bulk" from "everything changed." Existing `scope.batchModified()` calls become `scope.full()`.

**Step 3: Commit**

```
git commit -m "refactor: rewrite DocumentChange with typed scope builders"
```

---

## Task 3: Convert Model Structs — Person, Family

**Files:**
- Modify: `src/models/Person.h:36,49,105` and `src/models/Person.cpp`
- Modify: `src/models/Family.h:27-28,35,75` and `src/models/Family.cpp`

**Person.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 105): `QString` → `PersonId`
- `id()` (line 49): return `const PersonId&` instead of `const QString&`
- `createWithId` (line 35-46): first param `const QString& id` → `const PersonId& id`
- `operator==`: compare `m_id` (PersonId has its own ==)

**Person.cpp changes:**
- `create()`: use `PersonId::generate()` instead of `QUuid::createUuid().toString(QUuid::WithoutBraces)`
- `createWithId()`: param type changes
- `toJson()`: `json["id"] = m_id.toString();`
- `fromJson()`: `person.m_id = PersonId::fromString(json["id"].toString());`

**Family.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 75): `QString` → `FamilyId`
- `id()` (line 35): return `const FamilyId&`
- `createWithId` (line 27-28): first param → `const FamilyId& id`

**Family.cpp changes:**
- `create()`: use `FamilyId::generate()`
- `createWithId()`: param type changes
- `toJson()`: `json["id"] = m_id.toString();`
- `fromJson()`: `family.m_id = FamilyId::fromString(json["id"].toString());`

**Step: Commit**

```
git commit -m "refactor: convert Person and Family to strong ID types"
```

---

## Task 4: Convert Model Structs — Team, Tag, EmergencyAsset

**Files:**
- Modify: `src/models/Team.h` and `src/models/Team.cpp`
- Modify: `src/models/Tag.h` and `src/models/Tag.cpp`
- Modify: `src/models/EmergencyAsset.h` and `src/models/EmergencyAsset.cpp`

**Team.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 51): `QString` → `TeamId`
- `m_leaderId` (line 54): `QString` → `std::optional<PersonId>`
- `m_memberIds` (line 55): `QSet<QString>` → `QSet<PersonId>`
- `id()` (line 22): return `const TeamId&`
- `leaderId()` (line 25): return `std::optional<PersonId>` (was `const QString&`)
- `memberIds()` (line 26): return `const QSet<PersonId>&`
- `setLeaderId(const QString&)` → `setLeaderId(std::optional<PersonId>)`
- `setMemberIds(const QSet<QString>&)` → `setMemberIds(const QSet<PersonId>&)`
- `addMember/removeMember/hasMember`: param `const QString&` → `const PersonId&`
- `hasLeader()`: `return m_leaderId.has_value();` (was `!m_leaderId.isEmpty()`)
- `create()`: `leaderId` param → `std::optional<PersonId>`

**Team.cpp changes:**
- `create()`: use `TeamId::generate()`
- `toJson()`: write `m_id.toString()`, write leaderId as `m_leaderId ? m_leaderId->toString() : QString()`, write memberIds by converting each to string
- `fromJson()`: read using `TeamId::fromString()`, `PersonId::fromString()`; for leaderId, read string and convert to `std::optional<PersonId>` (empty → nullopt)

**Tag.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 94): `QString` → `TagId`
- `m_entityIds` (line 98): split into `QSet<PersonId> m_personIds` + `QSet<FamilyId> m_familyIds`
- `id()` (line 63): return `const TagId&`
- Remove: `entityIds()`, `setEntityIds()`, `addEntity()`, `removeEntity()`, `hasEntity()`
- Add: `personIds()`, `familyIds()`, `setPersonIds()`, `setFamilyIds()`, `addPerson()`, `removePerson()`, `addFamily()`, `removeFamily()`, `hasPerson()`, `hasFamily()`
- `isEmpty()`: check both sets empty

**Tag.cpp changes:**
- `create()`: use `TagId::generate()`
- `toJson()`: write personIds and familyIds by converting to string arrays
- `fromJson()`: read from "entityIds" JSON array, converting to PersonId or FamilyId based on tag level. This preserves backward compatibility with existing JSON.

**EmergencyAsset.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 43): `QString` → `EmergencyAssetId`
- `m_personIds` (line 46): `QSet<QString>` → `QSet<PersonId>`
- `id()`: return `const EmergencyAssetId&`
- `personIds()`: return `const QSet<PersonId>&`
- All person methods: `const QString&` → `const PersonId&`
- `setPersonIds`: `const QSet<QString>&` → `const QSet<PersonId>&`

**EmergencyAsset.cpp changes:**
- `create()`: use `EmergencyAssetId::generate()`
- `toJson()/fromJson()`: convert IDs at the boundary

**Step: Commit**

```
git commit -m "refactor: convert Team, Tag, EmergencyAsset to strong ID types"
```

---

## Task 5: Convert Model Structs — MinisteringGroup, MinisteringDistrict

**Files:**
- Modify: `src/models/MinisteringGroup.h` and `src/models/MinisteringGroup.cpp`
- Modify: `src/models/MinisteringDistrict.h` and `src/models/MinisteringDistrict.cpp`

**MinisteringGroup.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 71): `QString` → `MinisteringGroupId`
- `m_ministerIds` (line 73): `QSet<QString>` → `QSet<PersonId>`
- `m_familyIds` (line 74): `QSet<QString>` → `QSet<FamilyId>`
- `m_ministeredPersonIds` (line 75): `QSet<QString>` → `QSet<PersonId>`
- `m_presidencyMemberId` (line 77): `std::optional<QString>` → `std::optional<PersonId>`
- All getters/setters/helpers updated to use typed IDs
- Factory methods: `QSet<QString>` params → typed sets

**MinisteringDistrict.h changes:**
- Add `#include "Id.h"`
- `m_id` (line 48): `QString` → `MinisteringDistrictId`
- `m_presidencyMemberId` (line 50): `std::optional<QString>` → `std::optional<PersonId>`
- `m_groupIds` (line 51): `QSet<QString>` → `QSet<MinisteringGroupId>`
- All getters/setters/helpers updated
- `create()`: typed params
- `hasPresidencyMember()`: `m_presidencyMemberId.has_value()` (drop the `&& !m_presidencyMemberId->isEmpty()` check — strong IDs can't be empty)

**Both .cpp files:** `toJson()` / `fromJson()` updated to convert at the boundary.

**Step: Commit**

```
git commit -m "refactor: convert MinisteringGroup, MinisteringDistrict to strong ID types"
```

---

## Task 6: Convert Document

**Files:**
- Modify: `src/models/Document.h`
- Modify: `src/models/Document.cpp`

**Document.h changes:**

All `QHash<QString, Model>` become `QHash<TypedId, Model>`:
- `m_families` (line 178): `QHash<FamilyId, Family>`
- `m_teams` (line 179): `QHash<TeamId, Team>`
- `m_tags` (line 180): `QHash<TagId, Tag>`
- `m_emergencyAssets` (line 189): `QHash<EmergencyAssetId, EmergencyAsset>`
- `m_eqDistricts` (line 192): `QHash<MinisteringDistrictId, MinisteringDistrict>`
- `m_eqGroups` (line 193): `QHash<MinisteringGroupId, MinisteringGroup>`
- `m_rsDistricts` (line 194): same
- `m_rsGroups` (line 195): same
- `m_personToFamily` (line 183): `QHash<PersonId, FamilyId>`
- `m_personResponseAreas` (line 202): `QHash<PersonId, QSet<ResponseArea>>`

All getter return types change accordingly (e.g., line 45: `const QHash<FamilyId, Family>& families() const`).

Method signature changes per design doc "Document Method Signatures" table:
- `removeFamily(const QString&)` → `removeFamily(const FamilyId&)` (line 71)
- `setFamilies(const QHash<QString, Family>&)` → `setFamilies(const QHash<FamilyId, Family>&)` (line 72)
- `removeTeam(const QString&)` → `removeTeam(const TeamId&)` (line 82)
- `addMemberToTeam(const QString&, const QString&)` → `addMemberToTeam(const TeamId&, const PersonId&)` (line 84)
- `removeMemberFromTeam(const QString&, const QString&)` → `removeMemberFromTeam(const TeamId&, const PersonId&)` (line 85)
- `removeTag(const QString&)` → `removeTag(const TagId&)` (line 92)
- `addPersonToTag/removePersonFromTag` → `(const TagId&, const PersonId&)` (lines 94-95)
- `addFamilyToTag/removeFamilyFromTag` → `(const TagId&, const FamilyId&)` (lines 96-97)
- `removeEmergencyAsset(const QString&)` → `removeEmergencyAsset(const EmergencyAssetId&)` (line 105)
- `findEmergencyAssetById(const QString&)` → `findEmergencyAssetById(const EmergencyAssetId&)` (line 106)
- All ministering remove/set methods: typed IDs (lines 114-132)
- `cleanupPersonReferences(const QString&)` → `cleanupPersonReferences(const PersonId&)` (line 140)
- `findFamilyById(const QString&)` → `findFamilyById(const FamilyId&)` (line 145)
- `findPersonById(const QString&)` → `findPersonById(const PersonId&)` (line 146)
- `familyIdForPerson(const QString&) -> QString` → `familyIdForPerson(const PersonId&) -> std::optional<FamilyId>` (line 147)
- `findTeamById(const QString&)` → `findTeamById(const TeamId&)` (line 148)
- `findTagById(const QString&)` → `findTagById(const TagId&)` (line 149)
- `personResponseAreas(const QString&)` → `personResponseAreas(const PersonId&)` (line 156)

Note: `familiesInStake(const QString&)` and `familiesInWard(const QString&)` remain `QString` — those take ward/stake unit numbers.

**Document.cpp changes:**

All method implementations updated to use typed IDs. The `toJson()` and `fromJson()` methods convert at the JSON boundary (same pattern as models). The `onDocumentChanged()` method must be updated for the new `DocumentChange` structure — check typed optional fields instead of `ChangeScope` enum.

`familyIdForPerson` now returns `std::optional<FamilyId>`:
```cpp
std::optional<FamilyId> Document::familyIdForPerson(const PersonId& personId) const
{
    auto it = m_personToFamily.find(personId);
    if (it != m_personToFamily.end())
    {
        return it.value();
    }
    return std::nullopt;
}
```

`rebuildPersonToFamilyMap()` builds `QHash<PersonId, FamilyId>`.

`rebuildDecorationCaches()` builds `QHash<PersonId, QSet<ResponseArea>>`.

**Step: Commit**

```
git commit -m "refactor: convert Document to strong ID types"
```

---

## Task 7: Convert Commands

**Files:**
- Modify: `src/commands/FamilyCommands.h` and `.cpp`
- Modify: `src/commands/TagCommands.h` and `.cpp`
- Modify: `src/commands/TeamCommands.h` and `.cpp`
- Modify: `src/commands/EmergencyAssetCommands.h` and `.cpp`
- Modify: `src/commands/ImportWardDirectoryCommand.h` and `.cpp`
- Modify: `src/commands/ImportEQMinisteringCommand.h` and `.cpp`
- Modify: `src/commands/ImportRSMinisteringCommand.h` and `.cpp`
- No changes needed: `src/commands/MinisteringCommands.h/.cpp` (hold model structs directly; no `batchModified()` calls — only individual CRUD `documentChange()` returns)

**FamilyCommands.h:**
- `SetFamiliesCommand`: `m_newFamilies` / `m_oldFamilies` → `QHash<FamilyId, Family>` (lines 52, 61-62)
- Add/Update/DeleteFamilyCommand: no header changes (hold `Family` struct, typed ID is internal)
- `documentChange()` implementations: update to use new scope builders. E.g., `AddFamilyCommand::documentChange()` returns `DocumentChange::family().added(m_family.id())`

**TagCommands.h:**
- `AssignTagToPersonCommand`: `m_tagId` → `TagId`, `m_personId` → `PersonId` (lines 60-61)
- `AssignTagToFamilyCommand`: `m_tagId` → `TagId`, `m_familyId` → `FamilyId` (lines 76-77)
- `UnassignTagFromPersonCommand`: same pattern (lines 90-91)
- `UnassignTagFromFamilyCommand`: same pattern (lines 105-106)
- Constructor params change accordingly
- `execute()`/`undo()` implementations call Document methods with typed IDs
- `documentChange()`: return `DocumentChange::tag().updated(m_tagId)`

**TeamCommands.h:**
- `AddTeamMemberCommand`: `m_teamId` → `TeamId`, `m_memberId` → `PersonId` (lines 60-61)
- `RemoveTeamMemberCommand`: same pattern (lines 76-77)
- `documentChange()`: return `DocumentChange::team().updated(m_teamId)`

**EmergencyAssetCommands.h:**
- `AssignEmergencyAssetToPersonCommand`: `m_assetId` → `EmergencyAssetId`, `m_personId` → `PersonId` (lines 60-61)
- `UnassignEmergencyAssetFromPersonCommand`: same pattern (lines 76-77)
- `documentChange()`: `DocumentChange::emergencyAsset().updated(m_assetId)`

**ImportWardDirectoryCommand.h:**
- `m_newFamilies` / `m_previousFamilies` / `m_removedFamilies`: `QHash<FamilyId, Family>` (lines 32-33, 43)
- `m_removedFamilyIds`: `QSet<FamilyId>` (line 34)
- `cleanupRemovedFamily(Document&, const QString&, const Family&)` → `...(const FamilyId&, const Family&)` (line 46)
- Constructor: `const QHash<QString, Family>&` → `const QHash<FamilyId, Family>&`, `const QSet<QString>&` → `const QSet<FamilyId>&` (lines 18-19)
- `documentChange()`: return `DocumentChange::full()` (imports are full changes)

**ImportEQMinisteringCommand.h:**
- All `QHash<QString, ...>` → typed hashes (lines 30-33, 38-40)
- Constructor: typed hash params
- `documentChange()`: return `DocumentChange::full()`

**ImportRSMinisteringCommand.h:**
- Same pattern as ImportEQMinisteringCommand

**All .cpp files:** Update implementations to match new signatures. The `batchModified()` calls become `full()`.

**Step: Commit**

```
git commit -m "refactor: convert commands to strong ID types"
```

---

## Task 8: Convert Services

**Files:**
- Modify: `src/services/PersonMatching.h` (no .cpp changes beyond signature fixes)
- Modify: `src/services/WardDirectoryImportService.h` and `.cpp`
- Modify: `src/services/MinisteringImportService.h` and `.cpp`
- Modify: `src/services/BackgroundGeocodingService.h` and `.cpp`
- Modify: `src/services/DocumentManager.h` and `.cpp`

**PersonMatching.h:**
- `PersonMatchScore::personId` → `PersonId` (line 22)
- `FamilyMemberMatchResult::familyId` → `std::optional<FamilyId>` (line 29; currently bare `QString`, "empty if no match" — empty string violates strong ID contract, so use optional)
- `FamilyMemberMatchResult::personIdMapping` → `QHash<PersonId, PersonId>` (line 32)
- `FamilyReplacementResult::replacedFamilyId` → `std::optional<FamilyId>` (line 38; same rationale — "empty if truly new")
- `FamilyReplacementResult::personIdMapping` → `QHash<PersonId, PersonId>` (line 41)
- `findFamilyByMembers()` target param → `const QHash<FamilyId, Family>&` (line 74)
- `findReplacedFamily()` target param → `const QHash<FamilyId, Family>&` (line 81)

**WardDirectoryImportService.h:**
- `WardDirectoryImportResult::families` → `QHash<FamilyId, Family>` (line 18)
- `WardDirectoryImportResult::removedFamilyIds` → `QSet<FamilyId>` (line 19)
- `importFromPdf()` existing families param → `const QHash<FamilyId, Family>&` (line 39)
- Internal `findMatchingFamily` and `preserveIds`: params change accordingly

**MinisteringImportService.h:**
- `MinisteringImportResult::districts` → `QHash<MinisteringDistrictId, MinisteringDistrict>` (line 19)
- `MinisteringImportResult::groups` → `QHash<MinisteringGroupId, MinisteringGroup>` (line 20)
- `MinisteringImportResult::families` → `QHash<FamilyId, Family>` (line 21)
- `importFromPdf()` existing families → `const QHash<FamilyId, Family>&` (line 46)
- `mergeFamilies()` → typed hash params (lines 56-61)
- `mergeFamilyMembers()` personIdMapping → `QHash<PersonId, PersonId>&` (line 82)

**BackgroundGeocodingService.h:**
- `familyGeocoded(const QString&, ...)` signal → `familyGeocoded(const FamilyId&, ...)` (line 52)
- `m_familyAddresses` → `QHash<FamilyId, QString>` (line 73)
- `m_addressToFamilyId` → `QHash<QString, FamilyId>` (line 76)
- `m_queuedIds` → `QSet<FamilyId>` (line 79)

**DocumentManager.h:**
- `onFamilyGeocoded(const QString&, ...)` → `onFamilyGeocoded(const FamilyId&, ...)` (line 72)
- `m_pendingWardLookups` / `m_pendingStakeLookups` remain `QSet<QString>` (ward/stake unit numbers)

**All .cpp files:** Update implementations to match. The `BackgroundGeocodingService::onDocumentChanged()` method must be updated for new DocumentChange structure.

**Step: Commit**

```
git commit -m "refactor: convert services to strong ID types"
```

---

## Task 9: Convert Filter + BaseTreeModel + SelectionPreservingTreeView

**Files:**
- Modify: `src/listmodels/Filter.h` and `.cpp`
- Modify: `src/listmodels/BaseTreeModel.h`
- Modify: `src/widgets/SelectionPreservingTreeView.h` and `.cpp`

**Filter.h changes:**
- Add `#include "Id.h"`
- `m_tagIds` (line 104): `QSet<QString>` → `QSet<TagId>`
- `m_teamIds` (line 105): `QSet<QString>` → `QSet<TeamId>`
- `m_assetTypeIds` (line 106): `QSet<QString>` → `QSet<EmergencyAssetId>`
- Getter/setter signatures change accordingly (lines 43-45, 58-60)
- `hasFamilyLevelTag(...)` (line 98): `const QString& familyId` → `const FamilyId&`
- `hasPersonLevelTag(...)` (line 99): `const QString& personId` → `const PersonId&`
- `isOnTeam(...)` (line 100): `const QString& personId` → `const PersonId&`
- `hasResponseArea(...)` (line 101): `const QString& personId` → `const PersonId&`
- `m_callings` and `m_specialNeeds` remain `QSet<QString>` (display strings, not entity IDs)

**Filter.cpp:** Update all method implementations.

**BaseTreeModel.h changes:**
- Add `#include "Id.h"` (for SelectionKey)
- `selectionKeyAt()` (line 58): return `SelectionKey` instead of `QString`
- Remove: `idAt()` (line 63) — replaced by concrete-typed accessors on each model
- `FamilyAssociation::relatedFamilyIds` (line 70): `QSet<QString>` → `QSet<FamilyId>`
- `FamilyAssociation::contactPointFamilyIds` (line 71): `QSet<QString>` → `QSet<FamilyId>`

**SelectionPreservingTreeView.h changes:**
- Add `#include "Id.h"` (for SelectionKey)
- `m_savedKey` (line 44): `QString` → `std::optional<SelectionKey>`
- `findIndex(const QString&)` → `findIndex(const SelectionKey&)` (line 36)
- `findIndexRecursive(..., const QString&)` → `findIndexRecursive(..., const SelectionKey&)` (line 39)

**SelectionPreservingTreeView.cpp changes:**
- `onModelAboutToBeReset()`: save key as `SelectionKey` from model
- `onModelReset()`: restore from `std::optional<SelectionKey>`
- `findIndex`/`findIndexRecursive`: compare `SelectionKey` values

**Step: Commit**

```
git commit -m "refactor: convert Filter, BaseTreeModel, SelectionPreservingTreeView to strong ID types"
```

---

## Task 10: Convert List Models

**Files:**
- Modify: `src/listmodels/FamilyTreeModel.h` and `.cpp`
- Modify: `src/listmodels/PersonTreeModel.h` and `.cpp`
- Modify: `src/listmodels/NeedsModel.h` and `.cpp`
- Modify: `src/listmodels/TeamListModel.h` and `.cpp`
- Modify: `src/listmodels/TagListModel.h` and `.cpp`
- Modify: `src/listmodels/EmergencyAssetModel.h` and `.cpp`
- Modify: `src/listmodels/MinisteringModel.h` and `.cpp`
- Modify: `src/listmodels/UnassignedMinisteringModel.h` and `.cpp`

### FamilyTreeModel.h
- `m_familyIds` (line 121): `QList<QString>` → `QList<FamilyId>`
- Remove `idAt()` override
- `familyIdAt()` (line 78): return `FamilyId` (was `QString`)
- Add: `personIdAt()` returning `std::optional<PersonId>` (for Member rows)
- `indexForFamilyId(const QString&)` → `indexForFamilyId(const FamilyId&)` (line 79)
- `familyIds()` (line 83): return type → `QList<FamilyId>` (not QStringList)
- `updateFamilyRow/insertFamilyRow/removeFamilyRow` (lines 96-98): `const QString&` → `const FamilyId&`
- `selectionKeyAt()`: return `SelectionKey::from(familyId)` for Family rows, `SelectionKey::from(personId)` for Member rows, parent's key for detail rows
- `onDocumentChanged()`: update for new DocumentChange (check `change.familyId` instead of `change.scope == ChangeScope::Family`)

### PersonTreeModel.h
- `m_personData` (line 82): `QList<QPair<QString, QString>>` → `QList<QPair<PersonId, FamilyId>>`
- TreeNode (lines 70-71): `personId` → `PersonId`, `familyId` → `FamilyId`
- Remove `idAt()` override
- `personIdAt()` (line 52): return `PersonId`
- `familyIdAt()` (line 53): return `FamilyId`
- `indexForPersonId(const QString&)` → `indexForPersonId(const PersonId&)` (line 54)
- `selectionKeyAt()`: return `SelectionKey::from(personId)`

### NeedsModel.h
- TreeNode (lines 69-70): `personId` → `PersonId`, `familyId` → `FamilyId`
- Remove `idAt()` override
- `familyIdAt()` (line 56): return `FamilyId`
- Add: `personIdAt()` returning `PersonId`
- `indexForPersonId(const QString&)` → `indexForPersonId(const PersonId&)` (line 57)
- `selectionKeyAt()`: return `SelectionKey::from(personId)`

### TeamListModel.h
- `m_teamIds` (line 45): `QList<QString>` → `QList<TeamId>`
- `setTeamIds(const QList<QString>&)` → `setTeamIds(const QList<TeamId>&)` (line 37)
- `teamIds()` (line 38): return `const QList<TeamId>&`
- `idAt(int)` → `teamIdAt(int)` returning `TeamId` (line 41)
- `rowForId(const QString&)` → `rowForId(const TeamId&)` (line 42)

### TagListModel.h
- `m_tagIds` (line 44): `QList<QString>` → `QList<TagId>`
- `setTagIds(const QList<QString>&)` → `setTagIds(const QList<TagId>&)` (line 36)
- `tagIds()` (line 37): return `const QList<TagId>&`
- `idAt(int)` → `tagIdAt(int)` returning `TagId` (line 40)
- `rowForId(const QString&)` → `rowForId(const TagId&)` (line 41)

### EmergencyAssetModel.h
- TreeNode (lines 76-77): `id` field removed; `assetId` → `EmergencyAssetId`; add `std::optional<PersonId> personId` for Person nodes
- Remove `idAt()` override
- `assetIdAt()` (line 61): return `EmergencyAssetId`
- Add: `personIdAt()` returning `std::optional<PersonId>`
- `refreshFamilyDisplayText(const QString&)` → `refreshFamilyDisplayText(const FamilyId&)` (line 69)
- `selectionKeyAt()`: `SelectionKey::from(assetId)` for Asset nodes, `SelectionKey::literal(assetId.toString() + ":" + personId->toString())` for Person nodes
- `onDocumentChanged()`: update for new DocumentChange

### MinisteringModel.h
- TreeNode (lines 79-81): replace `id` and `secondaryId` with typed optional fields:
  ```cpp
  std::optional<MinisteringDistrictId> districtId;
  std::optional<MinisteringGroupId> groupId;
  std::optional<PersonId> personId;
  std::optional<FamilyId> familyId;
  ```
  Field mapping per ItemType (`id` / `secondaryId` → new fields):
  | ItemType | `id` → | `secondaryId` → |
  |----------|--------|-----------------|
  | District | `districtId` | — |
  | Companionship | `groupId` | — |
  | SectionHeader | `groupId` (composite key built from groupId in selectionKeyAt) | — |
  | Minister | `personId` | — |
  | MinisteredFamily | `familyId` | — |
  | MinisteredSister | `personId` | — |
  | ContactDetail | — | copy parent's `personId` or `familyId` (person contacts → `personId`, family contacts → `familyId`) |
- Remove `idAt()` override
- Add typed accessors: `districtIdAt()`, `companionshipIdAt()` (already exists but returns QString), `personIdAt()`, `familyIdAt()`
- `refreshFamilyDisplayText(const QString&)` → `refreshFamilyDisplayText(const FamilyId&)` (line 72)
- `familyIdsForPersons(const QSet<QString>&)` → `familyIdsForPersons(const QSet<PersonId>&)` returning `QSet<FamilyId>` (line 97)
- `addMinistersSection/addMinisteredSection`: `const QString&` → `const MinisteringGroupId&` (lines 93-94)
- `selectionKeyAt()`: build composite keys using `SelectionKey::literal()` per the format documented in BaseTreeModel.h
- `onDocumentChanged()`: update for new DocumentChange (check `change.eqDistrictId || change.eqGroupId` etc.)

### UnassignedMinisteringModel.h
- TreeNode (lines 76-78): replace `id` and `secondaryId` with:
  ```cpp
  std::optional<PersonId> personId;
  std::optional<FamilyId> familyId;
  ```
  Field mapping per ItemType:
  | ItemType | `id` → | `secondaryId` → |
  |----------|--------|-----------------|
  | UnassignedHeader | — (use `SelectionKey::literal("unassigned")`) | — |
  | MinisteredFamily | `familyId` | — |
  | MinisteredSister | `personId` | — |
  | ContactDetail | — | copy parent's `personId` or `familyId` |
- Remove `idAt()` override
- Add: `personIdAt()`, `familyIdAt()` returning typed optionals
- `refreshFamilyDisplayText(const QString&)` → `refreshFamilyDisplayText(const FamilyId&)` (line 69)
- `familyIdsForPersons(const QSet<QString>&)` → `familyIdsForPersons(const QSet<PersonId>&)` returning `QSet<FamilyId>` (line 91)
- `unassignedFamilyIds()` → returns `QSet<FamilyId>` (line 92)
- `unassignedSisterIds()` → returns `QSet<PersonId>` (line 93)
- `selectionKeyAt()`: `SelectionKey::literal("unassigned")` for header, `SelectionKey::from(familyId)` / `SelectionKey::from(personId)` for items

**All .cpp files:** Update implementations, especially `onDocumentChanged()` handlers (replace `change.scope == ChangeScope::X` / `change.action == ChangeAction::BatchModified` with the new DocumentChange field checks per the consumer pattern in the design doc).

**Step: Commit**

```
git commit -m "refactor: convert all list models to strong ID types"
```

---

## Task 11: Convert Widgets (Part 1 — Core)

**Files:**
- Modify: `src/widgets/FamilyMarkerProvider.h`
- Modify: `src/widgets/MarkerRenderer.h` and `.cpp`
- Modify: `src/widgets/ActionButtonsWidget.h` and `.cpp`
- Modify: `src/widgets/FamilyEditPanel.h` and `.cpp`
- Modify: `src/widgets/MemberEditor.h` and `.cpp`

**FamilyMarkerProvider.h:**
- Add `#include "Id.h"`
- `HighlightInfo::highlightedFamilyIds` (line 10): `QSet<QString>` → `QSet<FamilyId>`
- `HighlightInfo::contactPointFamilyIds` (line 11): `QSet<QString>` → `QSet<FamilyId>`
- `allHighlightedIds()` (line 21): returns `QSet<FamilyId>`
- `visibleFamilyIds()` (line 37): returns `QSet<FamilyId>`
- `familyStatusIcon(const QString&)` → `familyStatusIcon(const FamilyId&)` (line 41)

**MarkerRenderer.h:**
- `computeFamilyIcons(const QString&, ...)` → `computeFamilyIcons(const FamilyId&, ...)` (line 36)

**ActionButtonsWidget.h:**
- `m_familyId` (line 23): `QString` → `FamilyId`
- Constructor: `const QString&` → `const FamilyId&` (line 14)
- `familyId()` (line 16): returns `FamilyId`
- `editRequested(const QString&)` → `editRequested(const FamilyId&)` (line 19)
- `deleteRequested(const QString&)` → `deleteRequested(const FamilyId&)` (line 20)

**FamilyEditPanel.h:**
- `m_familyId` (line 47): `QString` → `FamilyId`
- `familyId()` (line 25): returns `FamilyId`

**MemberEditor.h:**
- `m_personId` (line 33): `QString` → `PersonId`
- `personId()` (line 24): returns `PersonId`

**All .cpp files:** Update implementations.

**Step: Commit**

```
git commit -m "refactor: convert core widgets to strong ID types"
```

---

## Task 12: Convert Widgets (Part 2 — Views)

**Files:**
- Modify: `src/widgets/MainWindow.h` and `.cpp`
- Modify: `src/widgets/WardListView.h` and `.cpp`
- Modify: `src/widgets/WardListDialog.h` and `.cpp`
- Modify: `src/widgets/EmergencyAssetView.h` and `.cpp`
- Modify: `src/widgets/NeedsSubView.h` and `.cpp`
- Modify: `src/widgets/MapWidget.h` and `.cpp`
- Modify: `src/widgets/UnmappedPanel.h` and `.cpp`
- Modify: `src/widgets/MinisteringTreeView.h` and `.cpp`
- Modify: `src/widgets/UnassignedTreeView.h` and `.cpp`
- Modify: `src/widgets/MinisteringTabView.h` and `.cpp`
- Modify: `src/widgets/MinisteringView.h` and `.cpp`
- Modify: `src/widgets/shared/FilterBar.cpp` (if it passes IDs)

**MainWindow.h/.cpp:**
- `onEditFamilyRequested(const QString&)` → `onEditFamilyRequested(const FamilyId&)` (line 55)
- `onDeleteFamilyRequested(const QString&)` → `onDeleteFamilyRequested(const FamilyId&)` (line 56)
- `openEditPanel(const QString&)` → `openEditPanel(const FamilyId&)` (line 67)
- `onDocumentChanged()`: update for new DocumentChange structure
- `PlaceholderView` (internal class in MainWindow.cpp, line 39): inherits `FamilyMarkerProvider` — its `visibleFamilyIds()` override return type changes to `QSet<FamilyId>`

**WardListView.h:**
- `visibleFamilyIds()`: return `QSet<FamilyId>` (override, line 28)
- `selectedFamilyId()` (line 30): return `std::optional<FamilyId>` — no selection is a valid state, and a default-constructed `FamilyId` would `Q_ASSERT` fail in `toString()`. Callers use `if (auto id = selectedFamilyId()) { ... }`.
- `setSelectedFamilyId(const QString&)` → `setSelectedFamilyId(const std::optional<FamilyId>&)` (line 31) — pass `std::nullopt` to clear selection
- `visibleFamilyIdsList()`: return `QList<FamilyId>` (line 32)
- `visibleFamiliesChanged(const QStringList&)` signal → `visibleFamiliesChanged(const QList<FamilyId>&)` (line 38)
- `editFamilyRequested(const QString&)` → `editFamilyRequested(const FamilyId&)` (line 39)
- `deleteFamilyRequested(const QString&)` → `deleteFamilyRequested(const FamilyId&)` (line 40)
- `detachActionButtons(const QString&)` → `detachActionButtons(const FamilyId&)` (line 52)
- `m_actionWidgets` (line 58): `QHash<QString, ActionButtonsWidget*>` → `QHash<FamilyId, ActionButtonsWidget*>`

**WardListDialog.h:**
- `setPreselectedIds(const QStringList&)` → split into `setPreselectedFamilyIds(const QList<FamilyId>&)` and `setPreselectedPersonIds(const QList<PersonId>&)` (line 47)
- `selectedIds()` → split into `selectedFamilyIds()` → `QList<FamilyId>` and `selectedPersonIds()` → `QList<PersonId>` (line 50)
- `selectFamily()` → returns `std::optional<FamilyId>` (line 59)
- `selectFamilies()` → returns `QList<FamilyId>` (line 64)
- `selectPerson()` → returns `std::optional<PersonId>` (line 69)
- `selectPersons()` → returns `QList<PersonId>` (line 74)
- `onMapFamilyClicked(const QString&)` → `onMapFamilyClicked(const FamilyId&)` (line 79)
- `familyIdForCurrentSelection()` → returns `std::optional<FamilyId>` (line 84)
- `visibleFamilyIds()`: return `QSet<FamilyId>` (override)

**EmergencyAssetView.h:**
- `m_contextAssetId` (line 63): `QString` → `EmergencyAssetId`
- `m_contextPersonId` (line 64): `QString` → `PersonId`
- `showSelectPeopleDialog(const QString&)` → `showSelectPeopleDialog(const EmergencyAssetId&)` (line 49)
- `removePersonFromAsset(const QString&, const QString&)` → `removePersonFromAsset(const EmergencyAssetId&, const PersonId&)` (line 50)
- `selectedAssetId()` → returns `EmergencyAssetId` (line 52)
- `visibleFamilyIds()`: return `QSet<FamilyId>` (override)

**NeedsSubView.h:**
- `showEditNeedDialog(const QString&, const QString&)` → `showEditNeedDialog(const PersonId&, const FamilyId&)` (line 37)
- `deleteNeed(const QString&, const QString&)` → `deleteNeed(const PersonId&, const FamilyId&)` (line 38)
- `visibleFamilyIds()`: return `QSet<FamilyId>` (override)

**MapWidget.h:**
- `familyClicked(const QString&)` signal → `familyClicked(const FamilyId&)` (line 56)
- `markerAtPoint()` → returns `std::optional<FamilyId>` (line 98)
- `ensureVisible(const QSet<QString>&)` → `ensureVisible(const QSet<FamilyId>&)` (line 106)

**UnmappedPanel.h:**
- `MarkerLayout::familyId` (line 55): `QString` → `FamilyId`
- `familyClicked(const QString&)` signal → `familyClicked(const FamilyId&)` (line 39)
- `markerAtPoint()` → returns `std::optional<FamilyId>` (line 62)

**MinisteringTreeView.h, UnassignedTreeView.h, MinisteringTabView.h, MinisteringView.h:**
- `visibleFamilyIds()` override: return `QSet<FamilyId>` (all of these inherit from FamilyMarkerProvider)

**FilterBar.cpp:** Check if it passes IDs — it manipulates Filter setters. If `FilterBar` calls `filter->setTagIds(...)` with `QSet<QString>`, it needs to convert to `QSet<TagId>`. Read the file to determine exact changes.

**All .cpp files:** Update implementations to use typed IDs.

**Step: Commit**

```
git commit -m "refactor: convert all view widgets to strong ID types"
```

---

## Task 13: Convert MapViewModel

**Files:**
- Modify: `src/viewmodels/MapViewModel.h` and `.cpp`

**MapViewModel.h:**
- `m_familyIcons` (line 91): `QHash<QString, MarkerRenderer::MarkerIcons>` → `QHash<FamilyId, MarkerRenderer::MarkerIcons>`
- `m_selectedId` (line 97): `QString` → `std::optional<FamilyId>` — no selection is a valid state; a default-constructed `FamilyId` would `Q_ASSERT` fail in `toString()`
- `selectedFamilyId()` (line 45): return `std::optional<FamilyId>`
- `setSelectedFamilyId(const QString&)` → `setSelectedFamilyId(const std::optional<FamilyId>&)` (line 52) — pass `std::nullopt` to clear selection
- `selectFamily(const QString&)` → `selectFamily(const FamilyId&)` (line 55)
- `centerOnFamily(const QString&)` → `centerOnFamily(const FamilyId&)` (line 56)
- `familyIcons(const QString&)` → `familyIcons(const FamilyId&)` (line 65)
- `familyClicked(const QString&)` signal → `familyClicked(const FamilyId&)` (line 76)
- **Remove dead code:** `m_highlightedIds` (QVariantMap, line 92), `highlightedIds` Q_PROPERTY (line 24), `setHighlighting()` (line 58), `clearHighlighting()` (line 59). Keep `highlightingChanged` signal (triggers repaint).
- **Remove Q_PROPERTY** for `selectedFamilyId` (line 33) — no QML bindings exist, only used from C++ code. Just use normal C++ methods.
- **Remove Q_INVOKABLE** from `selectFamily()` (line 55), `centerOnFamily()` (line 56), `fitAllFamilies()`, `mapClicked()`, `setHighlighting()`, `clearHighlighting()` — no QML invocations exist. Removing these avoids MOC needing to know about `FamilyId` for invokable metadata.
- `onDocumentChanged()`: update for new DocumentChange structure

**MapViewModel.cpp:** Update `onDocumentChanged()` for new DocumentChange structure, update `familyToVariant()` to use typed IDs.

**Step: Commit**

```
git commit -m "refactor: convert MapViewModel to strong ID types, remove dead highlighting code"
```

---

## Task 14: Convert tools/test_import.cpp

**Files:**
- Modify: `tools/test_import.cpp`

This is a developer tool for testing PDF imports from the command line. It uses Document, Family, Person, MinisteringDistrict, MinisteringGroup, WardDirectoryImportService, and MinisteringImportService — all of which have typed IDs after prior tasks.

**Changes:**

Variable declarations:
- `QHash<QString, Family>()` → `QHash<FamilyId, Family>()` (line 108: empty existing families passed to `importFromPdf`)
- `QHash<QString, Family> existing` → `QHash<FamilyId, Family> existing` (line 187)
- `QHash<QString, QString> personToFamilyMap` → `QHash<PersonId, FamilyId> personToFamilyMap` (lines 211, 347)
- `QHash<QString, Family> familyMap` → `QHash<FamilyId, Family> familyMap` (line 346)

Loop variables (range-for over model accessors — types flow from the model changes):
- `for (const QString& groupId : district.groupIds())` → `for (const MinisteringGroupId& groupId : district.groupIds())` (lines 233, 379)
- `for (const QString& ministerId : group.ministerIds())` → `for (const PersonId& ministerId : group.ministerIds())` (lines 252, 392)
- `for (const QString& personId : group.ministeredPersonIds())` → `for (const PersonId& personId : group.ministeredPersonIds())` (lines 274, 417)
- `for (const QString& famId : group.familyIds())` → `for (const FamilyId& famId : group.familyIds())` (lines 282, 434)

ID access in output strings — `.toString()` needed where IDs are printed:
- `qDebug() << "  ID:" << f.id()` — works via `operator<<` (QDebug overload in IdBase)
- `qDebug() << "  Group ID:" << groupId` — same, QDebug overload handles it
- String formatting with `%1.arg(ministerId)` etc. → `.arg(ministerId.toString())` (check all `qDebug` / `QString::arg` calls)

Map lookups — types match automatically after variable declaration changes:
- `personToFamilyMap.insert(p.id(), it.key())` — types flow from `PersonId` / `FamilyId`
- `personToFamilyMap.find(ministerId)` — `ministerId` is now `PersonId`, map key is `PersonId`
- `result.families.find(famId)` — `famId` is now `FamilyId`, hash key is `FamilyId`

Optional access:
- `district.presidencyMemberId().value()` — now returns `PersonId` (was `QString`), add `.toString()` if used in string formatting

**Step: Commit**

```
git commit -m "refactor: convert tools/test_import.cpp to strong ID types"
```

---

## Task 15: Build & Fix

**Step 1: User builds**

The user runs `build.bat` (Ctrl+Shift+B in VS Code).

**Step 2: Fix compilation errors**

Expect potential issues:
- Missing `#include "Id.h"` in files that now use typed IDs
- Implicit `QString` conversions that no longer work (need explicit `.toString()` or `::fromString()`)
- Signal/slot signature mismatches (Qt's meta-object system requires exact type match)
- `Q_DECLARE_METATYPE` needed for any ID type used in QVariant or signal/slot connections across threads
- `data()` methods in models that return IDs as QVariant — may need `QVariant::fromValue<PersonId>(...)` or just `.toString()`

**Step 3: Final commit**

```
git commit -m "fix: resolve compilation errors from strong ID types conversion"
```

---

## Summary of All Files Modified

| Layer | Files |
|-------|-------|
| **Foundation** | `Id.h` (new), `CMakeLists.txt` |
| **DocumentChange** | `DocumentChange.h`, `DocumentChange.cpp` |
| **Models** | `Person.h/.cpp`, `Family.h/.cpp`, `Team.h/.cpp`, `Tag.h/.cpp`, `EmergencyAsset.h/.cpp`, `MinisteringGroup.h/.cpp`, `MinisteringDistrict.h/.cpp` |
| **Document** | `Document.h/.cpp` |
| **Commands** | `FamilyCommands.h/.cpp`, `TagCommands.h/.cpp`, `TeamCommands.h/.cpp`, `EmergencyAssetCommands.h/.cpp`, `ImportWardDirectoryCommand.h/.cpp`, `ImportEQMinisteringCommand.h/.cpp`, `ImportRSMinisteringCommand.h/.cpp` |
| **Services** | `PersonMatching.h/.cpp`, `WardDirectoryImportService.h/.cpp`, `MinisteringImportService.h/.cpp`, `BackgroundGeocodingService.h/.cpp`, `DocumentManager.h/.cpp` |
| **Filter/Base** | `Filter.h/.cpp`, `BaseTreeModel.h`, `SelectionPreservingTreeView.h/.cpp` |
| **List Models** | `FamilyTreeModel.h/.cpp`, `PersonTreeModel.h/.cpp`, `NeedsModel.h/.cpp`, `TeamListModel.h/.cpp`, `TagListModel.h/.cpp`, `EmergencyAssetModel.h/.cpp`, `MinisteringModel.h/.cpp`, `UnassignedMinisteringModel.h/.cpp` |
| **Widgets** | `FamilyMarkerProvider.h`, `MarkerRenderer.h/.cpp`, `ActionButtonsWidget.h/.cpp`, `FamilyEditPanel.h/.cpp`, `MemberEditor.h/.cpp`, `MainWindow.h/.cpp`, `WardListView.h/.cpp`, `WardListDialog.h/.cpp`, `EmergencyAssetView.h/.cpp`, `NeedsSubView.h/.cpp`, `MapWidget.h/.cpp`, `UnmappedPanel.h/.cpp`, `MinisteringTreeView.h/.cpp`, `UnassignedTreeView.h/.cpp`, `MinisteringTabView.h/.cpp`, `MinisteringView.h/.cpp`, `FilterBar.cpp` |
| **ViewModels** | `MapViewModel.h/.cpp` |
| **Tools** | `tools/test_import.cpp` |

**Total: ~61 files modified, 1 file created**
