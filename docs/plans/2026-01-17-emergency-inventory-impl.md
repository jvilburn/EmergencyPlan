# Emergency Inventory Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the unified ResourceCategory/ResourceType model with three distinct concepts: Skills, Equipment, and Special Needs.

**Architecture:** Remove old resource models and commands, add new SkillCategory/Skill, EquipmentCategory/Equipment, and SpecialNeed models. Update Document class with new collections and mutators. Update DocumentChange scopes. Create new commands for all CRUD operations.

**Tech Stack:** Qt 6, C++17, CMake with Ninja

---

## Task 1: Create SkillCategory Model

**Files:**
- Create: `src/models/SkillCategory.h`
- Create: `src/models/SkillCategory.cpp`

**Step 1: Create the header file**

Write to `src/models/SkillCategory.h`:

```cpp
#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>

class SkillCategory
{
public:
    SkillCategory() = default;

    // Factory method for creating new categories
    static SkillCategory create(const QString& name, int sortOrder);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    int sortOrder() const { return m_sortOrder; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }

    // JSON serialization
    QJsonObject toJson() const;
    static SkillCategory fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const SkillCategory& other) const;
    bool operator!=(const SkillCategory& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    int m_sortOrder = 0;
};
```

**Step 2: Create the implementation file**

Write to `src/models/SkillCategory.cpp`:

```cpp
#include "SkillCategory.h"

SkillCategory SkillCategory::create(const QString& name, int sortOrder)
{
    SkillCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    return category;
}

QJsonObject SkillCategory::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["sortOrder"] = m_sortOrder;
    return json;
}

SkillCategory SkillCategory::fromJson(const QJsonObject& json)
{
    SkillCategory category;
    category.m_id = json["id"].toString();
    category.m_name = json["name"].toString();
    category.m_sortOrder = json["sortOrder"].toInt(0);
    return category;
}

bool SkillCategory::operator==(const SkillCategory& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_sortOrder == other.m_sortOrder;
}
```

**Step 3: Update CMakeLists.txt**

Add to the SOURCES section (around line 98-99, after ResourceType.cpp):
```cmake
    src/models/SkillCategory.cpp
```

