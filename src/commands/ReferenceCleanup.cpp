#include "ReferenceCleanup.h"
#include "Document.h"
#include "ItemType.h"

#include <QJsonArray>

namespace
{

const MinisteringOrg Orgs[] = {MinisteringOrg::EldersQuorum, MinisteringOrg::ReliefSociety};

/// The ids in ids that removed holds.
template<typename Id>
QSet<Id> among(const QSet<Id>& ids, const QSet<Id>& removed)
{
    QSet<Id> found = ids;
    found.intersect(removed);
    return found;
}

/// ids without the ones removed holds.
template<typename Id>
QSet<Id> without(const QSet<Id>& ids, const QSet<Id>& removed)
{
    QSet<Id> kept = ids;
    kept.subtract(removed);
    return kept;
}

/// ids with the ones restored holds.
template<typename Id>
QSet<Id> with(const QSet<Id>& ids, const QSet<Id>& restored)
{
    QSet<Id> all = ids;
    all.unite(restored);
    return all;
}

/// The organization whose groups hold one with that id, or none.
std::optional<MinisteringOrg> orgOfGroup(const Document& document, const MinisteringGroupId& id)
{
    for (MinisteringOrg org : Orgs)
    {
        if (document.groups(org).contains(id))
        {
            return org;
        }
    }
    return std::nullopt;
}

/// The organization whose districts hold one with that id, or none.
std::optional<MinisteringOrg> orgOfDistrict(const Document& document, const MinisteringDistrictId& id)
{
    for (MinisteringOrg org : Orgs)
    {
        if (document.districts(org).contains(id))
        {
            return org;
        }
    }
    return std::nullopt;
}

/// A role as restoring leaves it: held, when the role is empty and held names one of
/// present; otherwise as it is.
std::optional<PersonId> restoredRole(const std::optional<PersonId>& current,
                                     const std::optional<PersonId>& held,
                                     const QSet<PersonId>& present)
{
    if (!current.has_value() && held.has_value() && present.contains(*held))
    {
        return held;
    }
    return current;
}

}  // namespace

ReferenceCleanup ReferenceCleanup::of(const Document& document, const QSet<PersonId>& people, const QSet<FamilyId>& families)
{
    ReferenceCleanup cleanup;
    cleanup.m_people = people;
    cleanup.m_families = families;

    for (const Team& team : document.teams())
    {
        if (team.memberIds().intersects(people) || cleanup.namesRemoved(team.leaderId()))
        {
            cleanup.m_heldTeams.insert(team.id(), team);
        }
    }
    for (const EmergencyAsset& asset : document.emergencyAssets())
    {
        if (asset.personIds().intersects(people))
        {
            cleanup.m_heldAssets.insert(asset.id(), asset);
        }
    }
    for (MinisteringOrg org : Orgs)
    {
        for (const MinisteringGroup& group : document.groups(org))
        {
            if (group.ministerIds().intersects(people)
                || group.ministeredPersonIds().intersects(people)
                || group.familyIds().intersects(families)
                || cleanup.namesRemoved(group.presidencyMemberId()))
            {
                cleanup.m_heldGroups.insert(group.id(), group);
            }
        }
        for (const MinisteringDistrict& district : document.districts(org))
        {
            if (cleanup.namesRemoved(district.presidencyMemberId()))
            {
                cleanup.m_heldDistricts.insert(district.id(), district);
            }
        }
    }
    if (document.emergencyResponse().has_value())
    {
        for (const ResponseTask& task : document.emergencyResponse()->tasks())
        {
            if (cleanup.unassigns(task))
            {
                cleanup.m_heldTasks.insert(task.id(), task);
            }
        }
    }
    return cleanup;
}

ReferenceCleanup ReferenceCleanup::ofTeam(const Document& document, const TeamId& team)
{
    ReferenceCleanup cleanup;
    cleanup.m_teams.insert(team);
    if (document.emergencyResponse().has_value())
    {
        for (const ResponseTask& task : document.emergencyResponse()->tasks())
        {
            if (cleanup.unassigns(task))
            {
                cleanup.m_heldTasks.insert(task.id(), task);
            }
        }
    }
    return cleanup;
}

