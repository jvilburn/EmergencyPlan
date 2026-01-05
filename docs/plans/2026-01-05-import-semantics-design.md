# Import Semantics Redesign

## Key Insight

The core problem: when ward directory and ministering PDFs have conflicting data, there's no way to know which is outdated. The solution is **PDF date tracking** with **clear semantics per import type**.

| Import Type | Semantics | Data Authority | Date Source |
|-------------|-----------|----------------|-------------|
| Ward Directory | **REPLACE** | Authoritative for family data | File modification date |
| Ministering | **REFERENCE + conditional** | Always authoritative for districts/groups. If newer, also authoritative for persons and ministered families. | Parsed from PDF footer |

**Date comparison is bidirectional:**
- Ward directory import checks against stored ministering date
- Ministering import checks against stored ward directory date

**First import is permissive** - no comparison needed when there's no stored date for the other type.

**Critical distinction in ministering parsing:**
- **Minister artifacts** - Single-person families from minister name references
- **Ministered families** - Real family units from the ministered list

These have fundamentally different merge behaviors. The parser outputs them as separate lists, not inferred from family size.

---

## Family Matching

Family identity is based on **who's in it**, not where it is. Address-based matching fails because:
- Same address ≠ same family (related families sharing a house)
- Different address ≠ different family (family moved)

### Matching Algorithm

1. Find existing families with matching surname
2. For each candidate, count how many members match (using person matching scoring)
3. Best match wins if **majority of members match**

### Fallback for Unmatched Families (when imported data is newer)

When family matching by surname fails, do person-first search to detect **family replacement**:

1. For each person in the new family, search ALL existing families
2. If majority of members found in an existing family → this is a **replacement**
   - Example: Banks family (mom + 2 kids) exists. New Carnline family (dad + mom + 2 kids with Banks surname) imported. Mom and kids match Banks family → Carnline replaces Banks.
   - Remove old family (Banks)
   - Add new family (Carnline) - members stay together in new family structure
   - Preserve person IDs for matched members
3. If no majority match in any existing family → truly new family, add it

**Key point:** The new family's members stay together. Person search determines *which old family is being replaced*, not how to split members.

---

## Person Matching

Person matching is used in two contexts:
1. **Within a matched family** - To preserve person IDs when re-importing
2. **Across all families** - Fallback when family matching fails

### Scoring Algorithm

Find the optimal pairing between old and new members using weighted scoring:

| Factor | Points |
|--------|--------|
| First name exact match | +100 |
| First name partial (Mike/Michael) | +50 |
| Birth year matches | +30 |
| Both are parents | +20 |

**Best match approach:**
- Don't require strict one-to-one by first name
- Find optimal pairing across all members
- Use birth info and parent status as tiebreakers

### Cross-family Search

`findPersonInFamilies(name, allFamilies)` searches all families for a person by name. Used when:
- Family matching fails (Benjamin Banks case)
- Resolving minister artifacts

---

## Ward Directory Import

**REPLACE semantics** with ID preservation and date-aware updates.

### For Each Family in the PDF

1. Match to existing Document family (surname + majority members)
2. If matched:
   - Ward dir newer → replace family data, preserve matched person IDs
   - Ward dir older → only add phone/email if empty
3. If not matched (person-first fallback):
   - Search all families for members
   - If majority found in existing family:
     - Ward dir newer → replace that family, preserve person IDs
     - Ward dir older → discard PDF family, only add phone/email to existing
   - If not found:
     - Add as new family (regardless of date)

### For Document Families NOT in the PDF

| Ward Dir Date | Action |
|---------------|--------|
| Newer | Remove (with cascading cleanup) |
| Older | Keep |

**Date source:** File modification date of the PDF

---

## Ministering Import - Overview

**REFERENCE semantics** for districts/groups. **Conditional authority** for families based on date.

**Parser outputs two separate lists:**
- `ministerFamilies` - artifact families from minister name references
- `ministeredFamilies` - real family units from ministered list

**Two merge phases:**

1. **Internal merge** - Merge minister artifacts with ministered families from same PDF
   - If minister found in ministered family → link, delete artifact
   - If not found → keep in minister list (not ministered list) for proper external merge

2. **External merge** - Merge parsed families into Document (separate rules for each list)

**Date source:** Parsed from PDF footer

**Fail condition:** Import fails if date cannot be parsed from footer

**Key behaviors:**
- Minister artifacts always try to resolve to existing persons
- Ministered families stay together as a unit **only if ministering PDF is newer**
- Districts/groups are always replaced (ministering is authoritative for assignments)

---

## Ministering Import - Internal Merge

Merge minister artifacts with ministered families from the **same PDF** before external merge.

**For each minister artifact:**