Add to the HEADERS section (around line 124-125, after ResourceType.h):
```cmake
    src/models/SkillCategory.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build succeeds with no errors

**Step 5: Commit**

```bash
git add src/models/SkillCategory.h src/models/SkillCategory.cpp CMakeLists.txt
git commit -m "feat: add SkillCategory model"
```

---

## Task 2: Create Skill Model

**Files:**
- Create: `src/models/Skill.h`
- Create: `src/models/Skill.cpp`

**Step 1: Create the header file**

Write to `src/models/Skill.h`:

```cpp
#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class Skill
{
public:
    Skill() = default;

    // Factory method for creating new skills
    static Skill create(const QString& name, const QString& categoryId);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& categoryId() const { return m_categoryId; }
    const QSet<QString>& personIds() const { return m_personIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setCategoryId(const QString& categoryId) { m_categoryId = categoryId; }
    void setPersonIds(const QSet<QString>& personIds) { m_personIds = personIds; }
    void addPerson(const QString& personId) { m_personIds.insert(personId); }
    void removePerson(const QString& personId) { m_personIds.remove(personId); }

    // Computed properties
    bool hasPerson(const QString& personId) const { return m_personIds.contains(personId); }
    bool isEmpty() const { return m_personIds.isEmpty(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Skill fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Skill& other) const;
    bool operator!=(const Skill& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_categoryId;
    QSet<QString> m_personIds;
};
```

**Step 2: Create the implementation file**

Write to `src/models/Skill.cpp`:

```cpp
#include "Skill.h"

Skill Skill::create(const QString& name, const QString& categoryId)
{
    Skill skill;
    skill.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    skill.m_name = name;
    skill.m_categoryId = categoryId;
    return skill;
}

QJsonObject Skill::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["categoryId"] = m_categoryId;

    QJsonArray personArray;
    for (const QString& personId : m_personIds)
    {
        personArray.append(personId);
    }
    json["personIds"] = personArray;

    return json;
}

Skill Skill::fromJson(const QJsonObject& json)
{
    Skill skill;
    skill.m_id = json["id"].toString();
    skill.m_name = json["name"].toString();
    skill.m_categoryId = json["categoryId"].toString();

    const QJsonArray personArray = json["personIds"].toArray();
    for (const QJsonValue& value : personArray)
    {
        skill.m_personIds.insert(value.toString());
    }

    return skill;
}

bool Skill::operator==(const Skill& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_categoryId == other.m_categoryId
        && m_personIds == other.m_personIds;
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after SkillCategory.cpp):
```cmake
    src/models/Skill.cpp
```

Add to HEADERS (after SkillCategory.h):
```cmake
    src/models/Skill.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/models/Skill.h src/models/Skill.cpp CMakeLists.txt
git commit -m "feat: add Skill model"
```

---

## Task 3: Create EquipmentCategory Model

**Files:**
- Create: `src/models/EquipmentCategory.h`
- Create: `src/models/EquipmentCategory.cpp`

**Step 1: Create the header file**

Write to `src/models/EquipmentCategory.h`:

```cpp
#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>

class EquipmentCategory
{
public:
    EquipmentCategory() = default;

    // Factory method for creating new categories
    static EquipmentCategory create(const QString& name, int sortOrder);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    int sortOrder() const { return m_sortOrder; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }

    // JSON serialization
    QJsonObject toJson() const;
    static EquipmentCategory fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const EquipmentCategory& other) const;
    bool operator!=(const EquipmentCategory& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    int m_sortOrder = 0;
};
```

**Step 2: Create the implementation file**

Write to `src/models/EquipmentCategory.cpp`:

```cpp
#include "EquipmentCategory.h"

EquipmentCategory EquipmentCategory::create(const QString& name, int sortOrder)
{
    EquipmentCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    return category;
}

QJsonObject EquipmentCategory::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["sortOrder"] = m_sortOrder;
    return json;
}

EquipmentCategory EquipmentCategory::fromJson(const QJsonObject& json)
{
    EquipmentCategory category;
    category.m_id = json["id"].toString();
    category.m_name = json["name"].toString();
    category.m_sortOrder = json["sortOrder"].toInt(0);
    return category;
}

bool EquipmentCategory::operator==(const EquipmentCategory& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_sortOrder == other.m_sortOrder;
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after Skill.cpp):
```cmake
    src/models/EquipmentCategory.cpp
```

Add to HEADERS (after Skill.h):
```cmake
    src/models/EquipmentCategory.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/models/EquipmentCategory.h src/models/EquipmentCategory.cpp CMakeLists.txt
git commit -m "feat: add EquipmentCategory model"
```

---

## Task 4: Create Equipment Model

**Files:**
- Create: `src/models/Equipment.h`
- Create: `src/models/Equipment.cpp`

**Step 1: Create the header file**

Write to `src/models/Equipment.h`:

```cpp
#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class Equipment
{
public:
    Equipment() = default;

    // Factory method for creating new equipment
    static Equipment create(const QString& name, const QString& categoryId);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& categoryId() const { return m_categoryId; }
    const QSet<QString>& familyIds() const { return m_familyIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setCategoryId(const QString& categoryId) { m_categoryId = categoryId; }
    void setFamilyIds(const QSet<QString>& familyIds) { m_familyIds = familyIds; }
    void addFamily(const QString& familyId) { m_familyIds.insert(familyId); }
    void removeFamily(const QString& familyId) { m_familyIds.remove(familyId); }

    // Computed properties
    bool hasFamily(const QString& familyId) const { return m_familyIds.contains(familyId); }
    bool isEmpty() const { return m_familyIds.isEmpty(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Equipment fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Equipment& other) const;
    bool operator!=(const Equipment& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_categoryId;
    QSet<QString> m_familyIds;
};
```

**Step 2: Create the implementation file**

Write to `src/models/Equipment.cpp`:

```cpp
#include "Equipment.h"

Equipment Equipment::create(const QString& name, const QString& categoryId)
{
    Equipment equipment;
    equipment.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    equipment.m_name = name;
    equipment.m_categoryId = categoryId;
    return equipment;
}

QJsonObject Equipment::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["categoryId"] = m_categoryId;

    QJsonArray familyArray;
    for (const QString& familyId : m_familyIds)
    {
        familyArray.append(familyId);
    }
    json["familyIds"] = familyArray;

    return json;
}

Equipment Equipment::fromJson(const QJsonObject& json)
{
    Equipment equipment;
    equipment.m_id = json["id"].toString();
    equipment.m_name = json["name"].toString();
    equipment.m_categoryId = json["categoryId"].toString();

    const QJsonArray familyArray = json["familyIds"].toArray();
    for (const QJsonValue& value : familyArray)
    {
        equipment.m_familyIds.insert(value.toString());
    }

    return equipment;
}

bool Equipment::operator==(const Equipment& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_categoryId == other.m_categoryId
        && m_familyIds == other.m_familyIds;
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after EquipmentCategory.cpp):
```cmake
    src/models/Equipment.cpp
```

Add to HEADERS (after EquipmentCategory.h):
```cmake
    src/models/Equipment.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/models/Equipment.h src/models/Equipment.cpp CMakeLists.txt
git commit -m "feat: add Equipment model"
```

---

## Task 5: Create SpecialNeed Model

**Files:**
- Create: `src/models/SpecialNeed.h`
- Create: `src/models/SpecialNeed.cpp`

**Step 1: Create the header file**

Write to `src/models/SpecialNeed.h`:

```cpp
#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>

struct SpecialNeed
{
    std::optional<QString> personId;
    std::optional<QString> familyId;
    QString note;

    // JSON serialization
    QJsonObject toJson() const;
    static SpecialNeed fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const SpecialNeed& other) const;
    bool operator!=(const SpecialNeed& other) const { return !(*this == other); }

    // Identity check (same person or family)
    bool matchesEntity(const std::optional<QString>& pId, const std::optional<QString>& fId) const;
};
```

**Step 2: Create the implementation file**

Write to `src/models/SpecialNeed.cpp`:

```cpp
#include "SpecialNeed.h"

QJsonObject SpecialNeed::toJson() const
{
    QJsonObject json;
    if (personId.has_value())
    {
        json["personId"] = personId.value();
    }
    if (familyId.has_value())
    {
        json["familyId"] = familyId.value();
    }
    json["note"] = note;
    return json;
}

SpecialNeed SpecialNeed::fromJson(const QJsonObject& json)
{
    SpecialNeed need;
    if (json.contains("personId") && !json["personId"].isNull())
    {
        need.personId = json["personId"].toString();
    }
    if (json.contains("familyId") && !json["familyId"].isNull())
    {
        need.familyId = json["familyId"].toString();
    }
    need.note = json["note"].toString();
    return need;
}

bool SpecialNeed::operator==(const SpecialNeed& other) const
{
    return personId == other.personId
        && familyId == other.familyId
        && note == other.note;
}

bool SpecialNeed::matchesEntity(const std::optional<QString>& pId, const std::optional<QString>& fId) const
{
    return personId == pId && familyId == fId;
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after Equipment.cpp):
```cmake
    src/models/SpecialNeed.cpp
```

Add to HEADERS (after Equipment.h):
```cmake
    src/models/SpecialNeed.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/models/SpecialNeed.h src/models/SpecialNeed.cpp CMakeLists.txt
git commit -m "feat: add SpecialNeed model"
```

---

## Task 6: Update DocumentChange Scopes

**Files:**
- Modify: `src/models/DocumentChange.h`
- Modify: `src/models/DocumentChange.cpp`

**Step 1: Update the header file**

In `src/models/DocumentChange.h`, replace the ChangeScope enum:

```cpp
enum class ChangeScope
{
    Full,             // Entire document (open, new, bulk import)
    Family,
    Team,
    Tag,
    SkillCategory,
    Skill,
    EquipmentCategory,
    Equipment,
    SpecialNeed,
    EqDistrict,
    EqGroup,
    RsDistrict,
    RsGroup,
    Metadata          // Wards/Stakes
};
```

In the same file, update the DocumentChange struct to replace:
- `static ScopeBuilder resourceCategory();`
- `static ScopeBuilder resourceType();`

With:
- `static ScopeBuilder skillCategory();`
- `static ScopeBuilder skill();`
- `static ScopeBuilder equipmentCategory();`
- `static ScopeBuilder equipment();`
- `static ScopeBuilder specialNeed();`

**Step 2: Update the implementation file**

In `src/models/DocumentChange.cpp`, replace `resourceCategory()` and `resourceType()` with:

```cpp
ScopeBuilder DocumentChange::skillCategory()
{
    return ScopeBuilder{ChangeScope::SkillCategory};
}

ScopeBuilder DocumentChange::skill()
{
    return ScopeBuilder{ChangeScope::Skill};
}

ScopeBuilder DocumentChange::equipmentCategory()
{
    return ScopeBuilder{ChangeScope::EquipmentCategory};
}

ScopeBuilder DocumentChange::equipment()
{
    return ScopeBuilder{ChangeScope::Equipment};
}

ScopeBuilder DocumentChange::specialNeed()
{
    return ScopeBuilder{ChangeScope::SpecialNeed};
}
```

**Step 3: Build to verify**

Run: `build.bat`
Expected: Build fails (expected - ResourceCommands still uses old scopes)

**Step 4: Commit**

```bash
git add src/models/DocumentChange.h src/models/DocumentChange.cpp
git commit -m "feat: update DocumentChange scopes for emergency inventory"
```

---

## Task 7: Create SkillCommands

**Files:**
- Create: `src/commands/SkillCommands.h`
- Create: `src/commands/SkillCommands.cpp`

**Step 1: Create the header file**

Write to `src/commands/SkillCommands.h`:

```cpp
#pragma once

#include "Command.h"
#include "SkillCategory.h"
#include "Skill.h"

// ============================================================================
// Category Commands
// ============================================================================

class AddSkillCategoryCommand : public Command
{
public:
    explicit AddSkillCategoryCommand(const SkillCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_category;
};

class UpdateSkillCategoryCommand : public Command
{
public:
    UpdateSkillCategoryCommand(const SkillCategory& oldCategory, const SkillCategory& newCategory);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_oldCategory;
    SkillCategory m_newCategory;
};

class DeleteSkillCategoryCommand : public Command
{
public:
    explicit DeleteSkillCategoryCommand(const SkillCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_category;
};

// ============================================================================
// Skill Commands
// ============================================================================

class AddSkillCommand : public Command
{
public:
    explicit AddSkillCommand(const Skill& skill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_skill;
};

class UpdateSkillCommand : public Command
{
public:
    UpdateSkillCommand(const Skill& oldSkill, const Skill& newSkill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_oldSkill;
    Skill m_newSkill;
};

class DeleteSkillCommand : public Command
{
public:
    explicit DeleteSkillCommand(const Skill& skill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_skill;
};

// ============================================================================
// Assignment Commands
// ============================================================================

class AssignSkillToPersonCommand : public Command
{
public:
    AssignSkillToPersonCommand(const QString& skillId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_skillId;
    QString m_personId;
};

class UnassignSkillFromPersonCommand : public Command
{
public:
    UnassignSkillFromPersonCommand(const QString& skillId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_skillId;
    QString m_personId;
};
```

**Step 2: Create the implementation file**

Write to `src/commands/SkillCommands.cpp`:

```cpp
#include "SkillCommands.h"
#include "Document.h"

// ============================================================================
// AddSkillCategoryCommand
// ============================================================================

AddSkillCategoryCommand::AddSkillCategoryCommand(const SkillCategory& category)
    : m_category(category)
{
}

void AddSkillCategoryCommand::execute(Document& document)
{
    document.addSkillCategory(m_category);
}

void AddSkillCategoryCommand::undo(Document& document)
{
    document.removeSkillCategory(m_category.id());
}

QString AddSkillCategoryCommand::description() const
{
    return QObject::tr("Add Skill Category \"%1\"").arg(m_category.name());
}

DocumentChange AddSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().added(m_category.id());
}

// ============================================================================
// UpdateSkillCategoryCommand
// ============================================================================

UpdateSkillCategoryCommand::UpdateSkillCategoryCommand(const SkillCategory& oldCategory,
                                                        const SkillCategory& newCategory)
    : m_oldCategory(oldCategory)
    , m_newCategory(newCategory)
{
}

void UpdateSkillCategoryCommand::execute(Document& document)
{
    document.updateSkillCategory(m_newCategory);
}

void UpdateSkillCategoryCommand::undo(Document& document)
{
    document.updateSkillCategory(m_oldCategory);
}

QString UpdateSkillCategoryCommand::description() const
{
    return QObject::tr("Update Skill Category \"%1\"").arg(m_oldCategory.name());
}

DocumentChange UpdateSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().updated(m_newCategory.id());
}

// ============================================================================
// DeleteSkillCategoryCommand
// ============================================================================

DeleteSkillCategoryCommand::DeleteSkillCategoryCommand(const SkillCategory& category)
    : m_category(category)
{
}

void DeleteSkillCategoryCommand::execute(Document& document)
{
    document.removeSkillCategory(m_category.id());
}

void DeleteSkillCategoryCommand::undo(Document& document)
{
    document.addSkillCategory(m_category);
}

QString DeleteSkillCategoryCommand::description() const
{
    return QObject::tr("Delete Skill Category \"%1\"").arg(m_category.name());
}

DocumentChange DeleteSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().removed(m_category.id());
}

// ============================================================================
// AddSkillCommand
// ============================================================================

AddSkillCommand::AddSkillCommand(const Skill& skill)
    : m_skill(skill)
{
}

void AddSkillCommand::execute(Document& document)
{
    document.addSkill(m_skill);
}

void AddSkillCommand::undo(Document& document)
{
    document.removeSkill(m_skill.id());
}

QString AddSkillCommand::description() const
{
    return QObject::tr("Add Skill \"%1\"").arg(m_skill.name());
}

DocumentChange AddSkillCommand::documentChange() const
{
    return DocumentChange::skill().added(m_skill.id());
}

// ============================================================================
// UpdateSkillCommand
// ============================================================================

UpdateSkillCommand::UpdateSkillCommand(const Skill& oldSkill, const Skill& newSkill)
    : m_oldSkill(oldSkill)
    , m_newSkill(newSkill)
{
}

void UpdateSkillCommand::execute(Document& document)
{
    document.updateSkill(m_newSkill);
}

void UpdateSkillCommand::undo(Document& document)
{
    document.updateSkill(m_oldSkill);
}

QString UpdateSkillCommand::description() const
{
    return QObject::tr("Update Skill \"%1\"").arg(m_oldSkill.name());
}

DocumentChange UpdateSkillCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_newSkill.id());
}

// ============================================================================
// DeleteSkillCommand
// ============================================================================

DeleteSkillCommand::DeleteSkillCommand(const Skill& skill)
    : m_skill(skill)
{
}

void DeleteSkillCommand::execute(Document& document)
{
    document.removeSkill(m_skill.id());
}

void DeleteSkillCommand::undo(Document& document)
{
    document.addSkill(m_skill);
}

QString DeleteSkillCommand::description() const
{
    return QObject::tr("Delete Skill \"%1\"").arg(m_skill.name());
}

DocumentChange DeleteSkillCommand::documentChange() const
{
    return DocumentChange::skill().removed(m_skill.id());
}

// ============================================================================
// AssignSkillToPersonCommand
// ============================================================================

AssignSkillToPersonCommand::AssignSkillToPersonCommand(const QString& skillId,
                                                        const QString& personId)
    : m_skillId(skillId)
    , m_personId(personId)
{
}

void AssignSkillToPersonCommand::execute(Document& document)
{
    document.addPersonToSkill(m_skillId, m_personId);
}

void AssignSkillToPersonCommand::undo(Document& document)
{
    document.removePersonFromSkill(m_skillId, m_personId);
}

QString AssignSkillToPersonCommand::description() const
{
    return QObject::tr("Assign Skill to Person");
}

DocumentChange AssignSkillToPersonCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_skillId);
}

// ============================================================================
// UnassignSkillFromPersonCommand
// ============================================================================

UnassignSkillFromPersonCommand::UnassignSkillFromPersonCommand(const QString& skillId,
                                                                const QString& personId)
    : m_skillId(skillId)
    , m_personId(personId)
{
}

void UnassignSkillFromPersonCommand::execute(Document& document)
{
    document.removePersonFromSkill(m_skillId, m_personId);
}

void UnassignSkillFromPersonCommand::undo(Document& document)
{
    document.addPersonToSkill(m_skillId, m_personId);
}

QString UnassignSkillFromPersonCommand::description() const
{
    return QObject::tr("Remove Skill from Person");
}

DocumentChange UnassignSkillFromPersonCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_skillId);
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after ResourceCommands.cpp, around line 138):
```cmake
    src/commands/SkillCommands.cpp
```

Add to HEADERS (after ResourceCommands.h, around line 152):
```cmake
    src/commands/SkillCommands.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build fails (Document doesn't have skill methods yet)

**Step 5: Commit**

```bash
git add src/commands/SkillCommands.h src/commands/SkillCommands.cpp CMakeLists.txt
git commit -m "feat: add SkillCommands"
```

---

## Task 8: Create EquipmentCommands

**Files:**
- Create: `src/commands/EquipmentCommands.h`
- Create: `src/commands/EquipmentCommands.cpp`

**Step 1: Create the header file**

Write to `src/commands/EquipmentCommands.h`:

```cpp
#pragma once

#include "Command.h"
#include "EquipmentCategory.h"
#include "Equipment.h"

// ============================================================================
// Category Commands
// ============================================================================

class AddEquipmentCategoryCommand : public Command
{
public:
    explicit AddEquipmentCategoryCommand(const EquipmentCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_category;
};

class UpdateEquipmentCategoryCommand : public Command
{
public:
    UpdateEquipmentCategoryCommand(const EquipmentCategory& oldCategory, const EquipmentCategory& newCategory);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_oldCategory;
    EquipmentCategory m_newCategory;
};

class DeleteEquipmentCategoryCommand : public Command
{
public:
    explicit DeleteEquipmentCategoryCommand(const EquipmentCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_category;
};

// ============================================================================
// Equipment Commands
// ============================================================================

class AddEquipmentCommand : public Command
{
public:
    explicit AddEquipmentCommand(const Equipment& equipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_equipment;
};

class UpdateEquipmentCommand : public Command
{
public:
    UpdateEquipmentCommand(const Equipment& oldEquipment, const Equipment& newEquipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_oldEquipment;
    Equipment m_newEquipment;
};

class DeleteEquipmentCommand : public Command
{
public:
    explicit DeleteEquipmentCommand(const Equipment& equipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_equipment;
};

// ============================================================================
// Assignment Commands
// ============================================================================

class AssignEquipmentToFamilyCommand : public Command
{
public:
    AssignEquipmentToFamilyCommand(const QString& equipmentId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_equipmentId;
    QString m_familyId;
};

class UnassignEquipmentFromFamilyCommand : public Command
{
public:
    UnassignEquipmentFromFamilyCommand(const QString& equipmentId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_equipmentId;
    QString m_familyId;
};
```

**Step 2: Create the implementation file**

Write to `src/commands/EquipmentCommands.cpp`:

```cpp
#include "EquipmentCommands.h"
#include "Document.h"

// ============================================================================
// AddEquipmentCategoryCommand
// ============================================================================

AddEquipmentCategoryCommand::AddEquipmentCategoryCommand(const EquipmentCategory& category)
    : m_category(category)
{
}

void AddEquipmentCategoryCommand::execute(Document& document)
{
    document.addEquipmentCategory(m_category);
}

void AddEquipmentCategoryCommand::undo(Document& document)
{
    document.removeEquipmentCategory(m_category.id());
}

QString AddEquipmentCategoryCommand::description() const
{
    return QObject::tr("Add Equipment Category \"%1\"").arg(m_category.name());
}

DocumentChange AddEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().added(m_category.id());
}

// ============================================================================
// UpdateEquipmentCategoryCommand
// ============================================================================

UpdateEquipmentCategoryCommand::UpdateEquipmentCategoryCommand(const EquipmentCategory& oldCategory,
                                                                const EquipmentCategory& newCategory)
    : m_oldCategory(oldCategory)
    , m_newCategory(newCategory)
{
}

void UpdateEquipmentCategoryCommand::execute(Document& document)
{
    document.updateEquipmentCategory(m_newCategory);
}

void UpdateEquipmentCategoryCommand::undo(Document& document)
{
    document.updateEquipmentCategory(m_oldCategory);
}

QString UpdateEquipmentCategoryCommand::description() const
{
    return QObject::tr("Update Equipment Category \"%1\"").arg(m_oldCategory.name());
}

DocumentChange UpdateEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().updated(m_newCategory.id());
}

// ============================================================================
// DeleteEquipmentCategoryCommand
// ============================================================================

DeleteEquipmentCategoryCommand::DeleteEquipmentCategoryCommand(const EquipmentCategory& category)
    : m_category(category)
{
}

void DeleteEquipmentCategoryCommand::execute(Document& document)
{
    document.removeEquipmentCategory(m_category.id());
}

void DeleteEquipmentCategoryCommand::undo(Document& document)
{
    document.addEquipmentCategory(m_category);
}

QString DeleteEquipmentCategoryCommand::description() const
{
    return QObject::tr("Delete Equipment Category \"%1\"").arg(m_category.name());
}

DocumentChange DeleteEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().removed(m_category.id());
}

// ============================================================================
// AddEquipmentCommand
// ============================================================================

AddEquipmentCommand::AddEquipmentCommand(const Equipment& equipment)
    : m_equipment(equipment)
{
}

void AddEquipmentCommand::execute(Document& document)
{
    document.addEquipment(m_equipment);
}

void AddEquipmentCommand::undo(Document& document)
{
    document.removeEquipment(m_equipment.id());
}

QString AddEquipmentCommand::description() const
{
    return QObject::tr("Add Equipment \"%1\"").arg(m_equipment.name());
}

DocumentChange AddEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().added(m_equipment.id());
}

// ============================================================================
// UpdateEquipmentCommand
// ============================================================================

UpdateEquipmentCommand::UpdateEquipmentCommand(const Equipment& oldEquipment,
                                                const Equipment& newEquipment)
    : m_oldEquipment(oldEquipment)
    , m_newEquipment(newEquipment)
{
}

void UpdateEquipmentCommand::execute(Document& document)
{
    document.updateEquipment(m_newEquipment);
}

void UpdateEquipmentCommand::undo(Document& document)
{
    document.updateEquipment(m_oldEquipment);
}

QString UpdateEquipmentCommand::description() const
{
    return QObject::tr("Update Equipment \"%1\"").arg(m_oldEquipment.name());
}

DocumentChange UpdateEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_newEquipment.id());
}

// ============================================================================
// DeleteEquipmentCommand
// ============================================================================

DeleteEquipmentCommand::DeleteEquipmentCommand(const Equipment& equipment)
    : m_equipment(equipment)
{
}

void DeleteEquipmentCommand::execute(Document& document)
{
    document.removeEquipment(m_equipment.id());
}

void DeleteEquipmentCommand::undo(Document& document)
{
    document.addEquipment(m_equipment);
}

QString DeleteEquipmentCommand::description() const
{
    return QObject::tr("Delete Equipment \"%1\"").arg(m_equipment.name());
}

DocumentChange DeleteEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().removed(m_equipment.id());
}

// ============================================================================
// AssignEquipmentToFamilyCommand
// ============================================================================

AssignEquipmentToFamilyCommand::AssignEquipmentToFamilyCommand(const QString& equipmentId,
                                                                const QString& familyId)
    : m_equipmentId(equipmentId)
    , m_familyId(familyId)
{
}

void AssignEquipmentToFamilyCommand::execute(Document& document)
{
    document.addFamilyToEquipment(m_equipmentId, m_familyId);
}

void AssignEquipmentToFamilyCommand::undo(Document& document)
{
    document.removeFamilyFromEquipment(m_equipmentId, m_familyId);
}

QString AssignEquipmentToFamilyCommand::description() const
{
    return QObject::tr("Assign Equipment to Family");
}

DocumentChange AssignEquipmentToFamilyCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_equipmentId);
}

