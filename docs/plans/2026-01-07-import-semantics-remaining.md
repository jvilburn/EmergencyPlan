# Import Semantics - Remaining Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the date-aware import logic for ministering PDFs, with member-based family matching and cascading cleanup.

**Architecture:** Enhance PersonMatching with scored member matching, update MinisteringImportService with date-aware merge logic, add Document::removePerson with cascading cleanup.

**Tech Stack:** Qt 6, C++17, MSVC 2022

---

## Task 1: PersonMatching - Scored Person Matching

**Files:**
- Modify: `src/services/PersonMatching.h`
- Modify: `src/services/PersonMatching.cpp`

**Context:** Current person matching only does simple first name comparison. The design requires weighted scoring: first name exact +100, first name partial +50, birth year +30, both parents +20.

**Step 1: Add PersonMatchScore struct and function declaration to header**

In `PersonMatching.h`, add after the existing `FamilyMatchResult` struct:

```cpp
/// Score for a person match
struct PersonMatchScore
{
    QString personId;
    int score = 0;
};

/// Score how well two persons match.
/// Scoring: firstName exact +100, firstName partial +50, birthYear match +30, both isParent +20
int scorePersonMatch(const Person& source, const Person& target);

/// Find the best matching person in a family using scored matching.
/// Returns the person ID and score. Score of 0 means no match.
PersonMatchScore findBestPersonMatch(const Person& source, const Family& family);
```

**Step 2: Implement scorePersonMatch in cpp**

In `PersonMatching.cpp`, add:

```cpp
int PersonMatching::scorePersonMatch(const Person& source, const Person& target)
{
    int score = 0;

    QString sourceFirst = source.givenNames().split(' ').first().toLower();
    QString targetFirst = target.givenNames().split(' ').first().toLower();

    // First name matching
    if (sourceFirst == targetFirst)
    {
        score += 100;
    }
    else if (sourceFirst.startsWith(targetFirst) || targetFirst.startsWith(sourceFirst))
    {
        // Partial match (Mike/Michael, Bob/Robert won't match this way, but Tom/Thomas might)
        score += 50;
    }
    else
    {
        // No first name match at all - probably not the same person
        return 0;
    }

    // Birth year matching
    if (source.birthday().hasYear() && target.birthday().hasYear()
        && source.birthday().year() == target.birthday().year())
    {
        score += 30;
    }

    // Both are parents
    if (source.isParent() && target.isParent())
    {
        score += 20;
    }

    return score;
}
```

**Step 3: Implement findBestPersonMatch in cpp**

```cpp
PersonMatching::PersonMatchScore PersonMatching::findBestPersonMatch(
    const Person& source,
    const Family& family)
{
    PersonMatchScore best;

    for (const Person& member : family.members())
    {
        int score = scorePersonMatch(source, member);
        if (score > best.score)
        {
            best.personId = member.id();
            best.score = score;
        }
    }

    return best;
}
```

**Step 4: Build and verify no errors**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/services/PersonMatching.h src/services/PersonMatching.cpp
git commit -m "feat(PersonMatching): add scored person matching"
```

---

## Task 2: PersonMatching - Family Matching with Majority-Member Logic

**Files:**
- Modify: `src/services/PersonMatching.h`
- Modify: `src/services/PersonMatching.cpp`

**Context:** Current family matching uses surname + address + phone. Design wants member-based matching: find families with matching surname, then check if majority of members match.

**Step 1: Add new family matching function declaration**

In `PersonMatching.h`, add:

```cpp
/// Result of family matching with member-based scoring
struct FamilyMemberMatchResult
{
    QString familyId;           // Empty if no match
    int matchedMembers = 0;     // How many members matched
    int totalSourceMembers = 0; // How many members in source family
    QHash<QString, QString> personIdMapping;  // source person ID -> target person ID
};

/// Find a matching family using member-based matching.
/// Matches if majority of source members match target family members.
/// Returns empty familyId if no match with majority.
FamilyMemberMatchResult findFamilyByMembers(
    const Family& sourceFamily,
    const QHash<QString, Family>& targetFamilies);
