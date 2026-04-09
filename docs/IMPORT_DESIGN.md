# Import Semantics

## Key Insight
We're fighting a data update problem. The solution is clear semantics:

| Import Type | Semantics | Data Authority |
|-------------|-----------|----------------|
| Ward Directory | **REPLACE** | Authoritative for families/persons |
| Ministering | **REFERENCE** | Authoritative for districts/groups only |

**PDF date matters**: The data source date (not import date) determines which data is newer.
- Ministering PDF: Has date embedded in header — extract it
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

**ID Preservation Logic** (member-based matching via `PersonMatching`):
1. For each parsed family, find existing by surname first, then score member matches (`findFamilyByMembers`)
2. If found: reuse family ID, match members by scored matching (`findBestPersonMatch`) for person IDs
3. If not found: try fallback search across all families (`findReplacedFamily`) for renamed families
4. If still not found: create new IDs (new family in ward)
5. Existing families/persons not in new PDF:
   - If ward directory PDF is newer than ministering PDF → remove + cleanup
   - If ministering PDF is newer → keep (ministering created placeholder, ward directory is stale)

**Cleanup on removal**:
- Remove from ministering groups (ministerIds, ministeredPersonIds, familyIds)
- Remove from district/group presidencyMemberId if matches

**Key distinction**: We're preserving IDs, not merging data. The family data gets completely replaced.

**Benefits of ID preservation**:
- Ministering assignments remain valid (no re-import needed)
- Teams keep their person references
- Emergency assets stay linked

---

## Ministering Import (REFERENCE semantics)

The ministering PDF is the source of truth for districts/groups/assignments only.
Can be imported before or after ward directory (PDF dates determine precedence).

**Behavior**:
1. Parse PDF → get districts, groups, person/family references
2. For each person reference, find matching person in existing families (`findPersonInFamilies`)
3. If found: Add person ID to group
4. If NOT found: Create placeholder family/person from ministering data, then link
   - Placeholder has: name, address, phone (what ministering PDF provides)
   - Ward directory will fill in full details later

**Result**: All groups fully linked. Some families may be placeholders until ward directory import.

---

## Document Fields

Import dates are tracked in `Document`:
- `wardDirectoryPdfDate()` — `std::optional<QDate>` — when the ward directory PDF was created
- `ministeringPdfDate()` — `std::optional<QDate>` — when the ministering PDF was created

---

## PersonMatching API

All functions live in the `PersonMatching` namespace:

- `scorePersonMatch(source, target)` — Multi-factor score: first name, birth date, phone, email, parent status
- `findBestPersonMatch(source, family)` — Best matching person in a family by score
- `findBestFamilyMatch(name, address, phone, families)` — Multi-factor family matching: address, phone, members
- `findPersonInFamilies(name, families)` — Find person by name across families
- `findFamilyByMembers(sourceFamily, targetFamilies)` — Surname-filtered member-based matching with ID mapping
- `findReplacedFamily(sourceFamily, targetFamilies)` — Fallback: search all families ignoring surname (handles name changes)

---

## Files

1. **src/models/Document.h/cpp** — `wardDirectoryPdfDate`, `ministeringPdfDate` fields
2. **src/services/PersonMatching.h/cpp** — Matching functions for ID preservation
3. **src/services/WardDirectoryImportService.h/cpp** — ID preservation, date comparison
4. **src/services/MinisteringImportService.h/cpp** — Placeholder creation for unmatched references