// ============================================================================
// UnassignEquipmentFromFamilyCommand
// ============================================================================

UnassignEquipmentFromFamilyCommand::UnassignEquipmentFromFamilyCommand(const QString& equipmentId,
                                                                        const QString& familyId)
    : m_equipmentId(equipmentId)
    , m_familyId(familyId)
{
}

void UnassignEquipmentFromFamilyCommand::execute(Document& document)
{
    document.removeFamilyFromEquipment(m_equipmentId, m_familyId);
}

void UnassignEquipmentFromFamilyCommand::undo(Document& document)
{
    document.addFamilyToEquipment(m_equipmentId, m_familyId);
}

QString UnassignEquipmentFromFamilyCommand::description() const
{
    return QObject::tr("Remove Equipment from Family");
}

DocumentChange UnassignEquipmentFromFamilyCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_equipmentId);
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after SkillCommands.cpp):
```cmake
    src/commands/EquipmentCommands.cpp
```

Add to HEADERS (after SkillCommands.h):
```cmake
    src/commands/EquipmentCommands.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build fails (Document doesn't have equipment methods yet)

**Step 5: Commit**

```bash
git add src/commands/EquipmentCommands.h src/commands/EquipmentCommands.cpp CMakeLists.txt
git commit -m "feat: add EquipmentCommands"
```

---

## Task 9: Create SpecialNeedCommands

**Files:**
- Create: `src/commands/SpecialNeedCommands.h`
- Create: `src/commands/SpecialNeedCommands.cpp`

**Step 1: Create the header file**

Write to `src/commands/SpecialNeedCommands.h`:

```cpp
#pragma once

