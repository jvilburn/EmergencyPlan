# Strong ID Types + EmergencyAsset Rename — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace QString-based entity IDs with strongly-typed CRTP ID classes, rename EmergencyResource to EmergencyAsset, and split Tag's mixed entityIds into separate typed sets.

**Architecture:** CRTP base class `IdBase<Derived>` in `src/models/Id.h` provides shared behavior (generate, fromString, null, isNull, toString, operator==, qHash, QDebug). Seven derived ID classes provide compile-time type safety. DocumentChange::entityId stays as QString (cross-cutting notification mechanism — callers pass `.toString()`).

**Tech Stack:** C++17, Qt 6, MSVC 2022

**Design doc:** `docs/plans/2026-02-11-strong-id-types-design.md`

**Build note:** Do NOT build from Claude. The user builds manually via `build.bat` or Ctrl+Shift+B.

---

## Task 1: Create `src/models/Id.h`

**Files:**
- Create: `src/models/Id.h`

**Step 1: Create the header file**

```cpp
#pragma once

#include <QDebug>
#include <QString>
#include <QUuid>

template<typename Derived>
class IdBase
{
    QString m_value;

protected:
    explicit IdBase(const QString& value) : m_value(value) {}

public:
    static Derived generate()
    {
        return Derived(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    static Derived fromString(const QString& value) { return Derived(value); }
    static Derived null() { return Derived(QString()); }

    bool isNull() const { return m_value.isEmpty(); }
    const QString& toString() const { return m_value; }

    friend bool operator==(const Derived& a, const Derived& b)
    {
        return a.m_value == b.m_value;
    }

    friend bool operator!=(const Derived& a, const Derived& b)
    {
        return a.m_value != b.m_value;
    }

    friend size_t qHash(const Derived& id, size_t seed = 0)
    {
        return qHash(id.m_value, seed);
    }

    friend QDebug operator<<(QDebug dbg, const Derived& id)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace() << Derived::typeName() << '(' << id.m_value << ')';
        return dbg;
    }
};

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
```

**Step 2: Add to CMakeLists.txt**

In `MODEL_HEADERS`, add `src/models/Id.h` after the existing headers.

**Step 3: Commit**

```
git add src/models/Id.h CMakeLists.txt
git commit -m "feat: add CRTP-based strong ID types (Id.h)"
```

---

## Task 2: Rename EmergencyResource to EmergencyAsset

This is a pure rename — no ID type changes yet. Do the file renames first, then update all content.

**Files to rename (8 files):**
- `src/models/EmergencyResource.h` → `src/models/EmergencyAsset.h`
- `src/models/EmergencyResource.cpp` → `src/models/EmergencyAsset.cpp`
- `src/commands/EmergencyResourceCommands.h` → `src/commands/EmergencyAssetCommands.h`
- `src/commands/EmergencyResourceCommands.cpp` → `src/commands/EmergencyAssetCommands.cpp`
- `src/listmodels/EmergencyResourceModel.h` → `src/listmodels/EmergencyAssetModel.h`
- `src/listmodels/EmergencyResourceModel.cpp` → `src/listmodels/EmergencyAssetModel.cpp`
- `src/widgets/EmergencyResourceView.h` → `src/widgets/EmergencyAssetView.h`
- `src/widgets/EmergencyResourceView.cpp` → `src/widgets/EmergencyAssetView.cpp`

**Files to modify (content updates):**
- All 8 renamed files (class names, method names, variable names, includes)
- `src/models/Document.h` — methods, members, includes
- `src/models/Document.cpp` — all EmergencyResource references
- `src/models/DocumentChange.h` — `ChangeScope::EmergencyResource` → `ChangeScope::EmergencyAsset`
- `src/models/DocumentChange.cpp` — `emergencyResource()` → `emergencyAsset()`
- `src/widgets/MainWindow.h` — member variable type, include
- `src/widgets/MainWindow.cpp` — constructor, includes
- `src/widgets/shared/FilterBar.h` (if applicable)
- `src/widgets/shared/FilterBar.cpp` — UI strings "Resource" → "Asset"
- `src/listmodels/Filter.h` — `resourceTypeIds` → `assetTypeIds`
- `src/listmodels/Filter.cpp` — matching renames
- `src/commands/ImportWardDirectoryCommand.cpp` — if it references EmergencyResource
- `CMakeLists.txt` — all 8 file paths

