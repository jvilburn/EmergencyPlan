# Strong ID Types + EmergencyAsset Rename

## Overview

Replace all `QString`-based entity IDs with strongly-typed ID classes, and rename `EmergencyResource` to `EmergencyAsset`. Both are mechanical refactors with no behavioral changes.

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
```

The CRTP pattern ensures that `operator==` and `qHash` only accept the same derived type. Comparing a `PersonId` to a `FamilyId` is a compile error.

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
| `EmergencyAssetId` | EmergencyAsset (renamed from EmergencyResource) |
| `MinisteringGroupId` | MinisteringGroup, MinisteringDistrict group sets |
| `MinisteringDistrictId` | MinisteringDistrict |

Ward/Stake unit numbers remain `QString` -- they are external identifiers from church systems, not internally-generated UUIDs.

### Constructor Visibility

The `QString` constructor is **protected**. Public API for creating IDs:

- `PersonId::generate()` -- new UUID
- `PersonId::fromString(str)` -- deserialization from JSON
- `PersonId::null()` -- empty/absent ID

## Tag EntityIds Split

`Tag::m_entityIds` (a `QSet<QString>` holding either PersonIds or FamilyIds depending on tag level) splits into two typed sets:

- `QSet<PersonId> m_personIds`
- `QSet<FamilyId> m_familyIds`

Only one is populated based on tag level. This mirrors how `MinisteringGroup` already works with separate `m_ministerIds` / `m_familyIds` / `m_ministeredPersonIds`.

The generic `addEntity/removeEntity/hasEntity` methods become typed: `addPerson/removePerson/addFamily/removeFamily`. The Document-level API (`addPersonToTag`, `addFamilyToTag`) already has this separation.

## EmergencyResource Rename

`EmergencyResource` becomes `EmergencyAsset`. The sidebar tab is already labeled "Skills & Gear"; "asset" is the best single-word noun covering both skills and gear that someone contributes to emergency response.

This rename touches:
- Model: `EmergencyResource.h/cpp` -> `EmergencyAsset.h/cpp`
- Commands: `EmergencyResourceCommands.h/cpp` -> `EmergencyAssetCommands.h/cpp`
- List model: `EmergencyResourceModel.h/cpp` -> `EmergencyAssetModel.h/cpp`
- View: `EmergencyResourceView.h/cpp` -> `EmergencyAssetView.h/cpp`
- Document: all `emergencyResource*` methods and `m_emergencyResources` member
- ChangeScope enum: `EmergencyResource` -> `EmergencyAsset`
- UI strings: "Add Resource" -> "Add Asset", etc.
- CMakeLists.txt: updated file names

## Model Impact Summary

| Model | Changes |
|-------|---------|
| Person | `QString m_id` -> `PersonId m_id` |
| Family | `QString m_id` -> `FamilyId m_id` |
| Team | `TeamId m_id`, `PersonId m_leaderId`, `QSet<PersonId> m_memberIds` |
| Tag | `TagId m_id`, split `m_entityIds` into `QSet<PersonId>` + `QSet<FamilyId>` |
| EmergencyAsset | `EmergencyAssetId m_id`, `QSet<PersonId> m_personIds` |
| MinisteringGroup | `MinisteringGroupId m_id`, `QSet<PersonId> m_ministerIds`, `QSet<FamilyId> m_familyIds`, `QSet<PersonId> m_ministeredPersonIds`, `std::optional<PersonId> m_presidencyMemberId` |
| MinisteringDistrict | `MinisteringDistrictId m_id`, `std::optional<PersonId> m_presidencyMemberId`, `QSet<MinisteringGroupId> m_groupIds` |
| Document | All `QHash<QString, Model>` -> `QHash<TypedId, Model>`, `m_personToFamily` -> `QHash<PersonId, FamilyId>` |

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
- Any user-facing functionality (except "Resource" -> "Asset" in UI labels)