#include "Command.h"
#include "SpecialNeed.h"
#include <optional>

class SetSpecialNeedCommand : public Command
{
public:
    // For person-level special need
    static SetSpecialNeedCommand forPerson(const QString& personId, const QString& note);
    // For family-level special need
    static SetSpecialNeedCommand forFamily(const QString& familyId, const QString& note);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SetSpecialNeedCommand(std::optional<QString> personId, std::optional<QString> familyId, const QString& note);

    std::optional<QString> m_personId;
    std::optional<QString> m_familyId;
    QString m_note;
    std::optional<SpecialNeed> m_previousNeed;  // For undo
};

class ClearSpecialNeedCommand : public Command
{
public:
    // For person-level special need
    static ClearSpecialNeedCommand forPerson(const QString& personId);
    // For family-level special need
    static ClearSpecialNeedCommand forFamily(const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ClearSpecialNeedCommand(std::optional<QString> personId, std::optional<QString> familyId);

    std::optional<QString> m_personId;
    std::optional<QString> m_familyId;
    std::optional<SpecialNeed> m_removedNeed;  // For undo
};
```

**Step 2: Create the implementation file**

Write to `src/commands/SpecialNeedCommands.cpp`:

```cpp
#include "SpecialNeedCommands.h"
#include "Document.h"

// ============================================================================
// SetSpecialNeedCommand
// ============================================================================

SetSpecialNeedCommand SetSpecialNeedCommand::forPerson(const QString& personId, const QString& note)
{
    return SetSpecialNeedCommand(personId, std::nullopt, note);
}

SetSpecialNeedCommand SetSpecialNeedCommand::forFamily(const QString& familyId, const QString& note)
{
    return SetSpecialNeedCommand(std::nullopt, familyId, note);
}

SetSpecialNeedCommand::SetSpecialNeedCommand(std::optional<QString> personId,
                                              std::optional<QString> familyId,
                                              const QString& note)
    : m_personId(personId)
    , m_familyId(familyId)
    , m_note(note)
{
}

void SetSpecialNeedCommand::execute(Document& document)
{
    // Store previous state for undo
    m_previousNeed = document.findSpecialNeed(m_personId, m_familyId);

    SpecialNeed need;
    need.personId = m_personId;
    need.familyId = m_familyId;
    need.note = m_note;
    document.setSpecialNeed(need);
}

void SetSpecialNeedCommand::undo(Document& document)
{
    if (m_previousNeed.has_value())
    {
        document.setSpecialNeed(m_previousNeed.value());
    }
    else
    {
        document.clearSpecialNeed(m_personId, m_familyId);
    }
}

QString SetSpecialNeedCommand::description() const
{
    return QObject::tr("Set Special Need");
}

DocumentChange SetSpecialNeedCommand::documentChange() const
{
    QString entityId = m_personId.value_or(m_familyId.value_or(QString()));
    return DocumentChange::specialNeed().updated(entityId);
}

// ============================================================================
// ClearSpecialNeedCommand
// ============================================================================

ClearSpecialNeedCommand ClearSpecialNeedCommand::forPerson(const QString& personId)
{
    return ClearSpecialNeedCommand(personId, std::nullopt);
}

ClearSpecialNeedCommand ClearSpecialNeedCommand::forFamily(const QString& familyId)
{
    return ClearSpecialNeedCommand(std::nullopt, familyId);
}

ClearSpecialNeedCommand::ClearSpecialNeedCommand(std::optional<QString> personId,
                                                  std::optional<QString> familyId)
    : m_personId(personId)
    , m_familyId(familyId)
{
}

void ClearSpecialNeedCommand::execute(Document& document)
{
    // Store for undo
    m_removedNeed = document.findSpecialNeed(m_personId, m_familyId);
    document.clearSpecialNeed(m_personId, m_familyId);
}

void ClearSpecialNeedCommand::undo(Document& document)
{
    if (m_removedNeed.has_value())
    {
        document.setSpecialNeed(m_removedNeed.value());
    }
}

QString ClearSpecialNeedCommand::description() const
{
    return QObject::tr("Clear Special Need");
}

DocumentChange ClearSpecialNeedCommand::documentChange() const
{
    QString entityId = m_personId.value_or(m_familyId.value_or(QString()));
    return DocumentChange::specialNeed().removed(entityId);
}
```

**Step 3: Update CMakeLists.txt**

Add to SOURCES (after EquipmentCommands.cpp):
```cmake
    src/commands/SpecialNeedCommands.cpp
```

Add to HEADERS (after EquipmentCommands.h):
```cmake
    src/commands/SpecialNeedCommands.h
```

**Step 4: Build to verify**

Run: `build.bat`
Expected: Build fails (Document doesn't have special need methods yet)

**Step 5: Commit**

```bash
git add src/commands/SpecialNeedCommands.h src/commands/SpecialNeedCommands.cpp CMakeLists.txt
git commit -m "feat: add SpecialNeedCommands"
```

---

## Task 10: Update Document Class - Add New Collections and Methods

**Files:**
- Modify: `src/models/Document.h`
- Modify: `src/models/Document.cpp`

**Step 1: Update Document.h**

Add includes at the top (after existing model includes):
```cpp
#include "SkillCategory.h"
#include "Skill.h"
#include "EquipmentCategory.h"
#include "Equipment.h"
#include "SpecialNeed.h"
```

Replace the "Resource collection getters" section with:
```cpp
    // ========================================================================
    // Emergency inventory getters
    // ========================================================================
    const QHash<QString, SkillCategory>& skillCategories() const { return m_skillCategories; }
    const QHash<QString, Skill>& skills() const { return m_skills; }
    const QHash<QString, EquipmentCategory>& equipmentCategories() const { return m_equipmentCategories; }
    const QHash<QString, Equipment>& equipment() const { return m_equipment; }
    const QList<SpecialNeed>& specialNeeds() const { return m_specialNeeds; }
```

Replace the "Mutating operations - resource collections" section with:
```cpp
    // ========================================================================
    // Mutating operations - skill categories
    // ========================================================================
    void addSkillCategory(const SkillCategory& category);
    void updateSkillCategory(const SkillCategory& category);
    void removeSkillCategory(const QString& id);

