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
    WardDirectoryPdfParser::ParseResult parseResult = WardDirectoryPdfParser::parse(pdfPath);

    if (!parseResult.success)
    {
        WardDirectoryImportResult result;
        result.errors = parseResult.errors;
        return result;
    }

    // Get file modification date as proxy for PDF date
    QFileInfo fileInfo(pdfPath);
    return merge(parseResult, existingFamilies, fileInfo.lastModified().date(), ministeringPdfDate);
}

WardDirectoryImportResult WardDirectoryImportService::merge(
    const WardDirectoryPdfParser::ParseResult& parsed,
    const QHash<FamilyId, Family>& existingFamilies,
    std::optional<QDate> pdfDate,
    std::optional<QDate> ministeringPdfDate)
{
    WardDirectoryImportResult result;

    result.success = parsed.success;
    result.errors = parsed.errors;
    result.wardName = parsed.wardName;
    result.wardUnitNumber = parsed.wardUnitNumber;
    result.addressCoordinates = parsed.addressCoordinates;
    result.pdfDate = pdfDate;

    // The existing families no parsed family has matched yet
    QHash<FamilyId, Family> unmatched = existingFamilies;

    // Process each parsed family with ID preservation
    for (const Family& parsedFamily : parsed.families)
    {
        std::optional<Family> existingMatch = findMatchingFamily(parsedFamily, unmatched);

        if (existingMatch.has_value())
        {
            Family merged = mergeWithExisting(parsedFamily, *existingMatch);
            result.families.insert(merged.id(), merged);
            unmatched.remove(existingMatch->id());
        }
        else
        {
            result.families.insert(parsedFamily.id(), parsedFamily);
        }
    }

    // Determine which existing families should be removed
    for (const Family& existingFamily : unmatched)
    {
        if (!ministeringPdfDate.has_value()
            || (result.pdfDate.has_value() && result.pdfDate >= ministeringPdfDate))
        {
            result.removedFamilyIds.insert(existingFamily.id());
        }
        else
        {
            result.families.insert(existingFamily.id(), existingFamily);
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

Family WardDirectoryImportService::mergeWithExisting(
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
            updated.setSpecialNeedNote(existingPerson->specialNeedNote());
            updated.setNote(existingPerson->note());
            updatedMembers.append(updated);
        }
        else
        {
            updatedMembers.append(parsedPerson);
        }
    }

    Family updated = Family::createWithId(existingFamily.id(), parsedFamily.address(), updatedMembers);
    updated.setNote(existingFamily.note());
    return updated;
}
