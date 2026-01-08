#include "MinisteringImportService.h"
#include "MinisteringPdfParser.h"

namespace
{
    // Helper to remap a set of IDs using a mapping
    QSet<QString> remapIds(const QSet<QString>& ids, const QHash<QString, QString>& mapping)
    {
        QSet<QString> result;
        for (const QString& id : ids)
        {
            result.insert(mapping.value(id, id));
        }
        return result;
    }

    // Helper to remap a single optional ID
    std::optional<QString> remapId(const std::optional<QString>& id,
                                   const QHash<QString, QString>& mapping)
    {
        if (!id.has_value())
        {
            return std::nullopt;
        }
        return mapping.value(*id, *id);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

MinisteringImportService::MinisteringImportService(QObject* parent)
    : QObject(parent)
{
}

MinisteringImportService::~MinisteringImportService()
{
}

// ============================================================================
// Public API
// ============================================================================

MinisteringImportResult MinisteringImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<QString, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate)
{
    Q_UNUSED(wardDirectoryDate)  // Will be used in future tasks
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

    // Pass through PDF date
    if (parseResult.documentDate.isValid())
    {
        result.pdfDate = parseResult.documentDate;
    }

    // Step 1: Internal dedup - merge minister families into ministered families
    // This handles the case where the same family appears as both minister and ministered
    mergeFamilies(parseResult.ministeredFamilies,
                  parseResult.ministerFamilies,
                  parseResult.districts,
                  parseResult.groups);

    // Step 2: Merge with document families
    result.families = existingFamilies;
    mergeFamilies(result.families,
                  parseResult.ministeredFamilies,
                  parseResult.districts,
                  parseResult.groups);

    // Copy final districts and groups to result
    result.districts = parseResult.districts;
    result.groups = parseResult.groups;

    result.success = true;
    return result;
}

// ============================================================================
// Private Methods - Merge Functions
// ============================================================================

void MinisteringImportService::mergeFamilies(
    QHash<QString, Family>& targetFamilies,
    const QHash<QString, Family>& sourceFamilies,
    QHash<QString, MinisteringDistrict>& districts,
    QHash<QString, MinisteringGroup>& groups)
{
    QHash<QString, QString> familyIdMapping;
    QHash<QString, QString> personIdMapping;

    // Merge each source family into target
    for (const Family& sourceFamily : sourceFamilies)
    {
        std::optional<Family> match = findMatchingFamily(sourceFamily, targetFamilies);

        if (match.has_value())
        {
            // Found matching family - merge members and track ID mapping
            familyIdMapping.insert(sourceFamily.id(), match->id());

            std::optional<Family> updated = mergeFamilyMembers(
                sourceFamily, *match, personIdMapping);

            if (updated.has_value())
            {
                // Replace the matched family in targetFamilies with updated version
                targetFamilies[match->id()] = *updated;
            }
        }
        else
        {
            // No match - add as new family (IDs remain unchanged)
            familyIdMapping.insert(sourceFamily.id(), sourceFamily.id());
            for (const Person& member : sourceFamily.members())
            {
                personIdMapping.insert(member.id(), member.id());
            }
            targetFamilies.insert(sourceFamily.id(), sourceFamily);
        }
    }

    // Remap IDs in groups (all fields - one set will be empty based on EQ/RS format)
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

// ============================================================================
// Private Methods - Matching Functions
// ============================================================================

std::optional<Family> MinisteringImportService::findMatchingFamily(
    const Family& pdfFamily,
    const QHash<QString, Family>& families)
{
    // Collect all surname matches
    QList<Family> surnameMatches;
    for (const Family& family : families)
    {
        if (family.surname().compare(pdfFamily.surname(), Qt::CaseInsensitive) == 0)
        {
            surnameMatches.append(family);
        }
    }

    if (surnameMatches.isEmpty())
    {
        return std::nullopt;
    }

    if (surnameMatches.size() == 1)
    {
        return surnameMatches.first();
    }

    // Multiple surname matches - try display name
    QList<Family> displayNameMatches;
    for (const Family& candidate : surnameMatches)
    {
        if (candidate.displayName().compare(pdfFamily.displayName(), Qt::CaseInsensitive) == 0)
        {
            displayNameMatches.append(candidate);
        }
    }

    if (displayNameMatches.size() == 1)
    {
        return displayNameMatches.first();
    }

    // If we have display name matches, use those; otherwise fall back to surname matches
    QList<Family>& candidates = displayNameMatches.isEmpty() ? surnameMatches : displayNameMatches;

    // Still multiple - disambiguate by address
    if (!pdfFamily.address().isEmpty())
    {
        for (const Family& candidate : candidates)
        {
            if (candidate.address() == pdfFamily.address())
            {
                return candidate;
            }
        }
    }

    // Try phone disambiguation
    Phone pdfPhone = pdfFamily.displayPhone();
    if (!pdfPhone.isEmpty())
    {
        for (const Family& candidate : candidates)
        {
            for (const Phone& candidatePhone : candidate.allPhoneNumbers())
            {
                if (pdfPhone == candidatePhone)
                {
                    return candidate;
                }
            }
        }
    }

    // Can't disambiguate further - return first match
    return candidates.first();
}

std::optional<Person> MinisteringImportService::findMatchingPerson(
    const Person& person,
    const Family& family)
{
    QString firstName = person.givenNames().split(' ').first();

    // Collect all first name matches
    QList<Person> candidates;
    for (const Person& member : family.members())
    {
        QString existingFirstName = member.givenNames().split(' ').first();
        if (existingFirstName.compare(firstName, Qt::CaseInsensitive) == 0)
        {
            candidates.append(member);
        }
    }

    if (candidates.isEmpty())
    {
        return std::nullopt;
    }

    if (candidates.size() == 1)
    {
        return candidates.first();
    }

    // Multiple first name matches - try isParent disambiguation
    for (const Person& candidate : candidates)
    {
        if (person.isParent() == candidate.isParent())
        {
            return candidate;
        }
    }

    // Can't disambiguate further - return first match
    return candidates.first();
}

std::optional<Family> MinisteringImportService::mergeFamilyMembers(
    const Family& sourceFamily,
    Family targetFamily,
    QHash<QString, QString>& personIdMapping)
{
    bool anyChanges = false;

    // Copy address if target doesn't have one
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
            // Found matching member - map IDs
            personIdMapping.insert(sourceMember.id(), existingMember->id());

            // Check if we need to update this member
            bool needsNameUpdate = sourceMember.surname() != existingMember->surname()
                || sourceMember.givenNames() != existingMember->givenNames();
            // isParent = true wins over false
            bool needsIsParentUpdate = sourceMember.isParent() && !existingMember->isParent();
            // Copy gender if source has it and target doesn't
            bool needsGenderUpdate = sourceMember.gender().has_value()
                && !existingMember->gender().has_value();
            // Copy birthday if source has date and target doesn't
            bool needsBirthdayUpdate = sourceMember.birthday().hasDate()
                && !existingMember->birthday().hasDate();

            if (needsNameUpdate || needsIsParentUpdate || needsGenderUpdate
                || needsBirthdayUpdate)
            {
                // Find and update the member in the list
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
        else
        {
            // No match - add as new member (keep source ID)
            personIdMapping.insert(sourceMember.id(), sourceMember.id());
            updatedMembers.append(sourceMember);
            anyChanges = true;
        }
    }

    if (anyChanges)
    {
        targetFamily.setMembers(updatedMembers);
        return targetFamily;
    }

    return std::nullopt;
}

// ============================================================================
// Private Methods - Date Comparison
// ============================================================================

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