    // ========================================================================
    // Mutating operations - skills
    // ========================================================================
    void addSkill(const Skill& skill);
    void updateSkill(const Skill& skill);
    void removeSkill(const QString& id);
    void addPersonToSkill(const QString& skillId, const QString& personId);
    void removePersonFromSkill(const QString& skillId, const QString& personId);

    // ========================================================================
    // Mutating operations - equipment categories
    // ========================================================================
    void addEquipmentCategory(const EquipmentCategory& category);
    void updateEquipmentCategory(const EquipmentCategory& category);
    void removeEquipmentCategory(const QString& id);

    // ========================================================================
    // Mutating operations - equipment
    // ========================================================================
    void addEquipment(const Equipment& equipment);
    void updateEquipment(const Equipment& equipment);
    void removeEquipment(const QString& id);
    void addFamilyToEquipment(const QString& equipmentId, const QString& familyId);
    void removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId);

    // ========================================================================
    // Mutating operations - special needs
    // ========================================================================
    void setSpecialNeed(const SpecialNeed& need);
    void clearSpecialNeed(std::optional<QString> personId, std::optional<QString> familyId);
    std::optional<SpecialNeed> findSpecialNeed(std::optional<QString> personId, std::optional<QString> familyId) const;
```

Replace the member variables for resources with:
```cpp
    // Emergency inventory
    QHash<QString, SkillCategory> m_skillCategories;
    QHash<QString, Skill> m_skills;
    QHash<QString, EquipmentCategory> m_equipmentCategories;
    QHash<QString, Equipment> m_equipment;
    QList<SpecialNeed> m_specialNeeds;