**Step 1: Rename files with git mv**

```bash
git mv src/models/EmergencyResource.h src/models/EmergencyAsset.h
git mv src/models/EmergencyResource.cpp src/models/EmergencyAsset.cpp
git mv src/commands/EmergencyResourceCommands.h src/commands/EmergencyAssetCommands.h
git mv src/commands/EmergencyResourceCommands.cpp src/commands/EmergencyAssetCommands.cpp
git mv src/listmodels/EmergencyResourceModel.h src/listmodels/EmergencyAssetModel.h
git mv src/listmodels/EmergencyResourceModel.cpp src/listmodels/EmergencyAssetModel.cpp
git mv src/widgets/EmergencyResourceView.h src/widgets/EmergencyAssetView.h
git mv src/widgets/EmergencyResourceView.cpp src/widgets/EmergencyAssetView.cpp
```

**Step 2: Update renamed files — class names, method names, variables**

In each renamed file, replace:
- `EmergencyResource` → `EmergencyAsset` (class name, type references)
- `emergencyResource` → `emergencyAsset` (method names, variable names)
- `m_resource` → `m_asset` (member variables in commands)
- `resource` → `asset` (local variable names)
- Include guards / self-includes updated

**Step 3: Update command class names**

- `AddEmergencyResourceCommand` → `AddEmergencyAssetCommand`
- `UpdateEmergencyResourceCommand` → `UpdateEmergencyAssetCommand`
- `DeleteEmergencyResourceCommand` → `DeleteEmergencyAssetCommand`
- `AssignEmergencyResourceToPersonCommand` → `AssignEmergencyAssetToPersonCommand`
- `UnassignEmergencyResourceFromPersonCommand` → `UnassignEmergencyAssetFromPersonCommand`

**Step 4: Update command member variables**

In the assign/unassign commands:
- `m_resourceId` → `m_assetId`
- Parameter names `resourceId` → `assetId`

**Step 5: Update UI strings (tr() calls)**

In `EmergencyAssetView.cpp`:
- `tr("Add Resource...")` → `tr("Add Asset...")`
- `tr("Add Resource")` (dialog title) → `tr("Add Asset")`
- `tr("Resource name:")` → `tr("Asset name:")`
- `tr("Rename Resource")` → `tr("Rename Asset")`
- `tr("Delete resource \"%1\"?")` → `tr("Delete asset \"%1\"?")`
- `tr("Delete Resource")` → `tr("Delete Asset")`

In `EmergencyAssetCommands.cpp`:
- `tr("Add Resource \"%1\"")` → `tr("Add Asset \"%1\"")`
- `tr("Update Resource \"%1\"")` → `tr("Update Asset \"%1\"")`
- `tr("Delete Resource \"%1\"")` → `tr("Delete Asset \"%1\"")`
- `tr("Assign Resource to Person")` → `tr("Assign Asset to Person")`
- `tr("Remove Resource from Person")` → `tr("Remove Asset from Person")`

In `FilterBar.cpp`:
- `tr("Resource")` chip label → `tr("Asset")`

**Step 6: Update Document.h/cpp**

- `#include "EmergencyResource.h"` → `#include "EmergencyAsset.h"`
- `QHash<QString, EmergencyResource>` → `QHash<QString, EmergencyAsset>` (strong IDs come in Task 4)
- All method names: `addEmergencyResource` → `addEmergencyAsset`, etc.
- All variable names: `m_emergencyResources` → `m_emergencyAssets`
- Comments updated

**Step 7: Update DocumentChange.h/cpp**

- `ChangeScope::EmergencyResource` → `ChangeScope::EmergencyAsset`
- `DocumentChange::emergencyResource()` → `DocumentChange::emergencyAsset()`

**Step 8: Update all other files that include or reference EmergencyResource**

- `src/widgets/MainWindow.h/cpp` — include, member type, any references
- `src/listmodels/Filter.h/cpp` — `resourceTypeIds`/`m_resourceTypeIds` → `assetTypeIds`/`m_assetTypeIds`
- `src/commands/ImportWardDirectoryCommand.cpp` — if it references EmergencyResource types
- Any other files found by searching for `EmergencyResource`

**Step 9: Update CMakeLists.txt**