void ReferenceCleanup::apply(Document& document) const
{
    const QHash<TeamId, Team> teams = document.teams();
    for (const Team& team : teams)
    {
        Team cleaned = team;
        if (namesRemoved(team.leaderId()))
        {
            cleaned.setLeaderId(std::nullopt);
        }
        cleaned.setMemberIds(without(team.memberIds(), m_people));
        if (cleaned != team)
        {
            document.setTeam(cleaned);
        }
    }

    const QHash<EmergencyAssetId, EmergencyAsset> assets = document.emergencyAssets();
    for (const EmergencyAsset& asset : assets)
    {
        EmergencyAsset cleaned = asset;
        cleaned.setPersonIds(without(asset.personIds(), m_people));
        if (cleaned != asset)
        {
            document.setEmergencyAsset(cleaned);
        }
    }

    for (MinisteringOrg org : Orgs)
    {
        const QHash<MinisteringGroupId, MinisteringGroup> groups = document.groups(org);
        for (const MinisteringGroup& group : groups)
        {
            MinisteringGroup cleaned = group;
            cleaned.setMinisterIds(without(group.ministerIds(), m_people));
            cleaned.setMinisteredPersonIds(without(group.ministeredPersonIds(), m_people));
            cleaned.setFamilyIds(without(group.familyIds(), m_families));
            if (namesRemoved(group.presidencyMemberId()))
            {
                cleaned.setPresidencyMemberId(std::nullopt);
            }
            if (cleaned != group)
            {
                document.setGroup(org, cleaned);
            }
        }

        const QHash<MinisteringDistrictId, MinisteringDistrict> districts = document.districts(org);
        for (const MinisteringDistrict& district : districts)
        {
            if (namesRemoved(district.presidencyMemberId()))
            {
                MinisteringDistrict cleaned = district;
                cleaned.setPresidencyMemberId(std::nullopt);
                document.setDistrict(org, cleaned);
            }
        }
    }

    if (document.emergencyResponse().has_value())
    {
        EmergencyResponse response = *document.emergencyResponse();
        bool changed = false;
        for (const ResponseTask& task : document.emergencyResponse()->tasks())
        {
            if (unassigns(task))
            {
                ResponseTask unassigned = task;
                unassigned.setAssignment(std::nullopt);
                response.setTask(unassigned);
                changed = true;
            }
        }
        if (changed)
        {
            document.setEmergencyResponse(response);
        }
    }
}

void ReferenceCleanup::restore(Document& document) const
{
    QSet<PersonId> present;
    for (const PersonId& id : m_people)
    {
        if (document.findPersonById(id).has_value())
        {
            present.insert(id);
        }
    }

    for (const Team& held : m_heldTeams)
    {
        std::optional<Team> current = document.findTeamById(held.id());
        if (!current.has_value())
        {
            continue;
        }
        Team restored = *current;
        restored.setMemberIds(with(current->memberIds(), among(held.memberIds(), present)));
        restored.setLeaderId(restoredRole(current->leaderId(), held.leaderId(), present));
        document.setTeam(restored);
    }

    for (const EmergencyAsset& held : m_heldAssets)
    {
        std::optional<EmergencyAsset> current = document.findEmergencyAssetById(held.id());
        if (!current.has_value())
        {
            continue;
        }
        EmergencyAsset restored = *current;
        restored.setPersonIds(with(current->personIds(), among(held.personIds(), present)));
        document.setEmergencyAsset(restored);
    }

    for (const MinisteringGroup& held : m_heldGroups)
    {
        std::optional<MinisteringOrg> org = orgOfGroup(document, held.id());
        if (!org.has_value())
        {
            continue;
        }
        MinisteringGroup current = document.groups(*org).value(held.id());
        MinisteringGroup restored = current;
        restored.setMinisterIds(with(current.ministerIds(), among(held.ministerIds(), present)));
        restored.setMinisteredPersonIds(with(current.ministeredPersonIds(), among(held.ministeredPersonIds(), present)));
        restored.setFamilyIds(with(current.familyIds(), among(held.familyIds(), m_families)));
        restored.setPresidencyMemberId(restoredRole(current.presidencyMemberId(), held.presidencyMemberId(), present));
        document.setGroup(*org, restored);
    }

    for (const MinisteringDistrict& held : m_heldDistricts)
    {
        std::optional<MinisteringOrg> org = orgOfDistrict(document, held.id());
        if (!org.has_value())
        {
            continue;
        }
        MinisteringDistrict restored = document.districts(*org).value(held.id());
        restored.setPresidencyMemberId(restoredRole(restored.presidencyMemberId(), held.presidencyMemberId(), present));
        document.setDistrict(*org, restored);
    }

    for (const ResponseTask& held : m_heldTasks)
    {
        const ResponseTask* current = document.emergencyResponse().has_value()
            ? document.emergencyResponse()->findTask(held.id())
            : nullptr;
        std::optional<PersonId> person = held.assignment()->personId();
        if (current == nullptr
            || current->assignment().has_value()
            || (person.has_value() && !present.contains(*person)))
        {
            continue;
        }
        ResponseTask restored = *current;
        restored.setAssignment(held.assignment());
        EmergencyResponse response = *document.emergencyResponse();
        response.setTask(restored);
        document.setEmergencyResponse(response);
    }
}