```

Remove the old finder method declaration:
```cpp
    std::optional<ResourceType> findResourceTypeById(const QString& id) const;
```

Add new finder methods:
```cpp
    std::optional<Skill> findSkillById(const QString& id) const;
    std::optional<Equipment> findEquipmentById(const QString& id) const;
```

**Step 2: Update Document.cpp**

Remove old includes:
```cpp
#include "ResourceCategory.h"
#include "ResourceType.h"
```

Add new includes:
```cpp
#include "SkillCategory.h"
#include "Skill.h"
#include "EquipmentCategory.h"
#include "Equipment.h"
#include "SpecialNeed.h"
```

Remove all the old resource methods (addCategory, updateCategory, removeCategory, addResourceType, updateResourceType, removeResourceType, addPersonToResourceType, etc.).

Add the new skill category methods:
```cpp
void Document::addSkillCategory(const SkillCategory& category)
{
    m_skillCategories.insert(category.id(), category);
}

void Document::updateSkillCategory(const SkillCategory& category)
{
    m_skillCategories.insert(category.id(), category);
}

void Document::removeSkillCategory(const QString& id)
{
    m_skillCategories.remove(id);
}
```

Add the new skill methods:
```cpp
void Document::addSkill(const Skill& skill)
{
    m_skills.insert(skill.id(), skill);
}