Replace all 8 file paths in `MODEL_SOURCES`, `MODEL_HEADERS`, `COMMAND_SOURCES`, `COMMAND_HEADERS`, `WIDGET_SOURCES`, `WIDGET_HEADERS`, `LISTMODEL_SOURCES`, `LISTMODEL_HEADERS`.

**Step 10: Commit**

```
git add -A
git commit -m "refactor: rename EmergencyResource to EmergencyAsset"
```

---

## Task 3: Split Tag entityIds into personIds + familyIds

**Files:**
- Modify: `src/models/Tag.h`
- Modify: `src/models/Tag.cpp`
- Modify: `src/models/Document.cpp:152-186` (addPersonToTag, etc.)
- Modify: `src/models/Document.cpp:344-349` (cleanupPersonReferences)
- Modify: `src/commands/TagCommands.cpp` (execute/undo calls)
- Modify: `src/commands/ImportWardDirectoryCommand.cpp:145-153`
- Modify: `src/listmodels/TagListModel.cpp` (entityIds().size() → personIds().size() + familyIds().size())

**Step 1: Update Tag.h**

Replace `m_entityIds` with two sets and typed accessors:

```cpp
// Replace these members:
//   QSet<QString> m_entityIds;
// With:
    QSet<QString> m_personIds;
    QSet<QString> m_familyIds;

// Replace accessors:
//   const QSet<QString>& entityIds() const;
//   void setEntityIds(const QSet<QString>& entityIds);
//   void addEntity(const QString& entityId);
//   void removeEntity(const QString& entityId);
//   bool hasEntity(const QString& entityId) const;
//   bool isEmpty() const;
// With:
    const QSet<QString>& personIds() const { return m_personIds; }
    const QSet<QString>& familyIds() const { return m_familyIds; }
    void setPersonIds(const QSet<QString>& ids) { m_personIds = ids; }
    void setFamilyIds(const QSet<QString>& ids) { m_familyIds = ids; }
    void addPerson(const QString& personId) { m_personIds.insert(personId); }
    void removePerson(const QString& personId) { m_personIds.remove(personId); }
    void addFamily(const QString& familyId) { m_familyIds.insert(familyId); }
    void removeFamily(const QString& familyId) { m_familyIds.remove(familyId); }
    bool hasPerson(const QString& personId) const { return m_personIds.contains(personId); }
    bool hasFamily(const QString& familyId) const { return m_familyIds.contains(familyId); }
    bool isEmpty() const { return m_personIds.isEmpty() && m_familyIds.isEmpty(); }
```

**Step 2: Update Tag.cpp — toJson**

Serialize both sets under existing key for backwards compatibility:

```cpp
// In toJson(), replace the entityIds block with:
if (!m_personIds.isEmpty() || !m_familyIds.isEmpty())
{
    QJsonArray entityIdsArray;
    for (const QString& id : m_personIds)
    {
        entityIdsArray.append(id);
    }
    for (const QString& id : m_familyIds)
    {
        entityIdsArray.append(id);
    }
    json["entityIds"] = entityIdsArray;
}
```

**Step 3: Update Tag.cpp — fromJson**

Deserialize into the correct set based on tag level:

```cpp
// In fromJson(), replace the entityIds block with:
if (json.contains("entityIds"))
{
    QJsonArray entityIdsArray = json["entityIds"].toArray();
    for (const QJsonValue& value : entityIdsArray)
    {
        if (tag.m_level == TagLevel::Family)
        {
            tag.m_familyIds.insert(value.toString());
        }
        else
        {
            tag.m_personIds.insert(value.toString());
        }
    }
}
```

**Step 4: Update Tag.cpp — operator==**

Replace `m_entityIds == other.m_entityIds` with `m_personIds == other.m_personIds && m_familyIds == other.m_familyIds`.

**Step 5: Update Document.cpp callers**

- `addPersonToTag`: change `it->addEntity(personId)` → `it->addPerson(personId)`
- `removePersonFromTag`: change `it->removeEntity(personId)` → `it->removePerson(personId)`
- `addFamilyToTag`: change `it->addEntity(familyId)` → `it->addFamily(familyId)`
- `removeFamilyFromTag`: change `it->removeEntity(familyId)` → `it->removeFamily(familyId)`
- `cleanupPersonReferences`: change `it->entityIds().contains(personId)` → `it->hasPerson(personId)` and `it->removeEntity(personId)` → `it->removePerson(personId)`

**Step 6: Update ImportWardDirectoryCommand.cpp**