```

**Step 2: Implement findFamilyByMembers**

In `PersonMatching.cpp`, add:

```cpp
PersonMatching::FamilyMemberMatchResult PersonMatching::findFamilyByMembers(
    const Family& sourceFamily,
    const QHash<QString, Family>& targetFamilies)
{
    FamilyMemberMatchResult bestResult;
    bestResult.totalSourceMembers = sourceFamily.members().size();

    // First filter by surname
    QString sourceSurname = sourceFamily.surname().toLower();

    for (const Family& targetFamily : targetFamilies)
    {
        if (targetFamily.surname().toLower() != sourceSurname)
        {
            continue;
        }

        // Count matching members
        FamilyMemberMatchResult current;
        current.familyId = targetFamily.id();
        current.totalSourceMembers = sourceFamily.members().size();

        for (const Person& sourceMember : sourceFamily.members())
        {
            PersonMatchScore match = findBestPersonMatch(sourceMember, targetFamily);
            if (match.score >= 100)  // At least exact first name match
            {
                current.matchedMembers++;
                current.personIdMapping.insert(sourceMember.id(), match.personId);
            }
        }

        // Keep best match
        if (current.matchedMembers > bestResult.matchedMembers)
        {
            bestResult = current;
        }
    }

    // Check majority rule
    if (bestResult.matchedMembers > 0
        && bestResult.matchedMembers >= (bestResult.totalSourceMembers + 1) / 2)
    {
        return bestResult;
    }

    // No majority match
    bestResult.familyId.clear();
    bestResult.personIdMapping.clear();
    return bestResult;
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/services/PersonMatching.h src/services/PersonMatching.cpp
git commit -m "feat(PersonMatching): add member-based family matching"
```

---

## Task 3: PersonMatching - Cross-Family Person Search (Replacement Detection)

**Files:**
- Modify: `src/services/PersonMatching.h`
- Modify: `src/services/PersonMatching.cpp`

**Context:** When family surname matching fails, need to search all families for the person to detect family replacement (e.g., Banks family becomes Carnline family after remarriage).

**Step 1: Add cross-family search function declaration**

In `PersonMatching.h`, add:

```cpp
/// Result of searching for family members across all families
struct FamilyReplacementResult
{
    QString replacedFamilyId;   // The family being replaced (empty if truly new)
    int matchedMembers = 0;     // How many source members found in that family
    int totalSourceMembers = 0;
    QHash<QString, QString> personIdMapping;  // source person ID -> existing person ID
};

/// Search all families for members of the source family (ignoring surname).
/// Used as fallback when surname-based matching fails.
/// Returns the family that contains majority of source members, if any.
FamilyReplacementResult findReplacedFamily(
    const Family& sourceFamily,
    const QHash<QString, Family>& targetFamilies);
```

**Step 2: Implement findReplacedFamily**

In `PersonMatching.cpp`, add:

```cpp
PersonMatching::FamilyReplacementResult PersonMatching::findReplacedFamily(
    const Family& sourceFamily,
    const QHash<QString, Family>& targetFamilies)
{
    FamilyReplacementResult bestResult;
    bestResult.totalSourceMembers = sourceFamily.members().size();

    // For each source member, find which target family they might be in
    QHash<QString, int> familyMatchCounts;  // familyId -> count of matched members
    QHash<QString, QHash<QString, QString>> familyPersonMappings;  // familyId -> (source -> target)

    for (const Person& sourceMember : sourceFamily.members())
    {
        // Search all families (not filtered by surname)
        for (const Family& targetFamily : targetFamilies)
        {
            PersonMatchScore match = findBestPersonMatch(sourceMember, targetFamily);
            if (match.score >= 100)  // At least exact first name match
            {
                familyMatchCounts[targetFamily.id()]++;
                familyPersonMappings[targetFamily.id()].insert(sourceMember.id(), match.personId);
            }
        }
    }

    // Find family with most matches
    for (auto it = familyMatchCounts.begin(); it != familyMatchCounts.end(); ++it)
    {
        if (it.value() > bestResult.matchedMembers)
        {
            bestResult.replacedFamilyId = it.key();
            bestResult.matchedMembers = it.value();
            bestResult.personIdMapping = familyPersonMappings[it.key()];
        }
    }

    // Check majority rule
    if (bestResult.matchedMembers > 0
        && bestResult.matchedMembers >= (bestResult.totalSourceMembers + 1) / 2)
    {
        return bestResult;
    }

    // No majority match - truly new family
    bestResult.replacedFamilyId.clear();
    bestResult.personIdMapping.clear();
    return bestResult;
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/services/PersonMatching.h src/services/PersonMatching.cpp
git commit -m "feat(PersonMatching): add cross-family replacement detection"
```

---

## Task 4: Document - Add removePerson with Cascading Cleanup

**Files:**
- Modify: `src/models/Document.h`
- Modify: `src/models/Document.cpp`

**Context:** When a person is removed (but family remains), need to clean up all references: teams, tags, resource types, ministering groups, ministering districts.

**Step 1: Add declaration to Document.h**

In `Document.h`, add in the public section (near removeFamily):

```cpp
/// Remove a person from all references (teams, tags, resources, ministering).
/// Does NOT remove the person from their family - caller must handle that.
void cleanupPersonReferences(const QString& personId);
```

**Step 2: Implement cleanupPersonReferences in Document.cpp**

Add after the existing removeFamily implementation:

```cpp
void Document::cleanupPersonReferences(const QString& personId)
{
    // Remove from teams
    for (auto it = m_teams.begin(); it != m_teams.end(); ++it)
    {
        if (it->memberIds().contains(personId))
        {
            it->removeMember(personId);
        }
    }

    // Remove from tags
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it)
    {
        if (it->personIds().contains(personId))
        {
            it->removePerson(personId);
        }
    }

    // Remove from resource types
    for (auto it = m_resourceTypes.begin(); it != m_resourceTypes.end(); ++it)
    {
        if (it->personIds().contains(personId))
        {
            it->removePerson(personId);
        }
    }

    // Remove from EQ groups
    for (auto it = m_eqGroups.begin(); it != m_eqGroups.end(); ++it)
    {
        if (it->ministerIds().contains(personId))
        {
            it->removeMinister(personId);
        }
    }

    // Remove from RS groups
    for (auto it = m_rsGroups.begin(); it != m_rsGroups.end(); ++it)
    {
        if (it->ministerIds().contains(personId))
        {
            it->removeMinister(personId);
        }
        if (it->ministeredPersonIds().contains(personId))
        {
            it->removeMinisteredPerson(personId);
        }
    }

    // Clear presidency member ID if matches
    for (auto it = m_eqDistricts.begin(); it != m_eqDistricts.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }
    for (auto it = m_rsDistricts.begin(); it != m_rsDistricts.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }

    // Clear group presidency member ID if matches
    for (auto it = m_eqGroups.begin(); it != m_eqGroups.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }
    for (auto it = m_rsGroups.begin(); it != m_rsGroups.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/models/Document.h src/models/Document.cpp
git commit -m "feat(Document): add cleanupPersonReferences for cascading cleanup"
```

---

## Task 5: MinisteringImportService - Add Date Parameter

**Files:**
- Modify: `src/services/MinisteringImportService.h`
- Modify: `src/services/MinisteringImportService.cpp`

**Context:** The import needs to compare ministering PDF date against ward directory date to determine authority.

**Step 1: Update importFromPdf signature**

In `MinisteringImportService.h`, change:

```cpp
/// Import ministering assignments from a PDF file (auto-detects EQ or RS).
/// If wardDirectoryDate is provided and is newer than the PDF date,
/// family data from the PDF is treated as stale (only fills empty fields).
MinisteringImportResult importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate = std::nullopt);
```

**Step 2: Update implementation signature**

In `MinisteringImportService.cpp`, update the function signature to match:

```cpp
MinisteringImportResult MinisteringImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate)
{
    // ... existing code unchanged for now
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/services/MinisteringImportService.h src/services/MinisteringImportService.cpp
git commit -m "feat(MinisteringImportService): add wardDirectoryDate parameter"
```

---

## Task 6: MinisteringImportService - Add Date Comparison Helper

**Files:**
- Modify: `src/services/MinisteringImportService.h`
- Modify: `src/services/MinisteringImportService.cpp`

**Context:** Need a helper to determine if ministering data is authoritative for family updates.

**Step 1: Add helper declaration in header**

In `MinisteringImportService.h`, add in private section:

```cpp
/// Determine if ministering PDF is authoritative for family data.
/// Returns true if:
/// - wardDirectoryDate is not set (first import), OR
/// - PDF date is set AND is >= wardDirectoryDate
bool isMinisteringAuthoritative(
    std::optional<QDate> pdfDate,
    std::optional<QDate> wardDirectoryDate) const;
```

**Step 2: Implement helper**

In `MinisteringImportService.cpp`, add:

```cpp
bool MinisteringImportService::isMinisteringAuthoritative(
    std::optional<QDate> pdfDate,
    std::optional<QDate> wardDirectoryDate) const
{
    // No ward directory date means first import - ministering is authoritative
    if (!wardDirectoryDate.has_value())
    {
        return true;
    }

    // If PDF date is missing, we can't compare - assume not authoritative
    if (!pdfDate.has_value())
    {
        return false;
    }

    // Ministering is authoritative if same date or newer
    return *pdfDate >= *wardDirectoryDate;
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/services/MinisteringImportService.h src/services/MinisteringImportService.cpp
git commit -m "feat(MinisteringImportService): add date comparison helper"
```

---

## Task 7: MinisteringImportService - Refactor External Merge with Date Awareness

**Files:**
- Modify: `src/services/MinisteringImportService.h`
- Modify: `src/services/MinisteringImportService.cpp`

**Context:** The external merge (ministered families into document) needs to respect date authority. When ministering is older, only fill empty fields; when newer, can replace.

**Step 1: Add authoritative parameter to mergeFamilies**

In `MinisteringImportService.h`, update private section:

```cpp
/// Merge source families into target, remapping IDs in groups and districts.
/// If isAuthoritative is true: can add new families, replace family data
/// If isAuthoritative is false: only fill empty fields, don't add new families
void mergeFamilies(
    QHash<QString, Family>& targetFamilies,
    const QHash<QString, Family>& sourceFamilies,
    QHash<QString, MinisteringDistrict>& districts,
    QHash<QString, MinisteringGroup>& groups,
    bool isAuthoritative);
```

**Step 2: Update mergeFamilies implementation**

In `MinisteringImportService.cpp`, update the method:

```cpp
void MinisteringImportService::mergeFamilies(
    QHash<QString, Family>& targetFamilies,
    const QHash<QString, Family>& sourceFamilies,
    QHash<QString, MinisteringDistrict>& districts,
    QHash<QString, MinisteringGroup>& groups,
    bool isAuthoritative)
{
    QHash<QString, QString> familyIdMapping;
    QHash<QString, QString> personIdMapping;

    for (const Family& sourceFamily : sourceFamilies)
    {
        std::optional<Family> match = findMatchingFamily(sourceFamily, targetFamilies);

        if (match.has_value())
        {
            // Found matching family - merge members and track ID mapping
            familyIdMapping.insert(sourceFamily.id(), match->id());

            std::optional<Family> updated = mergeFamilyMembers(
                sourceFamily, *match, personIdMapping, isAuthoritative);

            if (updated.has_value())
            {
                targetFamilies[match->id()] = *updated;
            }
        }
        else if (isAuthoritative)
        {
            // No match and we're authoritative - add as new family
            familyIdMapping.insert(sourceFamily.id(), sourceFamily.id());
            for (const Person& member : sourceFamily.members())
            {
                personIdMapping.insert(member.id(), member.id());
            }
            targetFamilies.insert(sourceFamily.id(), sourceFamily);
        }
        else
        {
            // No match but not authoritative - don't add, just map IDs to themselves
            // (the family won't exist in target, but groups need valid ID mappings)
            familyIdMapping.insert(sourceFamily.id(), sourceFamily.id());
            for (const Person& member : sourceFamily.members())
            {
                personIdMapping.insert(member.id(), member.id());
            }
        }
    }

    // Remap IDs in groups
    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        it->setMinisterIds(remapIds(it->ministerIds(), personIdMapping));
        it->setFamilyIds(remapIds(it->familyIds(), familyIdMapping));
        it->setMinisteredPersonIds(remapIds(it->ministeredPersonIds(), personIdMapping));
        it->setPresidencyMemberId(remapId(it->presidencyMemberId(), personIdMapping));
    }

    // Remap presidency member IDs in districts
    for (auto it = districts.begin(); it != districts.end(); ++it)
    {
        it->setPresidencyMemberId(remapId(it->presidencyMemberId(), personIdMapping));
    }
}
```

**Step 3: Update mergeFamilyMembers to respect authority**

Update the signature in header:

```cpp
std::optional<Family> mergeFamilyMembers(
    const Family& sourceFamily,
    Family targetFamily,
    QHash<QString, QString>& personIdMapping,
    bool isAuthoritative);
```

Update the implementation - change the update conditions:

```cpp
std::optional<Family> MinisteringImportService::mergeFamilyMembers(
    const Family& sourceFamily,
    Family targetFamily,
    QHash<QString, QString>& personIdMapping,
    bool isAuthoritative)
{
    bool anyChanges = false;

    // Copy address if target doesn't have one (always allowed)
    if (targetFamily.address().isEmpty() && !sourceFamily.address().isEmpty())
    {
        targetFamily.setAddress(sourceFamily.address());
        anyChanges = true;
    }

    QList<Person> updatedMembers = targetFamily.members();

    for (const Person& sourceMember : sourceFamily.members())
    {
        std::optional<Person> existingMember = findMatchingPerson(sourceMember, targetFamily);

        if (existingMember.has_value())
        {
            personIdMapping.insert(sourceMember.id(), existingMember->id());

            // Only update names if authoritative
            bool needsNameUpdate = isAuthoritative
                && (sourceMember.surname() != existingMember->surname()
                    || sourceMember.givenNames() != existingMember->givenNames());

            // isParent = true wins over false (always)
            bool needsIsParentUpdate = sourceMember.isParent() && !existingMember->isParent();

            // Fill empty fields (always allowed)
            bool needsGenderUpdate = sourceMember.gender().has_value()
                && !existingMember->gender().has_value();
            bool needsBirthdayUpdate = sourceMember.birthday().hasDate()
                && !existingMember->birthday().hasDate();

            if (needsNameUpdate || needsIsParentUpdate || needsGenderUpdate
                || needsBirthdayUpdate)
            {
                for (int i = 0; i < updatedMembers.size(); ++i)
                {
                    if (updatedMembers[i].id() == existingMember->id())
                    {
                        Person updated = updatedMembers[i];
                        if (needsNameUpdate)
                        {
                            updated.setName(sourceMember.name());
                        }
                        if (needsIsParentUpdate)
                        {
                            updated.setIsParent(true);
                        }
                        if (needsGenderUpdate)
                        {
                            updated.setGender(sourceMember.gender());
                        }
                        if (needsBirthdayUpdate)
                        {
                            updated.setBirthday(sourceMember.birthday());
                        }
                        updatedMembers[i] = updated;
                        anyChanges = true;
                        break;
                    }
                }
            }
        }
        else if (isAuthoritative)
        {
            // No match and authoritative - add as new member
            personIdMapping.insert(sourceMember.id(), sourceMember.id());
            updatedMembers.append(sourceMember);
            anyChanges = true;
        }
        else
        {
            // No match and not authoritative - just map ID to itself
            personIdMapping.insert(sourceMember.id(), sourceMember.id());
        }
    }

    if (anyChanges)
    {
        targetFamily.setMembers(updatedMembers);
        return targetFamily;
    }

    return std::nullopt;
}
```

**Step 4: Update importFromPdf to use isAuthoritative**

In `importFromPdf`, update the merge calls:

```cpp
MinisteringImportResult MinisteringImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate)
{
    MinisteringImportResult result;
    result.success = false;

    // Parse the PDF
    MinisteringPdfParser::ParseResult parseResult = MinisteringPdfParser::parse(pdfPath);

    if (!parseResult.success)
    {
        result.errors = parseResult.errors;
        return result;
    }

    result.isRSFormat = parseResult.isRSFormat;
    result.wardName = parseResult.wardName;
    result.wardUnitNumber = parseResult.wardUnitNumber;
    result.stakeName = parseResult.stakeName;
    result.stakeUnitNumber = parseResult.stakeUnitNumber;

    if (parseResult.documentDate.isValid())
    {
        result.pdfDate = parseResult.documentDate;
    }

    // Determine if ministering data is authoritative for family changes
    bool isAuthoritative = isMinisteringAuthoritative(result.pdfDate, wardDirectoryDate);

    // Step 1: Internal dedup - always authoritative within same PDF
    mergeFamilies(parseResult.ministeredFamilies,
                  parseResult.ministerFamilies,
                  parseResult.districts,
                  parseResult.groups,
                  true);  // Internal merge is always authoritative

    // Step 2: Merge with document families - respect date authority
    result.families = existingFamilies;
    mergeFamilies(result.families,
                  parseResult.ministeredFamilies,
                  parseResult.districts,
                  parseResult.groups,
                  isAuthoritative);

    result.districts = parseResult.districts;
    result.groups = parseResult.groups;

    result.success = true;
    return result;
}
```

**Step 5: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 6: Commit**

```bash
git add src/services/MinisteringImportService.h src/services/MinisteringImportService.cpp
git commit -m "feat(MinisteringImportService): add date-aware merge logic"
```

---

## Task 8: Update MainWindow to Pass Ward Directory Date

**Files:**
- Modify: `src/widgets/MainWindow.cpp`

**Context:** MainWindow needs to pass the stored ward directory date to the ministering import service.

**Step 1: Update onImportMinisteringPdf**

Find the call to `importService.importFromPdf` and update it:

```cpp
MinisteringImportResult result = importService.importFromPdf(
    pdfPath,
    m_documentManager->document().families(),
    m_documentManager->document().wardDirectoryPdfDate());
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/widgets/MainWindow.cpp
git commit -m "feat(MainWindow): pass ward directory date to ministering import"
```

---

## Task 9: Update MinisteringImportService to Use New PersonMatching

**Files:**
- Modify: `src/services/MinisteringImportService.cpp`

**Context:** Replace the current surname/displayName matching with the new member-based matching from PersonMatching.

**Step 1: Add include**

At the top of `MinisteringImportService.cpp`, add:

```cpp
#include "PersonMatching.h"
```

**Step 2: Update findMatchingFamily to use PersonMatching**

Replace the `findMatchingFamily` implementation:

```cpp
std::optional<Family> MinisteringImportService::findMatchingFamily(
    const Family& pdfFamily,
    const QHash<QString, Family>& families)
{
    // Use member-based matching
    PersonMatching::FamilyMemberMatchResult match =
        PersonMatching::findFamilyByMembers(pdfFamily, families);

    if (!match.familyId.isEmpty())
    {
        return families.value(match.familyId);
    }

    // Fallback: try replacement detection (for surname changes)
    PersonMatching::FamilyReplacementResult replacement =
        PersonMatching::findReplacedFamily(pdfFamily, families);

    if (!replacement.replacedFamilyId.isEmpty())
    {
        return families.value(replacement.replacedFamilyId);
    }

    return std::nullopt;
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/services/MinisteringImportService.cpp
git commit -m "feat(MinisteringImportService): use member-based family matching"
```

---

## Summary

After completing all tasks, the import-semantics feature will have:

1. **Scored person matching** - First name, birth year, parent status
2. **Member-based family matching** - Majority of members must match
3. **Family replacement detection** - Finds families when surname changes
4. **Date-aware merge logic** - Respects ward directory vs ministering dates
5. **Cascading person cleanup** - Removes person references from all collections

### Files Modified

| File | Changes |
|------|---------|
| `src/services/PersonMatching.h` | Added scoring structs and functions |
| `src/services/PersonMatching.cpp` | Implemented matching algorithms |
| `src/models/Document.h` | Added cleanupPersonReferences |
| `src/models/Document.cpp` | Implemented cascading cleanup |
| `src/services/MinisteringImportService.h` | Added date parameter, updated signatures |
| `src/services/MinisteringImportService.cpp` | Date-aware merge, member-based matching |
| `src/widgets/MainWindow.cpp` | Pass ward directory date to import |
