#include "MinisteringImportService.h"
#include "PersonMatching.h"

namespace
{
    // Helper to remap a set of IDs using a mapping, leaving out the ids in leftOut
    template<typename Id>
    QSet<Id> remapIds(const QSet<Id>& ids, const QHash<Id, Id>& mapping, const QSet<Id>& leftOut)
    {
        QSet<Id> result;
        for (const Id& id : ids)
        {
            if (!leftOut.contains(id))
            {
                result.insert(mapping.value(id, id));
            }
        }
        return result;
    }

    // Helper to remap a single optional ID, which is none when leftOut holds it
    template<typename Id>
    std::optional<Id> remapId(const std::optional<Id>& id,
                              const QHash<Id, Id>& mapping,
                              const QSet<Id>& leftOut)
    {
        if (!id.has_value() || leftOut.contains(*id))
        {
            return std::nullopt;
        }
        return mapping.value(*id, *id);
    }

    // ========================================================================
    // Merging
    // ========================================================================

    /// True when the PDF's birthday says something the held one does not: another day or
    /// month, or a year the held one lacks or differs from. A PDF prints no year for adults,
    /// so a held year stays unless the day changed.
    bool birthdayDiffers(const Birthday& printed, const Birthday& held)
    {
        return printed.month() != held.month()
            || printed.day() != held.day()
            || (printed.year().has_value() && printed.year() != held.year());
    }

    /// Merge PDF family members into existing family, matched as PersonMatching::matchMembers
    /// matches them. Builds personIdMapping. Returns updated family if any changes made.
    /// A field the PDF prints replaces the held one when the PDF is authoritative and they
    /// differ, and fills it only when it is empty otherwise; a field the PDF leaves out is
    /// kept. An authoritative PDF also adds the members it cannot match.
    std::optional<Family> mergeFamilyMembers(
        const Family& sourceFamily,
        Family targetFamily,
        QHash<PersonId, PersonId>& personIdMapping,
        bool isAuthoritative)
    {
        // Whether a field the PDF prints replaces the held one
        auto takes = [isAuthoritative](bool printed, bool differs, bool heldEmpty)
        { return printed && (isAuthoritative ? differs : heldEmpty); };

        bool anyChanges = false;

        if (takes(!sourceFamily.address().isEmpty(), sourceFamily.address() != targetFamily.address(),
                  targetFamily.address().isEmpty()))
        {
            targetFamily.setAddress(sourceFamily.address());
            anyChanges = true;
        }

        QList<Person> updatedMembers = targetFamily.members();
        QHash<PersonId, PersonId> matches = PersonMatching::matchMembers(sourceFamily, targetFamily);

        for (const Person& sourceMember : sourceFamily.members())
        {
            auto match = matches.constFind(sourceMember.id());
            if (match == matches.constEnd())
            {
                if (isAuthoritative)
                {
                    personIdMapping.insert(sourceMember.id(), sourceMember.id());
                    updatedMembers.append(sourceMember);
                    anyChanges = true;
                }
                // Not authoritative: the member is not added, and gets no mapping
                continue;
            }
            personIdMapping.insert(sourceMember.id(), *match);

            for (Person& member : updatedMembers)
            {
                if (member.id() != *match)
                {
                    continue;
                }
                Person updated = member;
                if (isAuthoritative && sourceMember.name() != member.name())
                {
                    updated.setName(sourceMember.name());
                }
                // A parent stays a parent: the PDF marks parents by indent alone
                if (sourceMember.isParent())
                {
                    updated.setIsParent(true);
                }
                if (takes(sourceMember.gender().has_value(), sourceMember.gender() != member.gender(),
                          !member.gender().has_value()))
                {
                    updated.setGender(sourceMember.gender());
                }
                if (takes(sourceMember.birthday().hasDate(), birthdayDiffers(sourceMember.birthday(), member.birthday()),
                          !member.birthday().hasDate()))
                {
                    updated.setBirthday(sourceMember.birthday());
                }
                if (takes(!sourceMember.phone().isEmpty(), !sourceMember.phone().matches(member.phone()),
                          member.phone().isEmpty()))
                {
                    updated.setPhone(sourceMember.phone());
                }
                if (takes(!sourceMember.email().isEmpty(),
                          sourceMember.email().compare(member.email(), Qt::CaseInsensitive) != 0,
                          member.email().isEmpty()))
                {
                    updated.setEmail(sourceMember.email());
                }
                if (updated != member)
                {
                    member = updated;
                    anyChanges = true;
                }
                break;
            }
        }

        if (anyChanges)
        {
            targetFamily.setMembers(updatedMembers);
            return targetFamily;
        }

        return std::nullopt;
    }

