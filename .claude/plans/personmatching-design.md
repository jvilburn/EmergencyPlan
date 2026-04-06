# PersonMatching Module Design

## Goal
Unify family and person matching logic used by both `MinisteringImportService` and `WardDirectoryImportService` into the `PersonMatching` module with a deterministic multi-pass algorithm.

## Current State
- `PersonMatching::findBestFamilyMatch` - stub with scoring design (to be replaced)
- `PersonMatching::findPersonInFamilies` - implemented, uses Name::match
- `MinisteringImportService::findMatchingFamily` - surname-based matching with disambiguation
- `MinisteringImportService::findMatchingPerson` - first name matching within a family
- `WardDirectoryImportService` - no matching logic, just returns parsed families

## Problem
Benjamin Banks (surname "Banks") wasn't merged into Carnline family (different surname, same address) because matching is purely surname-based.

---

## Multi-Pass Family Matching Algorithm

### Pass 1: Address + Member Name Match
- **Condition**: Source family has non-empty address AND target family has same address AND at least one member's first name matches
- **Confidence**: High - same household confirms family relationship
- **Edge cases**:
  - If multiple families at same address with member match → use first match (rare edge case)
  - Handles roommates/blended families with different surnames

### Pass 2: Surname Match
- **Condition**: Source family surname matches target family surname (case-insensitive)
- **Disambiguation** (if multiple matches):
  1. Display name match
  2. Address match
  3. Phone match
  4. If still ambiguous → return first match
- **Note**: This is the existing logic from MinisteringImportService

### Pass 3: Full Name + Attribute Match (cautious fallback)
- **Condition**: At least one source member's full name (first + last) matches a person in any target family
- **Required confirmation**: At least one of:
  - Gender matches
  - Birthday matches (year OR month+day)
  - Phone matches
- **Confidence**: Lower - only use when passes 1-2 fail
- **Risk mitigation**: Multiple attribute requirements reduce false positives on common names

---

## API Design

```cpp
namespace PersonMatching
{
    /// Result of a family match
    struct FamilyMatchResult
    {
        std::optional<Family> family;
        MatchConfidence confidence = MatchConfidence::None;
    };

    enum class MatchConfidence
    {
        None,           // No match found
        AttributeMatch, // Pass 3: name + attributes
        SurnameMatch,   // Pass 2: surname-based
        AddressMatch    // Pass 1: address + member (highest)
    };

    /// Find the best matching family using multi-pass algorithm.
    /// Pass 1: Address + member name match (highest confidence)
    /// Pass 2: Surname match with disambiguation
    /// Pass 3: Full name + attribute match (cautious fallback)
    FamilyMatchResult findMatchingFamily(
        const Family& sourceFamily,
        const QHash<QString, Family>& targetFamilies);

    /// Find a matching person within a specific family.
    /// Used after family match to merge individual members.
    std::optional<Person> findMatchingPerson(
        const Person& sourcePerson,
        const Family& targetFamily);

    /// Find a person by name across all families (existing function).
    std::optional<Person> findPersonInFamilies(
        const QString& personName,
        const QList<Family>& families);
}
```

---

## Integration

### MinisteringImportService Changes
1. Remove `findMatchingFamily` private method
2. Remove `findMatchingPerson` private method
3. Call `PersonMatching::findMatchingFamily` in `mergeFamilies`
4. Call `PersonMatching::findMatchingPerson` in `mergeFamilyMembers`

### WardDirectoryImportService Changes
1. Add optional `existingFamilies` parameter to `importFromPdf`
2. Call `PersonMatching::findMatchingFamily` to merge with existing data
3. Use same merge logic as MinisteringImportService (extract to shared utility?)

---

## Implementation Steps

1. Add `MatchConfidence` enum to PersonMatching.h
2. Update `FamilyMatchResult` struct with confidence field
3. Implement `findMatchingFamily` with 3-pass algorithm
4. Add `findMatchingPerson` for within-family matching
5. Update MinisteringImportService to use PersonMatching functions
6. Add helper functions as needed (address comparison, phone comparison)
7. Write unit tests for edge cases

---

## Open Questions

1. **Should merge logic be shared?** Both import services have similar merge patterns. Consider extracting to a shared `FamilyMerger` utility.

2. **Pass 3 attribute requirements**: Is one attribute enough, or should we require two for common names like "John Smith"?

3. **Logging/debugging**: Should matches include a reason string for debugging import issues?
