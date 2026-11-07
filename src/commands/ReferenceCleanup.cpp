#include "ReferenceCleanup.h"
#include "Document.h"
#include "ItemType.h"

#include <QJsonArray>
#include <QObject>

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

/// A role that restoring may fill: it is empty, or already holds the id restored to it.
bool roleOpenTo(const std::optional<PersonId>& current, const std::optional<PersonId>& restored)
{
    return !current.has_value()
        || current == restored;
}

template<typename Id, typename T>
QJsonObject objectsToJson(const QHash<Id, T>& objects)
{
    QJsonObject json;
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it)
    {
        json[it.key().toString()] = it.value().toJson();
    }
    return json;
}

template<typename Id, typename T>
QHash<Id, T> objectsFromJson(const QJsonObject& json)
{
    QHash<Id, T> objects;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it)
    {
        T object = T::fromJson(it.value().toObject());
        objects.insert(object.id(), object);
    }
    return objects;
}

template<typename Id>
QSet<Id> idsFromJson(const QJsonValue& value)
{
    QSet<Id> ids;
    for (const QJsonValue& id : value.toArray())
    {
        ids.insert(Id::fromString(id.toString()));
    }
    return ids;
}

/// Writes the ids under key, or nothing when there are none.
template<typename Id>
void writeIds(QJsonObject& json, const QString& key, const QSet<Id>& ids)
{
    if (!ids.isEmpty())
    {
        json[key] = sortedIdArray(ids);
    }
}

/// Writes the objects under key, or nothing when there are none.
template<typename Id, typename T>
void writeObjects(QJsonObject& json, const QString& key, const QHash<Id, T>& objects)
{
    if (!objects.isEmpty())
    {
        json[key] = objectsToJson(objects);
    }
}

QString taskName(const ResponseTask& task)
{
    return QObject::tr("the %1 task: %2").arg(task.category(), task.description());
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
            if (cleanup.namesRemoved(task.assignment()))
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
            if (cleanup.namesRemoved(task.assignment()))
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
            if (namesRemoved(task.assignment()))
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
    restoreInto(document);
}

QStringList ReferenceCleanup::skippedOn(const Document& document) const
{
    Document restored = document;
    return restoreInto(restored);
}

bool ReferenceCleanup::holdsPlaces() const
{
    return !m_heldTeams.isEmpty()
        || !m_heldAssets.isEmpty()
        || !m_heldGroups.isEmpty()
        || !m_heldDistricts.isEmpty()
        || !m_heldTasks.isEmpty();
}

QStringList ReferenceCleanup::restoreInto(Document& document) const
{
    QStringList skipped;

    for (const Team& held : m_heldTeams)
    {
        std::optional<Team> current = document.findTeamById(held.id());
        if (!current.has_value())
        {
            skipped.append(QObject::tr("the %1 team").arg(held.name()));
            continue;
        }
        Team restored = *current;
        restored.setMemberIds(with(current->memberIds(), among(held.memberIds(), m_people)));
        if (namesRemoved(held.leaderId()))
        {
            if (roleOpenTo(current->leaderId(), held.leaderId()))
            {
                restored.setLeaderId(held.leaderId());
            }
            else
            {
                skipped.append(QObject::tr("leader of the %1 team").arg(current->name()));
            }
        }
        document.setTeam(restored);
    }

    for (const EmergencyAsset& held : m_heldAssets)
    {
        std::optional<EmergencyAsset> current = document.findEmergencyAssetById(held.id());
        if (!current.has_value())
        {
            skipped.append(QObject::tr("the %1 asset").arg(held.name()));
            continue;
        }
        EmergencyAsset restored = *current;
        restored.setPersonIds(with(current->personIds(), among(held.personIds(), m_people)));
        document.setEmergencyAsset(restored);
    }

    for (const MinisteringGroup& held : m_heldGroups)
    {
        std::optional<MinisteringOrg> org = orgOfGroup(document, held.id());
        if (!org.has_value())
        {
            skipped.append(QObject::tr("a ministering group"));
            continue;
        }
        MinisteringGroup current = document.groups(*org).value(held.id());
        MinisteringGroup restored = current;
        restored.setMinisterIds(with(current.ministerIds(), among(held.ministerIds(), m_people)));
        restored.setMinisteredPersonIds(with(current.ministeredPersonIds(), among(held.ministeredPersonIds(), m_people)));
        restored.setFamilyIds(with(current.familyIds(), among(held.familyIds(), m_families)));
        if (namesRemoved(held.presidencyMemberId()))
        {
            if (roleOpenTo(current.presidencyMemberId(), held.presidencyMemberId()))
            {
                restored.setPresidencyMemberId(held.presidencyMemberId());
            }
            else
            {
                skipped.append(QObject::tr("presidency member over a ministering group"));
            }
        }
        document.setGroup(*org, restored);
    }

    for (const MinisteringDistrict& held : m_heldDistricts)
    {
        std::optional<MinisteringOrg> org = orgOfDistrict(document, held.id());
        if (!org.has_value())
        {
            skipped.append(QObject::tr("the ministering district %1").arg(held.name()));
            continue;
        }
        MinisteringDistrict current = document.districts(*org).value(held.id());
        if (!roleOpenTo(current.presidencyMemberId(), held.presidencyMemberId()))
        {
            skipped.append(QObject::tr("presidency member over the ministering district %1").arg(current.name()));
            continue;
        }
        MinisteringDistrict restored = current;
        restored.setPresidencyMemberId(held.presidencyMemberId());
        document.setDistrict(*org, restored);
    }

    for (const ResponseTask& held : m_heldTasks)
    {
        const ResponseTask* current = document.emergencyResponse().has_value()
            ? document.emergencyResponse()->findTask(held.id())
            : nullptr;
        if (current == nullptr)
        {
            skipped.append(taskName(held));
            continue;
        }
        if (current->assignment().has_value() && current->assignment() != held.assignment())
        {
            skipped.append(taskName(*current));
            continue;
        }
        ResponseTask restored = *current;
        restored.setAssignment(held.assignment());
        EmergencyResponse response = *document.emergencyResponse();
        response.setTask(restored);
        document.setEmergencyResponse(response);
    }

    skipped.sort();
    return skipped;
}

bool ReferenceCleanup::namesRemoved(const std::optional<PersonId>& id) const
{
    return id.has_value()
        && m_people.contains(*id);
}

bool ReferenceCleanup::namesRemoved(const std::optional<TaskAssignment>& assignment) const
{
    if (!assignment.has_value())
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

ReferenceCleanup ReferenceCleanup::fromJson(const QJsonObject& json)
{
    ReferenceCleanup cleanup;
    cleanup.m_people = idsFromJson<PersonId>(json["people"]);
    cleanup.m_families = idsFromJson<FamilyId>(json["families"]);
    cleanup.m_teams = idsFromJson<TeamId>(json["teams"]);
    cleanup.m_heldTeams = objectsFromJson<TeamId, Team>(json["heldTeams"].toObject());
    cleanup.m_heldAssets = objectsFromJson<EmergencyAssetId, EmergencyAsset>(json["heldAssets"].toObject());
    cleanup.m_heldGroups = objectsFromJson<MinisteringGroupId, MinisteringGroup>(json["heldGroups"].toObject());
    cleanup.m_heldDistricts = objectsFromJson<MinisteringDistrictId, MinisteringDistrict>(json["heldDistricts"].toObject());
    cleanup.m_heldTasks = objectsFromJson<TaskId, ResponseTask>(json["heldTasks"].toObject());
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
