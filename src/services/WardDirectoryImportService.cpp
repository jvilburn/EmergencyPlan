#include "WardDirectoryImportService.h"
#include "PersonMatching.h"

#include <QFileInfo>

namespace
{
    std::optional<Family> findMatchingFamily(
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

    /// parsedFamily with existingFamily's id and note. A matched member is the existing
    /// person with what the directory prints set over it: name, phone, email, callings and
    /// whether they are a parent.
    Family mergeWithExisting(
        const Family& parsedFamily,
        const Family& existingFamily)
    {
        QHash<PersonId, PersonId> matches = PersonMatching::matchMembers(parsedFamily, existingFamily);

        QList<Person> updatedMembers;
        for (const Person& parsedPerson : parsedFamily.members())
        {
            std::optional<Person> existingPerson;
            auto match = matches.constFind(parsedPerson.id());
            for (const Person& member : existingFamily.members())
            {
                if (match != matches.constEnd() && member.id() == *match)
                {
                    existingPerson = member;
                }
            }

            if (existingPerson.has_value())
            {
                Person updated = *existingPerson;
                updated.setName(parsedPerson.name());
                updated.setPhone(parsedPerson.phone());
                updated.setEmail(parsedPerson.email());
                updated.setCallings(parsedPerson.callings());
                updated.setIsParent(parsedPerson.isParent());
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
}  // anonymous namespace

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
