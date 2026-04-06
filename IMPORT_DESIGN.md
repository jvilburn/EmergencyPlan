# Import Semantics Redesign

## Key Insight
We're fighting a data update problem. The solution is clear semantics:

| Import Type | Semantics | Data Authority |
|-------------|-----------|----------------|
| Ward Directory | **REPLACE** | Authoritative for families/persons |
| Ministering | **REFERENCE** | Authoritative for districts/groups only |

**PDF date matters**: The data source date (not import date) determines which data is newer.
- Ministering PDF: Has date embedded in header - extract it
- Ward directory PDF: Use file modification date as proxy (when user downloaded it)

## Problem Solved
- **Married woman**: Ward directory replaces old "Jane Doe" with new "Jane Smith". No duplicate.
- **New baby**: Ward directory has complete family. Replace.
- **Benjamin Banks**: Already in Carnline family from ward directory. Ministering just finds him.

---

## Ward Directory Import (REPLACE with ID preservation)

The ward directory PDF is the source of truth for families and persons.
IDs must be preserved to maintain references from teams, skills, resources.

**Behavior**:
- **First import**: Create families with new IDs
- **Subsequent imports**: Replace family/person DATA but preserve IDs

**ID Preservation Logic** (minimal matching):
1. For each parsed family, find existing by surname + display name
2. If found: reuse family ID, match members by first name for person IDs
3. If not found: create new IDs (new family in ward)
4. Existing families/persons not in new PDF:
   - If ward directory PDF is newer than ministering PDF → remove + cleanup
   - If ministering PDF is newer → keep (ministering created placeholder, ward directory is stale)

**Cleanup on removal**:
- Delete associated skills, resources, teams
- Remove from ministering groups (ministerIds, ministeredPersonIds, familyIds)
- Remove from district/group presidencyMemberId if matches

**Key distinction**: We're preserving IDs, not merging data. The family data gets completely replaced.

**Benefits of ID preservation**:
- Ministering assignments remain valid (no re-import needed)
- Teams keep their person references
- Skills stay linked to persons
- Resources stay linked to families

---

## Ministering Import (REFERENCE semantics)

The ministering PDF is the source of truth for districts/groups/assignments only.
Can be imported before or after ward directory (PDF dates determine precedence).

**Behavior**:
1. Parse PDF → get districts, groups, person/family references
2. For each person reference, find matching person in existing families
3. If found: Add person ID to group
4. If NOT found: Create placeholder family/person from ministering data, then link
   - Placeholder has: name, address, phone (what ministering PDF provides)
   - Ward directory will fill in full details later

**Result**: All groups fully linked. Some families may be placeholders until ward directory import.

**Person matching** (simple - already implemented):
- Use existing `findPersonInFamilies` with Name::match
- Full name match with component fallback (handles middle names)

---

## What Changes

### WardDirectoryImportService
- Add ID preservation logic:
  - Match parsed families to existing by surname + display name
  - Match parsed persons to existing by first name within family
  - Assign existing IDs to matched entities
- Return updated families (with preserved IDs where matched)

### MinisteringImportService
- Create placeholder families/persons for unmatched references
- Store PDF creation date with imported data
- Person references → lookup in existing families via `findPersonInFamilies`
- If not found → create placeholder, then link

### Document (store PDF dates)
- Add `wardDirectoryDate` and `ministeringDate` fields
- Used to determine which data is newer when there's a conflict

### PersonMatching
- Keep existing `findPersonInFamilies` (already works)
- Add `findMatchingFamily` for surname + display name matching (simple, for ID preservation)
- Add `findMatchingPersonInFamily` for first name matching (simple, for ID preservation)

---

## Files to Modify

1. **[Document.h/cpp](src/models/Document.h)** - Add wardDirectoryDate, ministeringDate fields
2. **[PersonMatching.h](src/services/PersonMatching.h)** - Add matching functions for ID preservation
3. **[PersonMatching.cpp](src/services/PersonMatching.cpp)** - Implement surname+displayName and firstName matching
4. **[WardDirectoryImportService.h](src/services/WardDirectoryImportService.h)** - Add existingFamilies parameter
5. **[WardDirectoryImportService.cpp](src/services/WardDirectoryImportService.cpp)** - Add ID preservation, date comparison
6. **[MinisteringImportService.h](src/services/MinisteringImportService.h)** - Add placeholder creation
7. **[MinisteringImportService.cpp](src/services/MinisteringImportService.cpp)** - Create placeholders for unmatched

---

## Implementation Steps

1. Add PDF date fields to Document model
2. Add matching functions to PersonMatching:
   - `findFamilyBySurnameAndDisplayName` - for ID preservation
   - `findPersonByFirstName` - for ID preservation within family
3. Update WardDirectoryImportService:
   - Accept existingFamilies + ministeringDate parameters
   - For each parsed family: match to existing, preserve ID if found
   - For removal: compare wardDirectoryDate vs ministeringDate
4. Update MinisteringImportService:
   - Use `findPersonInFamilies` to resolve person references
   - If not found: create placeholder family/person
   - Return PDF creation date for storage

---

## Scenario Analysis (with REPLACE + ID preservation)

### Case 1: Family has a new baby
- Ward directory PDF has Smith family with John, Jane, Emma
- Match to existing Smith family → preserve family ID
- Match John, Jane by first name → preserve person IDs
- Emma is new → gets new person ID
- ✅ Family/person references preserved, Emma added

### Case 2: Single woman gets married
- Old: "Doe" family with "Jane Doe"
- New: "Smith" family with "Jane Smith"
- Surname + displayName don't match → old removed, new created
- Old "Jane Doe" skills/resources/teams cascade deleted
- New "Jane Smith" starts fresh
- ✅ Clean transition, no orphans

### Case 3: Benjamin Banks
- Ward directory has Carnline family with Benjamin Banks as member
- Benjamin gets person ID from ward directory import
- Ministering PDF references "Banks, Benjamin"
- `findPersonInFamilies("Banks, Benjamin")` finds him
- ✅ Linked correctly

### Case 4: Ward directory update
- Re-import ward directory with updated data
- Families matched by surname + displayName → IDs preserved
- Persons matched by first name → IDs preserved
- ✅ Ministering, teams, skills, resources all remain valid