- `tag.entityIds().contains(familyId)` → `tag.hasFamily(familyId)`
- `tag.entityIds().contains(member.id())` → `tag.hasPerson(member.id())`
- `document.removeFamilyFromTag` / `document.removePersonFromTag` — unchanged (Document API already separated)

**Step 7: Update TagListModel.cpp**

- `tag.entityIds().size()` → `tag.personIds().size() + tag.familyIds().size()`

**Step 8: Commit**

```
git add src/models/Tag.h src/models/Tag.cpp src/models/Document.cpp \
       src/commands/TagCommands.cpp src/commands/ImportWardDirectoryCommand.cpp \
       src/listmodels/TagListModel.cpp
git commit -m "refactor: split Tag entityIds into separate personIds and familyIds"
```

---

## Task 4: Update model internal types to strong IDs

All 7 models + Document get their member types changed. This task changes the internal representation and JSON serialization. It will temporarily break downstream code (commands, services, widgets) which is fixed in subsequent tasks.

**Files:**
- Modify: `src/models/Person.h`, `src/models/Person.cpp`
- Modify: `src/models/Family.h`, `src/models/Family.cpp`
- Modify: `src/models/Team.h`, `src/models/Team.cpp`
- Modify: `src/models/Tag.h`, `src/models/Tag.cpp`
- Modify: `src/models/EmergencyAsset.h`, `src/models/EmergencyAsset.cpp`
- Modify: `src/models/MinisteringGroup.h`, `src/models/MinisteringGroup.cpp`
- Modify: `src/models/MinisteringDistrict.h`, `src/models/MinisteringDistrict.cpp`
- Modify: `src/models/Document.h`, `src/models/Document.cpp`

### Person

**Person.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id() const` → `const PersonId& id() const`
- `createWithId(const QString& id, ...)` → `createWithId(const PersonId& id, ...)`
- `QString m_id` → `PersonId m_id`

**Person.cpp changes:**
- `create()`: `person.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces)` → `person.m_id = PersonId::generate()`
- `createWithId()`: `person.m_id = id` (unchanged, now PersonId = PersonId)
- `toJson()`: `json["id"] = m_id` → `json["id"] = m_id.toString()`
- `fromJson()`: `person.m_id = json["id"].toString()` → `person.m_id = PersonId::fromString(json["id"].toString())`

### Family

**Family.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id() const` → `const FamilyId& id() const`
- `createWithId(const QString& id, ...)` → `createWithId(const FamilyId& id, ...)`
- `QString m_id` → `FamilyId m_id`

**Family.cpp changes:**
- `create()`: `family.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces)` → `family.m_id = FamilyId::generate()`
- `createWithId()`: `family.m_id = id` (unchanged)
- `toJson()`: `json["id"] = m_id` → `json["id"] = m_id.toString()`
- `fromJson()`: `family.m_id = json["id"].toString()` → `family.m_id = FamilyId::fromString(json["id"].toString())`

### Team

**Team.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id()` → `const TeamId& id()`
- `const QString& leaderId()` → `const PersonId& leaderId()`
- `const QSet<QString>& memberIds()` → `const QSet<PersonId>& memberIds()`
- `create(... const QString& leaderId = QString())` → `create(... const PersonId& leaderId = PersonId::null())`
- All setter/add/remove/has methods: `const QString&` → typed IDs
- `hasLeader()`: `!m_leaderId.isEmpty()` → `!m_leaderId.isNull()`
- Members: `QString m_id` → `TeamId m_id`, `QString m_leaderId` → `PersonId m_leaderId`, `QSet<QString> m_memberIds` → `QSet<PersonId> m_memberIds`

**Team.cpp changes:**
- `create()`: UUID generation → `TeamId::generate()`
- `toJson()`: `json["id"] = m_id` → `json["id"] = m_id.toString()`, `json["leaderId"] = m_leaderId` → `json["leaderId"] = m_leaderId.toString()`, iterate memberIds with `.toString()`
- `fromJson()`: All `.toString()` wrapped in `XxxId::fromString(...)`
- `isEmpty()` check for leader: `!m_leaderId.isEmpty()` → `!m_leaderId.isNull()`

### Tag

**Tag.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id()` → `const TagId& id()`
- `QString m_id` → `TagId m_id`
- `QSet<QString> m_personIds` → `QSet<PersonId> m_personIds`
- `QSet<QString> m_familyIds` → `QSet<FamilyId> m_familyIds`
- All person/family accessors: typed parameters

