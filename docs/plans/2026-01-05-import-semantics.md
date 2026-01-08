# Import Semantics Redesign Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement REPLACE semantics for ward directory imports (with ID preservation) and REFERENCE semantics for ministering imports (with placeholder creation).

**Architecture:** Ward directory is authoritative for family/person data (REPLACE), while ministering is authoritative only for districts/groups (REFERENCE). PDF dates determine which source is newer when resolving conflicts.

**Tech Stack:** Qt 6 / C++17, QDate for dates, QHash for collections

---

## Task 1: Add PDF Date Fields to Document Model

**Files:**
- Modify: `src/models/Document.h`
- Modify: `src/models/Document.cpp`

**Step 1: Add date member variables and accessors to Document.h**

Add after line 56 (after rsGroups getter):

```cpp
// ========================================================================
// Import date tracking
// ========================================================================
std::optional<QDate> wardDirectoryPdfDate() const { return m_wardDirectoryPdfDate; }
std::optional<QDate> ministeringPdfDate() const { return m_ministeringPdfDate; }
void setWardDirectoryDate(std::optional<QDate> date);
void setMinisteringDate(std::optional<QDate> date);
```

Add member variables after line 181 (after m_rsGroups):

```cpp
// Import dates (for conflict resolution)
std::optional<QDate> m_wardDirectoryPdfDate;
std::optional<QDate> m_ministeringPdfDate;
```

**Step 2: Build to verify header compiles**

Run: `cmd //c build.bat`
Expected: Build succeeds

**Step 3: Implement setters in Document.cpp**

Add after line 327 (after setRsGroups):

```cpp
void Document::setWardDirectoryDate(std::optional<QDate> date)
{
    m_wardDirectoryPdfDate = date;
}

void Document::setMinisteringDate(std::optional<QDate> date)
{
    m_ministeringPdfDate = date;
}
```

**Step 4: Add JSON serialization for dates**

In `toJson()` after line 488 (after rsGroups serialization):

```cpp
// Import dates
if (m_wardDirectoryPdfDate.has_value())
{
    json["wardDirectoryPdfDate"] = m_wardDirectoryPdfDate->toString(Qt::ISODate);
}
if (m_ministeringPdfDate.has_value())
{
    json["ministeringPdfDate"] = m_ministeringPdfDate->toString(Qt::ISODate);
}
```

In `fromJson()` after line 513 (after rsGroups deserialization):

```cpp
// Import dates
if (json.contains("wardDirectoryPdfDate"))
{
    document.m_wardDirectoryPdfDate = QDate::fromString(
        json["wardDirectoryPdfDate"].toString(), Qt::ISODate);
}
if (json.contains("ministeringPdfDate"))
{
    document.m_ministeringPdfDate = QDate::fromString(
        json["ministeringPdfDate"].toString(), Qt::ISODate);
}
```

**Step 5: Update equality operator**

In `operator==` add to the comparison chain:

```cpp
&& m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
&& m_ministeringPdfDate == other.m_ministeringPdfDate;
```

**Step 6: Build to verify implementation**

Run: `cmd //c build.bat`
Expected: Build succeeds (79/79 targets)

**Step 7: Commit**

```bash
git add src/models/Document.h src/models/Document.cpp
git commit -m "$(cat <<'EOF'
feat(Document): add wardDirectoryPdfDate and ministeringPdfDate fields

Track when PDFs were created to resolve conflicts between ward directory
and ministering data sources.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Add ID Preservation to WardDirectoryImportService

**Files:**
- Modify: `src/services/WardDirectoryImportService.h`
- Modify: `src/services/WardDirectoryImportService.cpp`

**Step 1: Update header with new signature and result fields**

Replace the entire `WardDirectoryImportResult` struct:

```cpp
/// Result of ward directory import operation
struct WardDirectoryImportResult
{
    bool success = false;
    QHash<QString, Family> families;
    QSet<QString> removedFamilyIds;  // Families in existing but not in PDF
    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
    std::optional<QDate> pdfDate;  // File modification date as proxy
};
```

Update the `importFromPdf` signature:

```cpp
/// Import families from a PDF file.
/// Preserves IDs for families/persons that match existing data.
/// @param pdfPath Path to the ward directory PDF
/// @param existingFamilies Current families in document (for ID preservation)
/// @param ministeringPdfDate Date of last ministering import (for removal decisions)
WardDirectoryImportResult importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> ministeringPdfDate = std::nullopt);
```

Add private helper declarations:

```cpp
private:
    /// Find existing family by surname and display name.
    std::optional<Family> findMatchingFamily(
        const Family& pdfFamily,
        const QHash<QString, Family>& families);

    /// Find matching person in family by first name.
    std::optional<Person> findMatchingPerson(
        const Person& person,
        const Family& family);

    /// Preserve IDs from existing family/persons into parsed family.
    Family preserveIds(
        const Family& parsedFamily,
        const Family& existingFamily);