    /// Merge source families into target, remapping IDs in groups and districts.
    /// - Matches families and persons as PersonMatching does
    /// - Merges each matched family's fields as mergeFamilyMembers does
    /// - When isAuthoritative: unmatched families are added
    /// - When not authoritative: no new families or members are added, and the source
    ///   families and persons left unmatched are left out of the groups and districts
    void mergeFamilies(
        QHash<FamilyId, Family>& targetFamilies,
        const QHash<FamilyId, Family>& sourceFamilies,
        QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
        QHash<MinisteringGroupId, MinisteringGroup>& groups,
        bool isAuthoritative)
    {
        QHash<FamilyId, FamilyId> familyIdMapping;
        QHash<PersonId, PersonId> personIdMapping;

        // Merge each source family into target
        for (const Family& sourceFamily : sourceFamilies)
        {
            std::optional<FamilyId> matchId = PersonMatching::findFamily(sourceFamily, targetFamilies);
            std::optional<Family> match;
            if (matchId.has_value())
            {
                match = targetFamilies.value(*matchId);
            }

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
                // No match and authoritative - add as new family
                familyIdMapping.insert(sourceFamily.id(), sourceFamily.id());
                for (const Person& member : sourceFamily.members())
                {
                    personIdMapping.insert(member.id(), member.id());
                }
                targetFamilies.insert(sourceFamily.id(), sourceFamily);
            }
            // No match and not authoritative - the family is not added, and gets no mapping
        }

        // The source families and persons with no mapping, which the merge left unmatched
        QSet<FamilyId> leftOutFamilies;
        QSet<PersonId> leftOutPersons;
        for (const Family& sourceFamily : sourceFamilies)
        {
            if (!familyIdMapping.contains(sourceFamily.id()))
            {
                leftOutFamilies.insert(sourceFamily.id());
            }
            for (const Person& member : sourceFamily.members())
            {
                if (!personIdMapping.contains(member.id()))
                {
                    leftOutPersons.insert(member.id());
                }
            }
        }

        // Remap IDs in groups (all fields - one set will be empty based on EQ/RS format)
        for (auto it = groups.begin(); it != groups.end(); ++it)
        {
            it->setMinisterIds(remapIds(it->ministerIds(), personIdMapping, leftOutPersons));
            it->setFamilyIds(remapIds(it->familyIds(), familyIdMapping, leftOutFamilies));
            it->setMinisteredPersonIds(remapIds(it->ministeredPersonIds(), personIdMapping, leftOutPersons));
            it->setPresidencyMemberId(remapId(it->presidencyMemberId(), personIdMapping, leftOutPersons));
        }

        // Remap presidency member IDs in districts
        for (auto it = districts.begin(); it != districts.end(); ++it)
        {
            it->setPresidencyMemberId(remapId(it->presidencyMemberId(), personIdMapping, leftOutPersons));
        }
    }

    // ========================================================================
    // Date Comparison
    // ========================================================================

    /// Determine if ministering PDF is authoritative for family data.
    /// Returns true if:
    /// - wardDirectoryDate is not set (first import), OR
    /// - PDF date is set AND is >= wardDirectoryDate
    bool isMinisteringAuthoritative(
        std::optional<QDate> pdfDate,
        std::optional<QDate> wardDirectoryDate)
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
}  // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

MinisteringImportResult MinisteringImportService::importFromPdf(
    const QString& pdfPath,
    const QHash<FamilyId, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate)
{
    // Parse the PDF
    MinisteringPdfParser::ParseResult parseResult = MinisteringPdfParser::parse(pdfPath);

    if (!parseResult.success)
    {
        MinisteringImportResult result;
        result.errors = parseResult.errors;
        return result;
    }

    return merge(parseResult, existingFamilies, wardDirectoryDate);
}

MinisteringImportResult MinisteringImportService::merge(
    MinisteringPdfParser::ParseResult parseResult,
    const QHash<FamilyId, Family>& existingFamilies,
    std::optional<QDate> wardDirectoryDate)
{
    MinisteringImportResult result;

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

    // Determine if ministering data is authoritative for family changes
    bool isAuthoritative = isMinisteringAuthoritative(result.pdfDate, wardDirectoryDate);

    // Step 1: Internal dedup - merge minister families into ministered families
    // This handles the case where the same family appears as both minister and ministered
    // Internal merge is always authoritative within the same PDF
    mergeFamilies(parseResult.ministeredFamilies,
                  parseResult.ministerFamilies,
                  parseResult.districts,
                  parseResult.groups,
                  true);

    // Step 2: Merge with document families - respect date authority
    if (existingFamilies.isEmpty())
    {
        // No existing families - just copy directly (no matching needed)
        result.families = parseResult.ministeredFamilies;
    }
    else
    {
        result.families = existingFamilies;
        mergeFamilies(result.families,
                      parseResult.ministeredFamilies,
                      parseResult.districts,
                      parseResult.groups,
                      isAuthoritative);
    }

    // Copy final districts and groups to result
    result.districts = parseResult.districts;
    result.groups = parseResult.groups;

    result.success = true;
    return result;
}