void Document::updateSkill(const Skill& skill)
{
    m_skills.insert(skill.id(), skill);
}

void Document::removeSkill(const QString& id)
{
    m_skills.remove(id);
}

void Document::addPersonToSkill(const QString& skillId, const QString& personId)
{
    auto it = m_skills.find(skillId);
    if (it != m_skills.end())
    {
        it->addPerson(personId);
    }
}

void Document::removePersonFromSkill(const QString& skillId, const QString& personId)
{
    auto it = m_skills.find(skillId);
    if (it != m_skills.end())
    {
        it->removePerson(personId);
    }
}

std::optional<Skill> Document::findSkillById(const QString& id) const
{
    auto it = m_skills.find(id);
    if (it != m_skills.end())
    {
        return *it;
    }
    return std::nullopt;
}
```

Add the new equipment category methods:
```cpp
void Document::addEquipmentCategory(const EquipmentCategory& category)
{
    m_equipmentCategories.insert(category.id(), category);
}

void Document::updateEquipmentCategory(const EquipmentCategory& category)
{
    m_equipmentCategories.insert(category.id(), category);
}

void Document::removeEquipmentCategory(const QString& id)
{
    m_equipmentCategories.remove(id);
}
```

Add the new equipment methods:
```cpp
void Document::addEquipment(const Equipment& equipment)
{
    m_equipment.insert(equipment.id(), equipment);
}

void Document::updateEquipment(const Equipment& equipment)
{
    m_equipment.insert(equipment.id(), equipment);
}

void Document::removeEquipment(const QString& id)
{
    m_equipment.remove(id);
}

void Document::addFamilyToEquipment(const QString& equipmentId, const QString& familyId)
{
    auto it = m_equipment.find(equipmentId);
    if (it != m_equipment.end())
    {
        it->addFamily(familyId);
    }
}

void Document::removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId)
{
    auto it = m_equipment.find(equipmentId);
    if (it != m_equipment.end())
    {
        it->removeFamily(familyId);
    }
}

std::optional<Equipment> Document::findEquipmentById(const QString& id) const
{
    auto it = m_equipment.find(id);
    if (it != m_equipment.end())
    {
        return *it;
    }
    return std::nullopt;
}
```

Add the new special need methods:
```cpp
void Document::setSpecialNeed(const SpecialNeed& need)
{
    // Remove existing if present
    clearSpecialNeed(need.personId, need.familyId);
    m_specialNeeds.append(need);
}

void Document::clearSpecialNeed(std::optional<QString> personId, std::optional<QString> familyId)
{
    m_specialNeeds.removeIf([&](const SpecialNeed& need) {
        return need.matchesEntity(personId, familyId);
    });
}

std::optional<SpecialNeed> Document::findSpecialNeed(std::optional<QString> personId,
                                                      std::optional<QString> familyId) const
{
    for (const SpecialNeed& need : m_specialNeeds)
    {
        if (need.matchesEntity(personId, familyId))
        {
            return need;
        }
    }
    return std::nullopt;
}
```

Update `removePersonReferences()` to use skills instead of resourceTypes:
```cpp
    // Remove from skills
    for (auto it = m_skills.begin(); it != m_skills.end(); ++it)
    {
        if (it->personIds().contains(personId))
        {
            it->removePerson(personId);
        }
    }

    // Remove special needs for this person
    clearSpecialNeed(personId, std::nullopt);
```

Update `removeFamilyReferences()` to use equipment instead of resourceTypes:
```cpp
    // Remove from equipment
    for (auto it = m_equipment.begin(); it != m_equipment.end(); ++it)
    {
        if (it->familyIds().contains(familyId))
        {
            it->removeFamily(familyId);
        }
    }

    // Remove special needs for this family
    clearSpecialNeed(std::nullopt, familyId);
```

Update `toJson()` - replace resource serialization with:
```cpp
    // Emergency inventory
    serializeHashToJson(json, "skillCategories", m_skillCategories);
    serializeHashToJson(json, "skills", m_skills);
    serializeHashToJson(json, "equipmentCategories", m_equipmentCategories);
    serializeHashToJson(json, "equipment", m_equipment);

    // Special needs (QList, not QHash)
    QJsonArray specialNeedsArray;
    for (const SpecialNeed& need : m_specialNeeds)
    {
        specialNeedsArray.append(need.toJson());
    }
    json["specialNeeds"] = specialNeedsArray;
```

Update `fromJson()` - replace resource deserialization with:
```cpp
    // Emergency inventory
    deserializeJsonToHash(json, "skillCategories", document.m_skillCategories);
    deserializeJsonToHash(json, "skills", document.m_skills);
    deserializeJsonToHash(json, "equipmentCategories", document.m_equipmentCategories);
    deserializeJsonToHash(json, "equipment", document.m_equipment);

    // Special needs
    const QJsonArray specialNeedsArray = json["specialNeeds"].toArray();
    for (const QJsonValue& value : specialNeedsArray)
    {
        document.m_specialNeeds.append(SpecialNeed::fromJson(value.toObject()));
    }
```

Update `operator==()` - replace resource comparison with:
```cpp
        && m_skillCategories == other.m_skillCategories
        && m_skills == other.m_skills
        && m_equipmentCategories == other.m_equipmentCategories
        && m_equipment == other.m_equipment
        && m_specialNeeds == other.m_specialNeeds