```

**Step 2: Build to verify header compiles**

Run: `cmd //c build.bat`
Expected: Build succeeds

**Step 3: Implement findMatchingFamily**

Add to WardDirectoryImportService.cpp:

```cpp
std::optional<Family> WardDirectoryImportService::findMatchingFamily(
    const Family& pdfFamily,
    const QHash<QString, Family>& families)
{
    // Match by surname + displayName (unique identifier for a family)
    for (const Family& family : families)
    {
        if (family.surname().compare(pdfFamily.surname(), Qt::CaseInsensitive) == 0
            && family.displayName().compare(pdfFamily.displayName(), Qt::CaseInsensitive) == 0)
        {
            return family;
        }
    }
    return std::nullopt;
}
```

**Step 4: Implement findMatchingPerson**

```cpp
std::optional<Person> WardDirectoryImportService::findMatchingPerson(
    const Person& person,
    const Family& family)
{
    QString firstName = person.givenNames().split(' ').first();

    for (const Person& member : family.members())
    {
        QString existingFirstName = member.givenNames().split(' ').first();
        if (existingFirstName.compare(firstName, Qt::CaseInsensitive) == 0)
        {
            return member;
        }
    }
    return std::nullopt;
}
```

**Step 5: Implement preserveIds**

```cpp
Family WardDirectoryImportService::preserveIds(
    const Family& parsedFamily,
    const Family& existingFamily)
{
    // Start with parsed family data but use existing family's ID
    Family result = parsedFamily;

    // Use reflection to set the ID (Family has no setId, so we need a different approach)
    // Actually, Family::create() generates new IDs. We need to copy data into existing.
    // Build a new family by copying existingFamily and updating its data.

    // Create a mapping of parsed person -> existing person ID
    QList<Person> updatedMembers;
    for (const Person& parsedPerson : parsedFamily.members())
    {
        std::optional<Person> existingPerson = findMatchingPerson(parsedPerson, existingFamily);

        if (existingPerson.has_value())
        {
            // Create new person with existing ID but parsed data
            Person updated = Person::createWithId(
                existingPerson->id(),
                parsedPerson.name(),
                parsedPerson.gender(),
                parsedPerson.birthday(),
                parsedPerson.phones(),
                parsedPerson.emails(),
                parsedPerson.callings(),
                parsedPerson.isParent(),
                parsedPerson.wardUnitNumber(),
                parsedPerson.stakeUnitNumber());
            updatedMembers.append(updated);
        }
        else
        {
            // New person, keep the parsed ID
            updatedMembers.append(parsedPerson);
        }
    }

    // Create family with existing ID but parsed data
    return Family::createWithId(
        existingFamily.id(),
        parsedFamily.latitude(),
        parsedFamily.longitude(),
        parsedFamily.address(),
        updatedMembers);
}
```

**Step 6: Add createWithId factory methods**

We need factory methods that accept an ID. Add to Person.h:

```cpp
/// Factory method for creating a person with a specific ID (for ID preservation)
static Person createWithId(
    const QString& id,
    const Name& name,
    std::optional<Gender> gender = std::nullopt,
    const Birthday& birthday = Birthday(),
    const QList<Phone>& phones = QList<Phone>(),
    const QList<Email>& emails = QList<Email>(),
    const QList<Calling>& callings = QList<Calling>(),
    bool isParent = false,
    const QString& wardUnitNumber = QString(),
    const QString& stakeUnitNumber = QString());
```