**Tag.cpp changes:**
- `create()`: UUID → `TagId::generate()`
- `toJson()`/`fromJson()`: use `.toString()` / `XxxId::fromString()`

### EmergencyAsset

**EmergencyAsset.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id()` → `const EmergencyAssetId& id()`
- `const QSet<QString>& personIds()` → `const QSet<PersonId>& personIds()`
- All person methods: `const QString&` → `const PersonId&`
- Members: `QString m_id` → `EmergencyAssetId m_id`, `QSet<QString> m_personIds` → `QSet<PersonId> m_personIds`

**EmergencyAsset.cpp changes:**
- `create()`: UUID → `EmergencyAssetId::generate()`
- `toJson()`/`fromJson()`: `.toString()` / `fromString()`

### MinisteringGroup

**MinisteringGroup.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id()` → `const MinisteringGroupId& id()`
- `const QSet<QString>& ministerIds()` → `const QSet<PersonId>& ministerIds()`
- `const QSet<QString>& familyIds()` → `const QSet<FamilyId>& familyIds()`
- `const QSet<QString>& ministeredPersonIds()` → `const QSet<PersonId>& ministeredPersonIds()`
- `std::optional<QString> presidencyMemberId()` → `std::optional<PersonId> presidencyMemberId()`
- All setters/add/remove: typed parameters
- Members: all updated

**MinisteringGroup.cpp changes:**
- Factory methods: UUID → `MinisteringGroupId::generate()`, parameter types updated
- `toJson()`: all `.toString()` on IDs, including `*m_presidencyMemberId` → `m_presidencyMemberId->toString()`
- `fromJson()`: all `fromString()` wrappers

### MinisteringDistrict

**MinisteringDistrict.h changes:**
- Add `#include "Id.h"`, remove `#include <QUuid>`
- `const QString& id()` → `const MinisteringDistrictId& id()`
- `std::optional<QString> presidencyMemberId()` → `std::optional<PersonId>`
- `const QSet<QString>& groupIds()` → `const QSet<MinisteringGroupId>& groupIds()`
- All setters/add/remove: typed
- Factory: `create(... std::optional<QString>& presidencyMemberId ..., QSet<QString>& groupIds)` → typed
- Members: all updated

**MinisteringDistrict.cpp changes:**
- `create()`: UUID → `MinisteringDistrictId::generate()`, parameter types
- `toJson()`: `.toString()` on all IDs
- `fromJson()`: `fromString()` wrappers

### Document

**Document.h changes:**
- Add `#include "Id.h"`
- All `QHash<QString, Model>` → `QHash<TypedId, Model>`:
  - `QHash<QString, Family>` → `QHash<FamilyId, Family>`
  - `QHash<QString, Team>` → `QHash<TeamId, Team>`
  - `QHash<QString, Tag>` → `QHash<TagId, Tag>`
  - `QHash<QString, EmergencyAsset>` → `QHash<EmergencyAssetId, EmergencyAsset>`
  - `QHash<QString, MinisteringDistrict>` → `QHash<MinisteringDistrictId, MinisteringDistrict>` (4 hashes)
  - `QHash<QString, MinisteringGroup>` → `QHash<MinisteringGroupId, MinisteringGroup>` (4 hashes)
- `QHash<QString, QString> m_personToFamily` → `QHash<PersonId, FamilyId> m_personToFamily`
- `QHash<QString, QSet<ResponseArea>> m_personResponseAreas` → `QHash<PersonId, QSet<ResponseArea>>`
- All method signatures: `const QString& id` → typed IDs
- `familyIdForPerson` returns `FamilyId` (use `.isNull()` instead of `.isEmpty()` at call sites)

**Document.cpp changes:**
- `serializeHashToJson` / `deserializeJsonToHash` templates: Update to accept any key type. The key type just needs `.toString()` for serialization and `T::fromJson().id()` for deserialization (which already works since `id()` returns the typed ID). The template parameter list changes from `QHash<QString, T>` to a generic hash:

```cpp
template<typename K, typename V>
void serializeHashToJson(QJsonObject& json, const QString& key, const QHash<K, V>& hash)
{
    if (!hash.isEmpty())
    {
        QJsonArray array;
        for (const V& item : hash)
        {
            array.append(item.toJson());
        }
        json[key] = array;
    }
}

template<typename K, typename V>
void deserializeJsonToHash(const QJsonObject& json, const QString& key, QHash<K, V>& hash)
{
    if (json.contains(key))
    {
        QJsonArray array = json[key].toArray();
        for (const QJsonValue& value : array)
        {
            V item = V::fromJson(value.toObject());
            hash.insert(item.id(), item);
        }
    }
}
```

- All `remove*(const QString& id)` → `remove*(const TypedId& id)`
- `cleanupPersonReferences(const QString& personId)` → `cleanupPersonReferences(const PersonId& personId)`
- `cleanupPersonReferences` body: `it->setLeaderId(QString())` → `it->setLeaderId(PersonId::null())`
- `familyIdForPerson` → returns `FamilyId`, use `m_personToFamily.value(personId)` (default-constructed FamilyId is null)
- `findFamilyById(const QString&)` → `findFamilyById(const FamilyId&)`, etc.
- `personResponseAreas(const QString&)` → `personResponseAreas(const PersonId&)`
- `setFamilies` / `setEqDistricts` / etc.: parameter hash types updated
- `rebuildPersonToFamilyMap`: `m_personToFamily.insert(member.id(), family.id())` — works as-is since both return typed IDs now
- `rebuildDecorationCaches`: `for (const QString& personId : ...)` → `for (const PersonId& personId : ...)`

**Step: Commit**

```
git add src/models/
git commit -m "refactor: adopt strong ID types in all models and Document"
```

---

## Task 5: Update DocumentChange + Commands

**Files:**
- Modify: `src/models/DocumentChange.h` — ScopeBuilder methods stay as `const QString&` (callers pass `.toString()`)
- Modify: `src/commands/FamilyCommands.h`, `src/commands/FamilyCommands.cpp`
- Modify: `src/commands/TeamCommands.h`, `src/commands/TeamCommands.cpp`
- Modify: `src/commands/TagCommands.h`, `src/commands/TagCommands.cpp`
- Modify: `src/commands/EmergencyAssetCommands.h`, `src/commands/EmergencyAssetCommands.cpp`
- Modify: `src/commands/MinisteringCommands.h`, `src/commands/MinisteringCommands.cpp`
- Modify: `src/commands/ImportWardDirectoryCommand.h`, `src/commands/ImportWardDirectoryCommand.cpp`
- Modify: `src/commands/ImportEQMinisteringCommand.h`, `src/commands/ImportEQMinisteringCommand.cpp`
- Modify: `src/commands/ImportRSMinisteringCommand.h`, `src/commands/ImportRSMinisteringCommand.cpp`

### DocumentChange

ScopeBuilder methods already accept `const QString&`. Command callers will pass `.toString()`:
```cpp
// Before (was already QString):
return DocumentChange::team().added(m_team.id());
// After (id() now returns TeamId, so add .toString()):
return DocumentChange::team().added(m_team.id().toString());
```

### FamilyCommands

- `SetFamiliesCommand`: `QHash<QString, Family>` → `QHash<FamilyId, Family>`
- `documentChange()` calls: add `.toString()` where passing model IDs

### TeamCommands

- `AddTeamMemberCommand(const QString& teamId, const QString& memberId)` → `AddTeamMemberCommand(const TeamId& teamId, const PersonId& memberId)`
- `RemoveTeamMemberCommand`: same
- Members: `QString m_teamId` → `TeamId m_teamId`, `QString m_memberId` → `PersonId m_memberId`
- `documentChange()`: `m_teamId` → `m_teamId.toString()`

### TagCommands

- `AssignTagToPersonCommand(const QString& tagId, const QString& personId)` → `(const TagId&, const PersonId&)`
- `AssignTagToFamilyCommand(const QString& tagId, const QString& familyId)` → `(const TagId&, const FamilyId&)`
- `UnassignTagFromPersonCommand` / `UnassignTagFromFamilyCommand`: same pattern
- Members: typed IDs
- `documentChange()`: `.toString()`

### EmergencyAssetCommands

- `AssignEmergencyAssetToPersonCommand(const QString& assetId, const QString& personId)` → `(const EmergencyAssetId&, const PersonId&)`
- `UnassignEmergencyAssetFromPersonCommand`: same
- Members: `QString m_assetId` → `EmergencyAssetId m_assetId`, `QString m_personId` → `PersonId m_personId`
- `documentChange()`: `.toString()`
- In `execute()`/`undo()`: `document.findEmergencyAssetById(m_assetId)` — already typed after Task 4

