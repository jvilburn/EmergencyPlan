# Emergency Preparedness Inventory - Design

## Overview

Refactor the resource tracking system to better model emergency preparedness inventory. The current unified `ResourceCategory`/`ResourceType` model is replaced with three distinct concepts that reflect how emergency planners think about capabilities.

## Problem Statement

The current model uses generic "resources" with a `ResourceLevel` enum to distinguish person vs family assignment. This creates confusion because:

1. **Skills** (person-level) and **Equipment** (family-level) are fundamentally different concepts
2. **Special Needs** (who needs help) is the inverse of skills/equipment (who can help)
3. The category "Recovery Resources" is vague - it's really equipment
4. The unified model forces awkward compromises in both data and UI

## Design

### Three Separate Concepts

#### 1. Skills (Person-Level)

Capabilities that individuals have. Tracked per-person.

**Categories (user-configurable with defaults):**
- **Medical** - EMT, Nurse, Doctor, CPR certified, First Aid
- **Communication** - HAM radio license, CERT trained
- **Repair** - Electrical, Plumbing, Carpentry, Mechanical, Welding

**Model:**
```cpp
class SkillCategory
{
    QString m_id;
    QString m_name;
    int m_sortOrder;
};

class Skill
{
    QString m_id;
    QString m_name;                  // User-configurable (e.g., "EMT", "HAM License")
    QString m_categoryId;            // References SkillCategory
    QSet<QString> m_personIds;       // Who has this skill
};
```

**Storage:**
- `Document::m_skillCategories` as `QHash<QString, SkillCategory>`
- `Document::m_skills` as `QHash<QString, Skill>`

#### 2. Equipment (Family-Level)

Physical items that households own. Tracked per-family.

**Categories (user-configurable with defaults):**
- **Power** - Generator, solar panels, battery bank
- **Tools** - Chainsaw, power tools, ladder
- **Transportation** - Trailer, 4x4 vehicle, truck with towing
- **Shelter** - Tents, camp stoves, sleeping bags
- **Water** - Storage containers, filtration system
- **Supplies** - First aid kit, food storage

**Model:**
```cpp
class EquipmentCategory
{
    QString m_id;
    QString m_name;
    int m_sortOrder;
};

class Equipment
{
    QString m_id;
    QString m_name;                  // User-configurable (e.g., "Generator", "Chainsaw")
    QString m_categoryId;            // References EquipmentCategory
    QSet<QString> m_familyIds;       // Who has this equipment
};
```

**Storage:**
- `Document::m_equipmentCategories` as `QHash<QString, EquipmentCategory>`
- `Document::m_equipment` as `QHash<QString, Equipment>`

#### 3. Special Needs (Person or Family Level)

Families or individuals who need assistance during emergencies. Freeform notes - no categories or types.

**Model:**
```cpp
struct SpecialNeed
{
    QString personId;    // Set if person-level (optional)
    QString familyId;    // Set if family-level (optional)
    QString note;        // Freeform description
};
```

**Storage:** `Document::m_specialNeeds` as `QList<SpecialNeed>`

**Key design decisions:**
- No `id` field - the personId/familyId serves as the identity (one note per entity)
- No types or categories - too rare and individualized to standardize
- Freeform note captures specifics better than predefined categories

### What Gets Removed

- `ResourceCategory` model
- `ResourceType` model
- `ResourceLevel` enum
- `MarkerDecorationType` (deferred - map markers can be added later if needed)
- All `ResourceCommands`

### JSON Schema Changes

**Before:**
```json
{
  "resourceCategories": [...],
  "resourceTypes": [...]
}
```

**After:**
```json
{
  "skillCategories": [
    { "id": "...", "name": "Medical", "sortOrder": 0 },
    { "id": "...", "name": "Communication", "sortOrder": 1 },
    { "id": "...", "name": "Repair", "sortOrder": 2 }
  ],
  "skills": [
    { "id": "...", "name": "EMT", "categoryId": "...", "personIds": ["p1", "p2"] }
  ],
  "equipmentCategories": [
    { "id": "...", "name": "Power", "sortOrder": 0 },
    { "id": "...", "name": "Tools", "sortOrder": 1 }
  ],
  "equipment": [
    { "id": "...", "name": "Generator", "categoryId": "...", "familyIds": ["f1"] }
  ],
  "specialNeeds": [
    { "familyId": "f2", "note": "Grandma wheelchair-bound, needs accessible transport" }
  ]
}
```

### Migration

Existing documents with `resourceCategories`/`resourceTypes` will need migration:
1. Map old categories to new enums based on name matching
2. Split resources by level into skills vs equipment
3. Drop any that don't cleanly map (user can re-add)

## Commands

### Skill Category Commands
- `AddSkillCategoryCommand(SkillCategory)`
- `UpdateSkillCategoryCommand(oldCategory, newCategory)`
- `DeleteSkillCategoryCommand(SkillCategory)`

### Skill Commands
- `AddSkillCommand(Skill)`
- `UpdateSkillCommand(oldSkill, newSkill)`
- `DeleteSkillCommand(Skill)`
- `AssignSkillToPersonCommand(skillId, personId)`
- `UnassignSkillFromPersonCommand(skillId, personId)`

### Equipment Category Commands
- `AddEquipmentCategoryCommand(EquipmentCategory)`
- `UpdateEquipmentCategoryCommand(oldCategory, newCategory)`
- `DeleteEquipmentCategoryCommand(EquipmentCategory)`

### Equipment Commands
- `AddEquipmentCommand(Equipment)`
- `UpdateEquipmentCommand(oldEquipment, newEquipment)`
- `DeleteEquipmentCommand(Equipment)`
- `AssignEquipmentToFamilyCommand(equipmentId, familyId)`
- `UnassignEquipmentFromFamilyCommand(equipmentId, familyId)`

### Special Need Commands
- `SetSpecialNeedCommand(personId/familyId, note)` - Add or update
- `ClearSpecialNeedCommand(personId/familyId)` - Remove

## UI Considerations (Future)

- Skills view: Tree by category, shows persons with each skill
- Equipment view: Tree by category, shows families with each equipment
- Special Needs view: Simple list of families/persons with notes
- Family edit panel: Show assigned equipment and special needs
- Person edit panel: Show assigned skills

## DocumentChange Scopes

Add new scopes:
- `ChangeScope::SkillCategory`
- `ChangeScope::Skill`
- `ChangeScope::EquipmentCategory`
- `ChangeScope::Equipment`
- `ChangeScope::SpecialNeed`

## Default Categories

New documents are initialized with default categories:

**Skill Categories:**
1. Medical (sortOrder: 0)
2. Communication (sortOrder: 1)
3. Repair (sortOrder: 2)

**Equipment Categories:**
1. Power (sortOrder: 0)
2. Tools (sortOrder: 1)
3. Transportation (sortOrder: 2)
4. Shelter (sortOrder: 3)
5. Water (sortOrder: 4)
6. Supplies (sortOrder: 5)

Users can add, edit, or delete these categories as needed.

## Not In Scope

- Map marker decorations (can be added later based on skills/equipment)
- Import from external sources
