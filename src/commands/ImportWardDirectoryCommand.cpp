#include "ImportWardDirectoryCommand.h"
#include "Document.h"
#include "EmergencyAsset.h"

ImportWardDirectoryCommand::ImportWardDirectoryCommand(
    const QHash<FamilyId, Family>& mergedFamilies,
    const QSet<FamilyId>& removedFamilyIds,
    const QString& wardUnitNumber,
    const QString& wardName,
    std::optional<QDate> pdfDate,
    const QString& description)
    : m_newFamilies(mergedFamilies)
    , m_removedFamilyIds(removedFamilyIds)
    , m_wardUnitNumber(wardUnitNumber)
    , m_wardName(wardName)
    , m_pdfDate(pdfDate)
    , m_description(description)
{
}

void ImportWardDirectoryCommand::execute(Document& document)
{
    // Save previous families for undo
    m_previousFamilies = document.families();

    // Save removed families for undo
    for (const FamilyId& familyId : m_removedFamilyIds)
    {
        auto it = m_previousFamilies.find(familyId);
        if (it != m_previousFamilies.end())
        {
            m_removedFamilies.insert(familyId, *it);
        }
    }

    // Set new families
    document.setFamilies(m_newFamilies);

    // Cleanup references to removed families
    for (const FamilyId& familyId : m_removedFamilyIds)
    {
        if (m_removedFamilies.contains(familyId))
        {
            cleanupRemovedFamily(document, familyId, m_removedFamilies[familyId]);
        }
    }

    // Update ward directory PDF date
    if (m_pdfDate.has_value())
    {
        m_previousWardDirectoryPdfDate = document.wardDirectoryPdfDate();
        document.setWardDirectoryPdfDate(m_pdfDate);
    }

    // Handle ward creation/update
    if (!m_wardUnitNumber.isEmpty())
    {
        std::optional<Ward> existingWard = document.metadata().findWardByUnit(m_wardUnitNumber);
        m_wardExisted = existingWard.has_value();

        if (m_wardExisted)
        {
            // Save previous ward state for undo
            m_previousWard = *existingWard;
        }

        // Create or update ward
        Ward ward;
        if (m_wardExisted)
        {
            // Preserve existing ward data, just ensure it exists
            ward = m_previousWard;
        }
        else
        {
            ward.setUnitNumber(m_wardUnitNumber);
            ward.setName(m_wardName);
        }
        document.metadata().updateWard(ward);
    }
}

void ImportWardDirectoryCommand::undo(Document& document)
{
    // Restore previous families
    document.setFamilies(m_previousFamilies);

    // Restore ward directory PDF date
    if (m_pdfDate.has_value())
    {
        document.setWardDirectoryPdfDate(m_previousWardDirectoryPdfDate);
    }

    // Handle ward removal/restoration
    if (!m_wardUnitNumber.isEmpty())
    {
        if (m_wardExisted)
        {
            // Restore previous ward state
            document.metadata().updateWard(m_previousWard);
        }
        else
        {
            // Ward didn't exist before, remove it
            document.metadata().removeWard(m_wardUnitNumber);
        }
    }
}

QString ImportWardDirectoryCommand::description() const
{
    if (!m_description.isEmpty())
    {
        return m_description;
    }
    return QObject::tr("Import %1 families").arg(m_newFamilies.size());
}

DocumentChange ImportWardDirectoryCommand::documentChange() const
{
    // Ward directory import touches families + metadata, use full
    return DocumentChange::full();
}

void ImportWardDirectoryCommand::cleanupRemovedFamily(
    Document& document,
    const FamilyId& familyId,
    const Family& family)
{
    // Remove persons from teams
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

    // Remove from tags (both family and person)
    for (const auto& [tagId, tag] : document.tags().asKeyValueRange())
    {
        if (tag.hasFamily(familyId))
        {
            document.removeFamilyFromTag(tagId, familyId);
        }
        for (const Person& member : family.members())
        {
            if (tag.hasPerson(member.id()))
            {
                document.removePersonFromTag(tagId, member.id());
            }
        }
    }

    // Remove from emergency assets (person-level)
    for (const Person& member : family.members())
    {
        for (const auto& [assetId, asset] : document.emergencyAssets().asKeyValueRange())
        {
            if (asset.hasPerson(member.id()))
            {
                EmergencyAsset updated = asset;
                updated.removePerson(member.id());
                document.updateEmergencyAsset(updated);
            }
        }
    }

    // Remove from EQ ministering groups
    for (const MinisteringGroupId& groupId : document.eqGroups().keys())
    {
        MinisteringGroup group = document.eqGroups()[groupId];
        bool modified = false;

        QSet<PersonId> ministerIds = group.ministerIds();
        QSet<FamilyId> familyIds = group.familyIds();
        QSet<PersonId> ministeredPersonIds = group.ministeredPersonIds();

        if (familyIds.remove(familyId))
        {
            modified = true;
        }

        for (const Person& member : family.members())
        {
            if (ministerIds.remove(member.id()))
            {
                modified = true;
            }
            if (ministeredPersonIds.remove(member.id()))
            {
                modified = true;
            }
            if (group.presidencyMemberId() == member.id())
            {
                group.setPresidencyMemberId(std::nullopt);
                modified = true;
            }
        }

        if (modified)
        {
            group.setMinisterIds(ministerIds);
            group.setFamilyIds(familyIds);
            group.setMinisteredPersonIds(ministeredPersonIds);
            document.updateEqGroup(group);
        }
    }

    // Remove from RS ministering groups (same logic)
    for (const MinisteringGroupId& groupId : document.rsGroups().keys())
    {
        MinisteringGroup group = document.rsGroups()[groupId];
        bool modified = false;

        QSet<PersonId> ministerIds = group.ministerIds();
        QSet<FamilyId> familyIds = group.familyIds();
        QSet<PersonId> ministeredPersonIds = group.ministeredPersonIds();

        if (familyIds.remove(familyId))
        {
            modified = true;
        }

        for (const Person& member : family.members())
        {
            if (ministerIds.remove(member.id()))
            {
                modified = true;
            }
            if (ministeredPersonIds.remove(member.id()))
            {
                modified = true;
            }
            if (group.presidencyMemberId() == member.id())
            {
                group.setPresidencyMemberId(std::nullopt);
                modified = true;
            }
        }

        if (modified)
        {
            group.setMinisterIds(ministerIds);
            group.setFamilyIds(familyIds);
            group.setMinisteredPersonIds(ministeredPersonIds);
            document.updateRsGroup(group);
        }
    }

    // Remove from EQ district presidencies
    for (const MinisteringDistrictId& districtId : document.eqDistricts().keys())
    {
        MinisteringDistrict district = document.eqDistricts()[districtId];
        for (const Person& member : family.members())
        {
            if (district.presidencyMemberId() == member.id())
            {
                district.setPresidencyMemberId(std::nullopt);
                document.updateEqDistrict(district);
                break;
            }
        }
    }

    // Remove from RS district presidencies
    for (const MinisteringDistrictId& districtId : document.rsDistricts().keys())
    {
        MinisteringDistrict district = document.rsDistricts()[districtId];
        for (const Person& member : family.members())
        {
            if (district.presidencyMemberId() == member.id())
            {
                district.setPresidencyMemberId(std::nullopt);
                document.updateRsDistrict(district);
                break;
            }
        }
    }
}