### MinisteringCommands

- These commands all take full model objects (MinisteringDistrict, MinisteringGroup), not raw IDs. The changes are only in `documentChange()` calls: add `.toString()`.

### Import Commands

- `ImportWardDirectoryCommand`: update any ID variable declarations to typed IDs where they interact with model APIs that now require them
- `ImportEQMinisteringCommand` / `ImportRSMinisteringCommand`: same pattern

**Step: Commit**

```
git add src/commands/ src/models/DocumentChange.h
git commit -m "refactor: adopt strong ID types in all commands"
```

---

## Task 6: Update Services

**Files:**
- Modify: `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Modify: `src/services/BackgroundGeocodingService.h`, `src/services/BackgroundGeocodingService.cpp`
- Modify: `src/services/WardDirectoryImportService.h`, `src/services/WardDirectoryImportService.cpp`
- Modify: `src/services/MinisteringImportService.h`, `src/services/MinisteringImportService.cpp`
- Modify: `src/services/PersonMatching.h`, `src/services/PersonMatching.cpp`

### DocumentManager

- `onFamilyGeocoded(const QString& id, ...)` → `onFamilyGeocoded(const FamilyId& id, ...)`
- Ward/Stake unit number sets stay as `QSet<QString>` (not entity IDs)

### BackgroundGeocodingService

- Any `familyId` parameters → `FamilyId`

### WardDirectoryImportService

- Family/Person ID handling in import logic → typed IDs
- This is the most complex service change — read the file carefully and update all ID variables

### MinisteringImportService

- Group/district/person ID handling → typed IDs

### PersonMatching

- Person ID parameters → `PersonId`

**Step: Commit**

```
git add src/services/
git commit -m "refactor: adopt strong ID types in all services"
```

---

## Task 7: Update ListModels + Filter

**Files:**
- Modify: `src/listmodels/Filter.h`, `src/listmodels/Filter.cpp`
- Modify: `src/listmodels/BaseTreeModel.h`, `src/listmodels/BaseTreeModel.cpp`
- Modify: `src/listmodels/EmergencyAssetModel.h`, `src/listmodels/EmergencyAssetModel.cpp`
- Modify: `src/listmodels/PersonTreeModel.h`, `src/listmodels/PersonTreeModel.cpp`
- Modify: `src/listmodels/FamilyTreeModel.h`, `src/listmodels/FamilyTreeModel.cpp`
- Modify: `src/listmodels/MinisteringModel.h`, `src/listmodels/MinisteringModel.cpp`
- Modify: `src/listmodels/NeedsModel.h`, `src/listmodels/NeedsModel.cpp`
- Modify: `src/listmodels/UnassignedMinisteringModel.h`, `src/listmodels/UnassignedMinisteringModel.cpp`
- Modify: `src/listmodels/TeamListModel.h`, `src/listmodels/TeamListModel.cpp`
- Modify: `src/listmodels/TagListModel.h`, `src/listmodels/TagListModel.cpp`

### Filter

- `QSet<QString> m_tagIds` → `QSet<TagId> m_tagIds`
- `QSet<QString> m_teamIds` → `QSet<TeamId> m_teamIds`
- `QSet<QString> m_assetTypeIds` → `QSet<EmergencyAssetId> m_assetTypeIds`
- All getters/setters updated
- Helper methods: `hasFamilyLevelTag(... const QString& familyId)` → `const FamilyId&`, etc.

### BaseTreeModel

- `QString idAt(...)` → decide: this is a generic base used by multiple models with different ID types. Keep as `QString` and have subclasses return `.toString()`, OR template it. **Recommendation:** keep as `QString` since the return value is used for selection tracking which doesn't need type safety.

### List model tree nodes

- Internal node structs (`PersonNode`, `FamilyNode`, etc.) — update `QString personId` / `QString familyId` fields to typed IDs where they store entity IDs.

**Step: Commit**

```
git add src/listmodels/
git commit -m "refactor: adopt strong ID types in list models and filter"
```

---

## Task 8: Update Widgets + ViewModels

**Files:**
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/widgets/EmergencyAssetView.h`, `src/widgets/EmergencyAssetView.cpp`
- Modify: `src/widgets/FamilyEditPanel.h`, `src/widgets/FamilyEditPanel.cpp`
- Modify: `src/widgets/MapWidget.h`, `src/widgets/MapWidget.cpp`
- Modify: `src/widgets/MarkerRenderer.h`, `src/widgets/MarkerRenderer.cpp`
- Modify: `src/widgets/NeedsSubView.h`, `src/widgets/NeedsSubView.cpp`
- Modify: `src/widgets/WardListView.h`, `src/widgets/WardListView.cpp`
- Modify: `src/widgets/WardListDialog.h`, `src/widgets/WardListDialog.cpp`
- Modify: `src/widgets/MinisteringView.h`, `src/widgets/MinisteringView.cpp`
- Modify: `src/widgets/MinisteringTreeView.h`, `src/widgets/MinisteringTreeView.cpp`
- Modify: `src/widgets/MinisteringTabView.h`, `src/widgets/MinisteringTabView.cpp`
- Modify: `src/widgets/UnassignedTreeView.h`, `src/widgets/UnassignedTreeView.cpp`
- Modify: `src/widgets/SidebarWidget.h`, `src/widgets/SidebarWidget.cpp`
- Modify: `src/widgets/shared/FilterBar.h`, `src/widgets/shared/FilterBar.cpp`
- Modify: `src/viewmodels/MapViewModel.h`, `src/viewmodels/MapViewModel.cpp`

