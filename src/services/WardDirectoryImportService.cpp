#include "WardDirectoryImportService.h"

#include <QFileInfo>

// ============================================================================
// Constructor / Destructor
// ============================================================================

WardDirectoryImportService::WardDirectoryImportService(QObject* parent)
    : QObject(parent)
{
}

WardDirectoryImportService::~WardDirectoryImportService()
{
}

// ============================================================================
// Public API
// ============================================================================

WardDirectoryImportResult WardDirectoryImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<FamilyId, Family>& existingFamilies,
    std::optional<QDate> ministeringPdfDate)
{
    WardDirectoryImportResult result;

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
    QSet<FamilyId> matchedExistingIds;

    // Process each parsed family with ID preservation
    for (const auto& [parsedId, parsedFamily] : parseResult.families.asKeyValueRange())
    {
        std::optional<Family> existingMatch = findMatchingFamily(parsedFamily, existingFamilies);

        if (existingMatch.has_value())
        {
            Family preserved = preserveIds(parsedFamily, *existingMatch);
            result.families.insert(preserved.id(), preserved);
            matchedExistingIds.insert(existingMatch->id());
        }
        else
        {
            result.families.insert(parsedFamily.id(), parsedFamily);
        }
    }

    // Determine which existing families should be removed
    for (const auto& [existingId, existingFamily] : existingFamilies.asKeyValueRange())
    {
        if (!matchedExistingIds.contains(existingId))
        {
            if (!ministeringPdfDate.has_value()
                || (result.pdfDate.has_value() && result.pdfDate >= ministeringPdfDate))
            {
                result.removedFamilyIds.insert(existingId);
            }
            else
            {
                result.families.insert(existingId, existingFamily);
            }
        }
    }

    return result;
}

// ============================================================================
// Private Helpers
// ============================================================================

std::optional<Family> WardDirectoryImportService::findMatchingFamily(
    const Family& pdfFamily,
    const QHash<FamilyId, Family>& families)
{
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

std::optional<Person> WardDirectoryImportService::findMatchingPerson(
    const Person& person,
    const Family& family)
{
    QString firstName = person.givenNames().split(' ').first();
    if (firstName.isEmpty())
    {
        return std::nullopt;
    }

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

Family WardDirectoryImportService::preserveIds(
    const Family& parsedFamily,
    const Family& existingFamily)
{
    QList<Person> updatedMembers;
    for (const Person& parsedPerson : parsedFamily.members())
    {
        std::optional<Person> existingPerson = findMatchingPerson(parsedPerson, existingFamily);

        if (existingPerson.has_value())
        {
            Person updated = Person::createWithId(
                existingPerson->id(),
                parsedPerson.name(),
                parsedPerson.gender(),
                parsedPerson.birthday(),
                parsedPerson.phone(),
                parsedPerson.altPhone(),
                parsedPerson.email(),
                parsedPerson.callings(),
                parsedPerson.isParent(),
                parsedPerson.wardUnitNumber(),
                parsedPerson.stakeUnitNumber());
            updatedMembers.append(updated);
        }
        else
        {
            updatedMembers.append(parsedPerson);
        }
    }

    return Family::createWithId(
        existingFamily.id(),
        parsedFamily.latitude(),
        parsedFamily.longitude(),
        parsedFamily.address(),
        updatedMembers);
}