Add to Family.h:

```cpp
/// Factory method for creating a family with a specific ID (for ID preservation)
static Family createWithId(
    const QString& id,
    std::optional<double> latitude = std::nullopt,
    std::optional<double> longitude = std::nullopt,
    const Address& address = Address(),
    const QList<Person>& members = QList<Person>());
```

**Step 7: Implement createWithId in Person.cpp**

```cpp
Person Person::createWithId(
    const QString& id,
    const Name& name,
    std::optional<Gender> gender,
    const Birthday& birthday,
    const QList<Phone>& phones,
    const QList<Email>& emails,
    const QList<Calling>& callings,
    bool isParent,
    const QString& wardUnitNumber,
    const QString& stakeUnitNumber)
{
    Person person;
    person.m_id = id;
    person.m_name = name;
    person.m_gender = gender;
    person.m_birthday = birthday;
    person.m_phones = phones;
    person.m_emails = emails;
    person.m_callings = callings;
    person.m_isParent = isParent;
    person.m_wardUnitNumber = wardUnitNumber;
    person.m_stakeUnitNumber = stakeUnitNumber;
    return person;
}
```

**Step 8: Implement createWithId in Family.cpp**

```cpp
Family Family::createWithId(
    const QString& id,
    std::optional<double> latitude,
    std::optional<double> longitude,
    const Address& address,
    const QList<Person>& members)
{
    Family family;
    family.m_id = id;
    family.m_latitude = latitude;
    family.m_longitude = longitude;
    family.m_address = address;
    family.m_members = members;
    return family;
}
```

**Step 9: Update importFromPdf implementation**

Replace the implementation in WardDirectoryImportService.cpp:

```cpp
WardDirectoryImportResult WardDirectoryImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> ministeringPdfDate)
{
    WardDirectoryImportResult result;

    // Parse the PDF
    WardDirectoryPdfParser::ParseResult parseResult = WardDirectoryPdfParser::parse(pdfPath);

    result.success = parseResult.success;
    result.errors = parseResult.errors;
    result.wardName = parseResult.wardName;
    result.wardUnitNumber = parseResult.wardUnitNumber;

    if (!parseResult.success)
    {
        return result;
    }

    // Get file modification date as proxy for PDF date
    QFileInfo fileInfo(pdfPath);
    result.pdfDate = fileInfo.lastModified().date();

    // Track which existing families were matched
    QSet<QString> matchedExistingIds;

    // Process each parsed family with ID preservation
    for (const auto& [parsedId, parsedFamily] : parseResult.families.asKeyValueRange())
    {
        std::optional<Family> existingMatch = findMatchingFamily(parsedFamily, existingFamilies);

        if (existingMatch.has_value())
        {
            // Preserve IDs from existing family
            Family preserved = preserveIds(parsedFamily, *existingMatch);
            result.families.insert(preserved.id(), preserved);
            matchedExistingIds.insert(existingMatch->id());
        }
        else
        {
            // New family, use parsed IDs
            result.families.insert(parsedFamily.id(), parsedFamily);
        }
    }

    // Determine which existing families should be removed
    for (const auto& [existingId, existingFamily] : existingFamilies.asKeyValueRange())
    {
        if (!matchedExistingIds.contains(existingId))
        {
            // Family not in new PDF - check if we should remove it
            // Remove if ward directory is newer than ministering, or no ministering date
            if (!ministeringPdfDate.has_value()
                || (result.pdfDate.has_value() && result.pdfDate >= ministeringPdfDate))
            {
                result.removedFamilyIds.insert(existingId);
            }
            else
            {
                // Ministering is newer - keep the family (it's a placeholder)
                result.families.insert(existingId, existingFamily);
            }
        }
    }

    return result;
}
```

**Step 10: Add QFileInfo include**

At top of WardDirectoryImportService.cpp:

```cpp
#include <QFileInfo>
```

**Step 11: Build to verify implementation**

Run: `cmd //c build.bat`
Expected: Build succeeds