1. Search ministered families for the person by name
2. If found:
   - Link minister to that person in the ministered family
   - Delete the artifact from minister list
3. If not found:
   - Keep artifact in minister list
   - Will be handled during external merge into Document

**Example:**
- Minister artifact: "Banks, Benjamin"
- Ministered families include "Carnline" with Benjamin Banks as member
- Benjamin found → link to Carnline, delete Banks artifact

**No date comparison needed** - both come from same PDF.

**Result after internal merge:**
- `ministerFamilies` - only unresolved artifacts remain
- `ministeredFamilies` - unchanged, but with minister linkages

---

## Ministering Import - External Merge (Minister Artifacts)

After internal merge, remaining minister artifacts are merged into Document.

**For each minister artifact:**

1. Search ALL Document families for the person by name
2. If found:
   - Link to existing person ID
   - If ministering newer → update existing family from artifact
   - If ministering older → no update
   - Delete artifact
3. If not found:
   - If ministering newer → keep artifact as placeholder family
   - If ministering older → delete artifact (don't add stale data)

**Summary table:**

| Ministering Date | Person Found | Action |
|------------------|--------------|--------|
| Newer | Yes | Link to existing, update family, delete artifact |
| Newer | No | Keep artifact as placeholder |
| Older | Yes | Link to existing, delete artifact (no update) |
| Older | No | Delete artifact |

---

## Ministering Import - External Merge (Ministered Families)

Ministered families are real family units - members stay together (if newer).

**For each ministered family:**

1. Match to existing Document family (surname + majority members)
2. If matched:
   - Ministering newer → replace existing family, preserve person IDs
   - Ministering older → only fill empty gender/birthdate
3. If not matched (person-first fallback):
   - Search all Document families for members
   - If majority found in existing family:
     - Ministering newer → replace that family, preserve person IDs
     - Ministering older → discard ministered family, only fill empty gender/birthdate
   - If not found:
     - Ministering newer → add as new family
     - Ministering older → discard (don't add stale data)

**Summary table:**

| Ministering Date | Match Type | Action |
|------------------|------------|--------|
| Newer | Family matched | Replace, preserve person IDs |
| Newer | Members found elsewhere | Replace that family, preserve person IDs |
| Newer | Not found | Add as new family |
| Older | Family matched | Only fill empty gender/birthdate |
| Older | Members found elsewhere | Only fill empty gender/birthdate |
| Older | Not found | Discard |

---

## Document Changes

### New Date Fields

- `wardDirectoryDate` - file modification date from last ward directory import
- `ministeringDate` - parsed from footer of last ministering import

Both stored in Document and persisted to JSON.

### Cascading Cleanup in `removeFamily`

When a family is removed, clean up all references:
- Teams → remove person from memberIds
- Tags → remove person/family
- ResourceTypes → remove person/family
- Ministering groups → remove from ministerIds, ministeredPersonIds, familyIds
- Ministering districts → clear presidencyMemberId if matches

### New `removePerson` Method

When a person is removed (but family remains), clean up:
- Teams → remove from memberIds
- Tags → remove personId
- ResourceTypes → remove personId
- Ministering groups → remove from ministerIds, ministeredPersonIds
- Ministering districts → clear presidencyMemberId if matches

---

## Files to Modify

**Models:**
- `Document.h` / `Document.cpp` - Add date fields, cascading cleanup in removeFamily/removePerson

**Services:**
- `WardDirectoryImportService.h` / `.cpp` - Add date comparison, ID preservation, family/person matching
- `MinisteringImportService.h` / `.cpp` - Separate minister/ministered lists, internal merge, date-aware external merge
- `PersonMatching.h` / `.cpp` - Add scored person matching, family matching with majority-member logic, family replacement detection

**Parsers:**
- Ministering PDF parser - Output separate `ministerFamilies` and `ministeredFamilies` lists, extract date from footer

---

## Implementation Steps

1. **Document date fields** - Add `wardDirectoryDate`, `ministeringDate` with JSON serialization

2. **Document cleanup methods** - Implement cascading cleanup in `removeFamily` and add `removePerson`

3. **PersonMatching enhancements:**
   - Add scored person matching (first name, birth info, parent status)
   - Add family matching with majority-member logic
   - Add family replacement detection (person-first search across all families)

4. **Ministering parser changes** - Output separate `ministerFamilies` and `ministeredFamilies` lists, extract date from footer

5. **MinisteringImportService rewrite:**
   - Internal merge (minister artifacts → ministered families)
   - External merge with date-aware logic for both lists
   - Handle minister artifacts and ministered families differently

6. **WardDirectoryImportService update:**
   - Add date comparison against stored ministering date
   - Implement ID preservation with family/person matching
   - Date-aware updates (replace vs fill-empty-fields)
   - Conditional removal of missing families