bool ReferenceCleanup::holdsPlaces() const
{
    return !m_heldTeams.isEmpty()
        || !m_heldAssets.isEmpty()
        || !m_heldGroups.isEmpty()
        || !m_heldDistricts.isEmpty()
        || !m_heldTasks.isEmpty();
}

bool ReferenceCleanup::namesRemoved(const std::optional<PersonId>& id) const
{
    return id.has_value()
        && m_people.contains(*id);
}

bool ReferenceCleanup::unassigns(const ResponseTask& task) const
{
    const std::optional<TaskAssignment>& assignment = task.assignment();
    if (task.isResolved() || !assignment.has_value())
    {
        return false;
    }
    return namesRemoved(assignment->personId())
        || (assignment->teamId().has_value() && m_teams.contains(*assignment->teamId()));
}

QJsonObject ReferenceCleanup::toJson() const
{
    QJsonObject json;
    writeIds(json, "people", m_people);
    writeIds(json, "families", m_families);
    writeIds(json, "teams", m_teams);
    writeObjects(json, "heldTeams", m_heldTeams);
    writeObjects(json, "heldAssets", m_heldAssets);
    writeObjects(json, "heldGroups", m_heldGroups);
    writeObjects(json, "heldDistricts", m_heldDistricts);
    writeObjects(json, "heldTasks", m_heldTasks);
    return json;
}

ReferenceCleanup ReferenceCleanup::fromJson(JsonReader json)
{
    ReferenceCleanup cleanup;
    cleanup.m_people = json.optionalIds<PersonId>("people");
    cleanup.m_families = json.optionalIds<FamilyId>("families");
    cleanup.m_teams = json.optionalIds<TeamId>("teams");
    cleanup.m_heldTeams = json.optionalHash<TeamId, Team>("heldTeams");
    cleanup.m_heldAssets = json.optionalHash<EmergencyAssetId, EmergencyAsset>("heldAssets");
    cleanup.m_heldGroups = json.optionalHash<MinisteringGroupId, MinisteringGroup>("heldGroups");
    cleanup.m_heldDistricts = json.optionalHash<MinisteringDistrictId, MinisteringDistrict>("heldDistricts");
    cleanup.m_heldTasks = json.optionalHash<TaskId, ResponseTask>("heldTasks");
    json.finish();
    return cleanup;
}

bool ReferenceCleanup::operator==(const ReferenceCleanup& other) const
{
    return m_people == other.m_people
        && m_families == other.m_families
        && m_teams == other.m_teams
        && m_heldTeams == other.m_heldTeams
        && m_heldAssets == other.m_heldAssets
        && m_heldGroups == other.m_heldGroups
        && m_heldDistricts == other.m_heldDistricts
        && m_heldTasks == other.m_heldTasks;
}

void writeCleanup(QJsonObject& json, const QString& key, const ReferenceCleanup& cleanup)
{
    QJsonObject written = cleanup.toJson();
    if (!written.isEmpty())
    {
        json[key] = written;
    }
}

ReferenceCleanup readCleanup(JsonReader& json, const QString& key)
{
    std::optional<JsonReader> cleanup = json.optionalObject(key);
    if (!cleanup.has_value())
    {
        return ReferenceCleanup();
    }
    return ReferenceCleanup::fromJson(*cleanup);
}