```

**Step 3: Build to verify**

Run: `build.bat`
Expected: Build fails (ResourceCommands still exists and uses old types)

**Step 4: Commit**

```bash
git add src/models/Document.h src/models/Document.cpp
git commit -m "feat: update Document with emergency inventory collections"
```

---

## Task 11: Remove Old Resource Files

**Files:**
- Delete: `src/models/ResourceCategory.h`
- Delete: `src/models/ResourceCategory.cpp`
- Delete: `src/models/ResourceType.h`
- Delete: `src/models/ResourceType.cpp`
- Delete: `src/models/ResourceLevel.h`
- Delete: `src/models/MarkerDecorationType.h`
- Delete: `src/commands/ResourceCommands.h`
- Delete: `src/commands/ResourceCommands.cpp`

**Step 1: Delete the old files**

```bash
rm src/models/ResourceCategory.h src/models/ResourceCategory.cpp
rm src/models/ResourceType.h src/models/ResourceType.cpp
rm src/models/ResourceLevel.h
rm src/models/MarkerDecorationType.h
rm src/commands/ResourceCommands.h src/commands/ResourceCommands.cpp
```

**Step 2: Update CMakeLists.txt**

Remove these lines from SOURCES:
```cmake
    src/models/ResourceCategory.cpp
    src/models/ResourceType.cpp
    src/commands/ResourceCommands.cpp
```

Remove these lines from HEADERS:
```cmake
    src/models/ResourceLevel.h
    src/models/ResourceCategory.h
    src/models/ResourceType.h
    src/models/MarkerDecorationType.h
    src/commands/ResourceCommands.h
```

**Step 3: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor: remove old resource models and commands"
```

---

## Task 12: Add Default Categories Helper

**Files:**
- Modify: `src/models/Document.h`
- Modify: `src/models/Document.cpp`

**Step 1: Add static method declaration to Document.h**

In the public section, add:
```cpp
    // Initialize default categories for a new document
    void initializeDefaultCategories();
```

**Step 2: Implement in Document.cpp**

```cpp
void Document::initializeDefaultCategories()
{
    // Default skill categories
    m_skillCategories.insert(
        SkillCategory::create(tr("Medical"), 0).id(),
        SkillCategory::create(tr("Medical"), 0));
    m_skillCategories.insert(
        SkillCategory::create(tr("Communication"), 1).id(),
        SkillCategory::create(tr("Communication"), 1));
    m_skillCategories.insert(
        SkillCategory::create(tr("Repair"), 2).id(),
        SkillCategory::create(tr("Repair"), 2));

    // Default equipment categories
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Power"), 0).id(),
        EquipmentCategory::create(tr("Power"), 0));
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Tools"), 1).id(),
        EquipmentCategory::create(tr("Tools"), 1));
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Transportation"), 2).id(),
        EquipmentCategory::create(tr("Transportation"), 2));
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Shelter"), 3).id(),
        EquipmentCategory::create(tr("Shelter"), 3));
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Water"), 4).id(),
        EquipmentCategory::create(tr("Water"), 4));
    m_equipmentCategories.insert(
        EquipmentCategory::create(tr("Supplies"), 5).id(),
        EquipmentCategory::create(tr("Supplies"), 5));
}
```

Note: The above creates duplicate IDs. Let me fix that:

```cpp
void Document::initializeDefaultCategories()
{
    // Default skill categories
    auto medical = SkillCategory::create(tr("Medical"), 0);
    m_skillCategories.insert(medical.id(), medical);

    auto communication = SkillCategory::create(tr("Communication"), 1);
    m_skillCategories.insert(communication.id(), communication);

    auto repair = SkillCategory::create(tr("Repair"), 2);
    m_skillCategories.insert(repair.id(), repair);

    // Default equipment categories
    auto power = EquipmentCategory::create(tr("Power"), 0);
    m_equipmentCategories.insert(power.id(), power);

    auto tools = EquipmentCategory::create(tr("Tools"), 1);
    m_equipmentCategories.insert(tools.id(), tools);

    auto transportation = EquipmentCategory::create(tr("Transportation"), 2);
    m_equipmentCategories.insert(transportation.id(), transportation);

    auto shelter = EquipmentCategory::create(tr("Shelter"), 3);
    m_equipmentCategories.insert(shelter.id(), shelter);

    auto water = EquipmentCategory::create(tr("Water"), 4);
    m_equipmentCategories.insert(water.id(), water);

    auto supplies = EquipmentCategory::create(tr("Supplies"), 5);
    m_equipmentCategories.insert(supplies.id(), supplies);
}
```

**Step 3: Build to verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/models/Document.h src/models/Document.cpp
git commit -m "feat: add initializeDefaultCategories helper"
```

---

## Task 13: Final Build and Test

**Step 1: Full rebuild**

```bash
rm -rf build
./build.bat
```

Expected: Build succeeds with no errors

**Step 2: Run the application**

```bash
./build/WardPlanning.exe
```

Expected: Application launches without crash

**Step 3: Verify JSON round-trip**

1. Create a new document
2. Save it
3. Close and reopen
4. Verify no errors on load

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete emergency inventory refactor"
```

---

## Summary

This plan replaces the unified ResourceCategory/ResourceType model with:

1. **SkillCategory** + **Skill** (person-level capabilities)
2. **EquipmentCategory** + **Equipment** (family-level items)
3. **SpecialNeed** (freeform notes for persons or families)

Files created:
- `src/models/SkillCategory.h/.cpp`
- `src/models/Skill.h/.cpp`
- `src/models/EquipmentCategory.h/.cpp`
- `src/models/Equipment.h/.cpp`
- `src/models/SpecialNeed.h/.cpp`
- `src/commands/SkillCommands.h/.cpp`
- `src/commands/EquipmentCommands.h/.cpp`
- `src/commands/SpecialNeedCommands.h/.cpp`

Files removed:
- `src/models/ResourceCategory.h/.cpp`
- `src/models/ResourceType.h/.cpp`
- `src/models/ResourceLevel.h`
- `src/models/MarkerDecorationType.h`
- `src/commands/ResourceCommands.h/.cpp`

Files modified:
- `src/models/Document.h/.cpp`
- `src/models/DocumentChange.h/.cpp`
- `CMakeLists.txt`