### General pattern for widgets

Most widget methods that take `const QString&` for IDs get updated to the typed ID. Signal/slot connections that pass IDs through need matching types on both ends.

### MapViewModel

- `QString m_selectedFamilyId` → `FamilyId m_selectedFamilyId`
- `selectFamily(const QString&)` → `selectFamily(const FamilyId&)`
- `centerOnFamily(const QString&)` → `centerOnFamily(const FamilyId&)`
- `familyClicked(const QString&)` signal → `familyClicked(const FamilyId&)`
- `selectedFamilyId` property returns FamilyId
- `familyIcons(const QString&)` → `familyIcons(const FamilyId&)`

### MainWindow

- `onEditFamilyRequested(const QString&)` → `onEditFamilyRequested(const FamilyId&)`
- `onDeleteFamilyRequested(const QString&)` → `onDeleteFamilyRequested(const FamilyId&)`
- `openEditPanel(const QString&)` → `openEditPanel(const FamilyId&)`

### EmergencyAssetView

- All `const QString&` ID parameters → typed IDs
- Local variables storing IDs from model lookups → typed

### FamilyEditPanel

- `familyId` parameters → `FamilyId`

### FilterBar

- Tag/team/asset ID handling → typed IDs

**Step: Commit**

```
git add src/widgets/ src/viewmodels/
git commit -m "refactor: adopt strong ID types in widgets and viewmodels"
```

---

## Task 9: Build verification + fixups

**Step 1: User builds the project**

The user runs `build.bat` or Ctrl+Shift+B. Any remaining compiler errors are fixed.

**Common issues to expect:**
- Missed `.toString()` calls where a strong ID is passed to a function still expecting `QString`
- `isEmpty()` vs `isNull()` — strong IDs use `isNull()`, but some code may still call `isEmpty()` on the underlying string via old patterns
- Template argument deduction issues in `serializeHashToJson`/`deserializeJsonToHash`
- Signal/slot signature mismatches (must match types exactly for Qt connections)
- `std::optional<QString>` → `std::optional<PersonId>` — default value `std::nullopt` stays the same, but comparisons like `== personId` need the optional to hold the right type

**Step 2: Fix all compiler errors**

**Step 3: Commit**

```
git add -A
git commit -m "fix: resolve remaining compiler errors from strong ID migration"
```

---

## Summary of commits

1. `feat: add CRTP-based strong ID types (Id.h)`
2. `refactor: rename EmergencyResource to EmergencyAsset`
3. `refactor: split Tag entityIds into separate personIds and familyIds`
4. `refactor: adopt strong ID types in all models and Document`
5. `refactor: adopt strong ID types in all commands`
6. `refactor: adopt strong ID types in all services`
7. `refactor: adopt strong ID types in list models and filter`
8. `refactor: adopt strong ID types in widgets and viewmodels`
9. `fix: resolve remaining compiler errors from strong ID migration` (if needed)