**Step 12: Commit**

```bash
git add src/models/Person.h src/models/Person.cpp \
        src/models/Family.h src/models/Family.cpp \
        src/services/WardDirectoryImportService.h \
        src/services/WardDirectoryImportService.cpp
git commit -m "$(cat <<'EOF'
feat(WardDirectoryImportService): add ID preservation for imports

Ward directory imports now preserve existing family/person IDs when
matching by surname+displayName (family) and firstName (person).
This maintains referential integrity with ministering, teams, and tags.

- Add createWithId factory methods to Person and Family
- Add findMatchingFamily and findMatchingPerson helpers
- Track removed families for cleanup
- Use PDF file date for conflict resolution with ministering

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Update ImportWardDirectoryCommand to Handle Cleanup

**Files:**
- Modify: `src/commands/ImportWardDirectoryCommand.h`
- Modify: `src/commands/ImportWardDirectoryCommand.cpp`

**Step 1: Update header with removed family tracking**

Add to constructor parameters:

```cpp
ImportWardDirectoryCommand(
    const QHash<QString, Family>& mergedFamilies,
    const QSet<QString>& removedFamilyIds,  // NEW
    const QString& wardUnitNumber,
    const QString& wardName,
    std::optional<QDate> pdfDate = std::nullopt,  // NEW
    const QString& description = QString());
```

Add member variables:

```cpp
QSet<QString> m_removedFamilyIds;
std::optional<QDate> m_pdfDate;

// For undo - track what was removed
QHash<QString, Family> m_removedFamilies;
```

**Step 2: Implement cleanup in execute()**

After saving previous families, before setting new families:

```cpp
// Save removed families for undo
for (const QString& familyId : m_removedFamilyIds)
{
    auto it = m_previousFamilies.find(familyId);
    if (it != m_previousFamilies.end())
    {
        m_removedFamilies.insert(familyId, *it);
    }
}

// TODO: Cleanup associated data (teams, tags, resources, ministering)
// This will be implemented in Task 5
```

Update the date on document:

```cpp
// Update ward directory date
if (m_pdfDate.has_value())
{
    m_previousWardDirectoryDate = document.wardDirectoryPdfDate();
    document.setWardDirectoryDate(m_pdfDate);
}
```

**Step 3: Update undo() to restore date**

```cpp
// Restore ward directory date
if (m_pdfDate.has_value())
{
    document.setWardDirectoryDate(m_previousWardDirectoryDate);
}
```

**Step 4: Build and commit**

Run: `cmd //c build.bat`
Expected: Build succeeds

```bash
git add src/commands/ImportWardDirectoryCommand.h \
        src/commands/ImportWardDirectoryCommand.cpp
git commit -m "$(cat <<'EOF'
feat(ImportWardDirectoryCommand): track removed families and PDF date

Prepare command to handle cleanup of removed families and track PDF date
for conflict resolution.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Extract Date from Ministering PDF Header

**Files:**
- Modify: `src/services/MinisteringPdfParser.h`
- Modify: `src/services/MinisteringPdfParser.cpp`
- Modify: `src/services/MinisteringImportService.h`
- Modify: `src/services/MinisteringImportService.cpp`

**Step 1: Add pdfDate to ParseResult in MinisteringPdfParser.h**

```cpp
struct ParseResult
{
    // ... existing fields ...
    std::optional<QDate> pdfDate;  // Extracted from header
};
```

**Step 2: Parse date from PDF header**

The ministering PDF header typically contains a date like "As of 15 Dec 2024".
Add parsing logic in MinisteringPdfParser.cpp where the header is parsed.

Look for pattern: `As of (\d{1,2}) (\w{3}) (\d{4})`

```cpp
// In the header parsing section, add:
QRegularExpression datePattern(R"(As of (\d{1,2}) (\w{3}) (\d{4}))");
QRegularExpressionMatch dateMatch = datePattern.match(headerText);
if (dateMatch.hasMatch())
{
    int day = dateMatch.captured(1).toInt();
    QString monthStr = dateMatch.captured(2);
    int year = dateMatch.captured(3).toInt();

    // Convert month abbreviation to number
    static const QMap<QString, int> months = {
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
        {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
        {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };

    int month = months.value(monthStr, 0);
    if (month > 0)
    {
        result.pdfDate = QDate(year, month, day);
    }
}
```

**Step 3: Add pdfDate to MinisteringImportResult**

In MinisteringImportService.h:

```cpp
struct MinisteringImportResult
{
    // ... existing fields ...
    std::optional<QDate> pdfDate;
};
```

**Step 4: Pass date through in MinisteringImportService::importFromPdf**

```cpp
result.pdfDate = parseResult.pdfDate;
```

**Step 5: Build and commit**

Run: `cmd //c build.bat`
Expected: Build succeeds

```bash
git add src/services/MinisteringPdfParser.h src/services/MinisteringPdfParser.cpp \
        src/services/MinisteringImportService.h src/services/MinisteringImportService.cpp
git commit -m "$(cat <<'EOF'
feat(MinisteringPdfParser): extract PDF date from header

Parse "As of DD Mon YYYY" pattern from ministering PDF headers to track
source data date for conflict resolution with ward directory.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Update Import Ministering Commands to Store Date

**Files:**
- Modify: `src/commands/ImportEQMinisteringCommand.h`
- Modify: `src/commands/ImportEQMinisteringCommand.cpp`
- Modify: `src/commands/ImportRSMinisteringCommand.h`
- Modify: `src/commands/ImportRSMinisteringCommand.cpp`

**Step 1: Add pdfDate parameter to constructors**

Both commands should accept and store `std::optional<QDate> pdfDate`.

**Step 2: Update execute() to set ministering date on document**

```cpp
// Save previous date for undo
m_previousMinisteringDate = document.ministeringPdfDate();

// Update ministering date
if (m_pdfDate.has_value())
{
    document.setMinisteringDate(m_pdfDate);
}
```

**Step 3: Update undo() to restore date**

```cpp
document.setMinisteringDate(m_previousMinisteringDate);
```

**Step 4: Build and commit**

Run: `cmd //c build.bat`
Expected: Build succeeds

```bash
git add src/commands/ImportEQMinisteringCommand.h src/commands/ImportEQMinisteringCommand.cpp \
        src/commands/ImportRSMinisteringCommand.h src/commands/ImportRSMinisteringCommand.cpp
git commit -m "$(cat <<'EOF'
feat(MinisteringCommands): track PDF date in document

Store ministering PDF date in Document for conflict resolution with
ward directory imports.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Implement Cleanup for Removed Families

**Files:**
- Modify: `src/commands/ImportWardDirectoryCommand.cpp`

**Step 1: Add cleanup helper method**

```cpp
void ImportWardDirectoryCommand::cleanupRemovedFamily(
    Document& document,
    const QString& familyId,
    const Family& family)
{
    // Remove from teams
    for (const Person& member : family.members())
    {
        for (const auto& [teamId, team] : document.teams().asKeyValueRange())
        {
            if (team.memberIds().contains(member.id()))
            {
                document.removeMemberFromTeam(teamId, member.id());
            }
        }
    }

    // Remove from tags (both family and person tags)
    for (const auto& [tagId, tag] : document.tags().asKeyValueRange())
    {
        if (tag.entityIds().contains(familyId))
        {
            document.removeFamilyFromTag(tagId, familyId);
        }
        for (const Person& member : family.members())
        {
            if (tag.entityIds().contains(member.id()))
            {
                document.removePersonFromTag(tagId, member.id());
            }
        }
    }

    // Remove from resource types
    for (const auto& [rtId, rt] : document.resourceTypes().asKeyValueRange())
    {
        if (rt.familyIds().contains(familyId))
        {
            document.removeFamilyFromResourceType(rtId, familyId);
        }
        for (const Person& member : family.members())
        {
            if (rt.personIds().contains(member.id()))
            {
                document.removePersonFromResourceType(rtId, member.id());
            }
        }
    }

    // Remove from ministering groups (EQ)
    for (auto& [groupId, group] : document.eqGroups())
    {
        bool modified = false;
        QSet<QString> ministerIds = group.ministerIds();
        QSet<QString> familyIds = group.familyIds();
        QSet<QString> ministeredPersonIds = group.ministeredPersonIds();

        for (const Person& member : family.members())
        {
            if (ministerIds.remove(member.id())) modified = true;
            if (ministeredPersonIds.remove(member.id())) modified = true;
        }
        if (familyIds.remove(familyId)) modified = true;

        if (modified)
        {
            group.setMinisterIds(ministerIds);
            group.setFamilyIds(familyIds);
            group.setMinisteredPersonIds(ministeredPersonIds);
        }

        // Clear presidencyMemberId if it matches
        for (const Person& member : family.members())
        {
            if (group.presidencyMemberId() == member.id())
            {
                group.setPresidencyMemberId(std::nullopt);
            }
        }
    }

    // Remove from ministering groups (RS) - same logic
    for (auto& [groupId, group] : document.rsGroups())
    {
        // ... same as EQ ...
    }

    // Remove from district presidencies
    for (auto& [districtId, district] : document.eqDistricts())
    {
        for (const Person& member : family.members())
        {
            if (district.presidencyMemberId() == member.id())
            {
                district.setPresidencyMemberId(std::nullopt);
            }
        }
    }
    for (auto& [districtId, district] : document.rsDistricts())
    {
        for (const Person& member : family.members())
        {
            if (district.presidencyMemberId() == member.id())
            {
                district.setPresidencyMemberId(std::nullopt);
            }
        }
    }
}
```

**Step 2: Call cleanup in execute()**

After setting new families:

```cpp
// Cleanup removed families
for (const QString& familyId : m_removedFamilyIds)
{
    if (m_removedFamilies.contains(familyId))
    {
        cleanupRemovedFamily(document, familyId, m_removedFamilies[familyId]);
    }
}
```

**Step 3: Build and commit**

Run: `cmd //c build.bat`
Expected: Build succeeds

```bash
git add src/commands/ImportWardDirectoryCommand.h \
        src/commands/ImportWardDirectoryCommand.cpp
git commit -m "$(cat <<'EOF'
feat(ImportWardDirectoryCommand): cleanup removed families

When families are removed during ward directory import, also clean up:
- Team memberships
- Tag associations
- Resource type associations
- Ministering group assignments
- District presidency assignments

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Update UI/Widget Code to Pass Dates

**Files:**
- Search for where `WardDirectoryImportService::importFromPdf` is called
- Search for where `MinisteringImportService::importFromPdf` is called
- Update call sites to pass required parameters

**Step 1: Find all call sites**

```bash
grep -rn "importFromPdf" src/widgets/ src/commands/
```

**Step 2: Update each call site**

Pass existing families and dates from document context.

**Step 3: Build and commit**

Run: `cmd //c build.bat`
Expected: Build succeeds

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat: integrate import semantics changes into UI

Update widget code to pass existing families and dates to import services.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Final Build and Integration Test

**Step 1: Full clean rebuild**

```bash
rm -rf build && cmd //c build.bat
```

**Step 2: Manual integration test**

1. Launch app: `build/bin/EmergencyPlan.exe`
2. Import ward directory PDF
3. Import ministering PDF
4. Re-import ward directory - verify IDs preserved
5. Check that ministering assignments still work
6. Save/load document - verify dates persisted

**Step 3: Final commit if any fixes needed**

---

## Summary of Changes

| File | Change |
|------|--------|
| Document.h/cpp | Add wardDirectoryPdfDate, ministeringPdfDate fields |
| Person.h/cpp | Add createWithId factory method |
| Family.h/cpp | Add createWithId factory method |
| WardDirectoryImportService.h/cpp | ID preservation logic, date handling |
| MinisteringPdfParser.h/cpp | Extract date from PDF header |
| MinisteringImportService.h/cpp | Pass through PDF date |
| ImportWardDirectoryCommand.h/cpp | Track removed families, cleanup, date |
| ImportEQMinisteringCommand.h/cpp | Store ministering date |
| ImportRSMinisteringCommand.h/cpp | Store ministering date |
| Widget call sites | Pass existing families and dates |
