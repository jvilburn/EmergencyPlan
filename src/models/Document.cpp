#include "Document.h"
#include "DocumentChange.h"
#include "ResponseArea.h"
#include "StrictJson.h"

#include <cmath>

Document Document::empty()
{
    Document doc;
    doc.initializeDefaultAssets();
    return doc;
}

void Document::initializeDefaultAssets()
{
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("First Aid"), ResponseArea::Medical));
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("CPR Certified"), ResponseArea::Medical));
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("Nurse / Doctor"), ResponseArea::Medical));

    addEmergencyAsset(EmergencyAsset::create(QObject::tr("Ham Radio"), ResponseArea::Communications));
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("CERT Trained"), ResponseArea::Communications));

    addEmergencyAsset(EmergencyAsset::create(QObject::tr("Chainsaw"), ResponseArea::Recovery));
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("Generator"), ResponseArea::Recovery));
    addEmergencyAsset(EmergencyAsset::create(QObject::tr("4WD Vehicle"), ResponseArea::Recovery));
}

// ============================================================================
// Family queries that need access to m_families
// ============================================================================

QList<Family> Document::familiesInStake(const QString& stakeUnitNumber) const
{
    QList<Family> result;
    for (const Family& family : m_families)
    {
        for (const Person& member : family.members())
        {
            if (member.stakeUnitNumber() == stakeUnitNumber)
            {
                result.append(family);
                break;  // Only add family once
            }
        }
    }
    return result;
}

QList<Family> Document::familiesInWard(const QString& wardUnitNumber) const
{
    QList<Family> result;
    for (const Family& family : m_families)
    {
        for (const Person& member : family.members())
        {
            if (member.wardUnitNumber() == wardUnitNumber)
            {
                result.append(family);
                break;  // Only add family once
            }
        }
    }
    return result;
}

// ============================================================================
// Family operations
// ============================================================================

void Document::addFamily(const Family& family)
{
    m_families.insert(family.id(), family);
}

void Document::addFamilies(const QList<Family>& families)
{
    for (const Family& family : families)
    {
        m_families.insert(family.id(), family);
    }
}

void Document::updateFamily(const Family& family)
{
    m_families.insert(family.id(), family);
}

void Document::removeFamily(const FamilyId& id)
{
    m_families.remove(id);
}

void Document::setFamilies(const QHash<FamilyId, Family>& families)
{
    m_families = families;
}

// ============================================================================
// Team operations
// ============================================================================

void Document::addTeam(const Team& team)
{
    m_teams.insert(team.id(), team);
}

void Document::updateTeam(const Team& team)
{
    m_teams.insert(team.id(), team);
}

void Document::removeTeam(const TeamId& id)
{
    m_teams.remove(id);
}

void Document::addMemberToTeam(const TeamId& teamId, const PersonId& memberId)
{
    auto it = m_teams.find(teamId);
    if (it != m_teams.end())
    {
        it->addMember(memberId);
    }
}

void Document::removeMemberFromTeam(const TeamId& teamId, const PersonId& memberId)
{
    auto it = m_teams.find(teamId);
    if (it != m_teams.end())
    {
        it->removeMember(memberId);
    }
}

// ============================================================================
// Tag operations
// ============================================================================

void Document::addTag(const Tag& tag)
{
    m_tags.insert(tag.id(), tag);
}

void Document::updateTag(const Tag& tag)
{
    m_tags.insert(tag.id(), tag);
}

void Document::removeTag(const TagId& id)
{
    m_tags.remove(id);
}

void Document::addPersonToTag(const TagId& tagId, const PersonId& personId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->addPerson(personId);
    }
}

void Document::removePersonFromTag(const TagId& tagId, const PersonId& personId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->removePerson(personId);
    }
}

void Document::addFamilyToTag(const TagId& tagId, const FamilyId& familyId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->addFamily(familyId);
    }
}

void Document::removeFamilyFromTag(const TagId& tagId, const FamilyId& familyId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->removeFamily(familyId);
    }
}

// ============================================================================
// EmergencyAsset operations
// ============================================================================

void Document::addEmergencyAsset(const EmergencyAsset& asset)
{
    m_emergencyAssets.insert(asset.id(), asset);
}

void Document::updateEmergencyAsset(const EmergencyAsset& asset)
{
    m_emergencyAssets.insert(asset.id(), asset);
}

void Document::removeEmergencyAsset(const EmergencyAssetId& id)
{
    m_emergencyAssets.remove(id);
}

std::optional<EmergencyAsset> Document::findEmergencyAssetById(const EmergencyAssetId& id) const
{
    auto it = m_emergencyAssets.constFind(id);
    if (it != m_emergencyAssets.constEnd())
    {
        return *it;
    }
    return std::nullopt;
}

QList<EmergencyAsset> Document::emergencyAssetsByArea(ResponseArea area) const
{
    QList<EmergencyAsset> result;
    for (const EmergencyAsset& asset : m_emergencyAssets)
    {
        if (asset.responseArea() == area)
        {
            result.append(asset);
        }
    }
    return result;
}

// ============================================================================
// Ministering operations
// ============================================================================

const QHash<MinisteringDistrictId, MinisteringDistrict>& Document::districts(MinisteringOrg org) const
{
    return (org == MinisteringOrg::EldersQuorum) ? m_eqDistricts : m_rsDistricts;
}

const QHash<MinisteringGroupId, MinisteringGroup>& Document::groups(MinisteringOrg org) const
{
    return (org == MinisteringOrg::EldersQuorum) ? m_eqGroups : m_rsGroups;
}

QHash<MinisteringDistrictId, MinisteringDistrict>& Document::districtsRef(MinisteringOrg org)
{
    return (org == MinisteringOrg::EldersQuorum) ? m_eqDistricts : m_rsDistricts;
}

QHash<MinisteringGroupId, MinisteringGroup>& Document::groupsRef(MinisteringOrg org)
{
    return (org == MinisteringOrg::EldersQuorum) ? m_eqGroups : m_rsGroups;
}

void Document::addDistrict(MinisteringOrg org, const MinisteringDistrict& district)
{
    districtsRef(org).insert(district.id(), district);
}

void Document::updateDistrict(MinisteringOrg org, const MinisteringDistrict& district)
{
    districtsRef(org).insert(district.id(), district);
}

void Document::removeDistrict(MinisteringOrg org, const MinisteringDistrictId& id)
{
    districtsRef(org).remove(id);
}

void Document::setDistricts(MinisteringOrg org, const QHash<MinisteringDistrictId, MinisteringDistrict>& districts)
{
    districtsRef(org) = districts;
}

void Document::addGroup(MinisteringOrg org, const MinisteringGroup& group)
{
    groupsRef(org).insert(group.id(), group);
}

void Document::updateGroup(MinisteringOrg org, const MinisteringGroup& group)
{
    groupsRef(org).insert(group.id(), group);
}

void Document::removeGroup(MinisteringOrg org, const MinisteringGroupId& id)
{
    groupsRef(org).remove(id);
}

void Document::setGroups(MinisteringOrg org, const QHash<MinisteringGroupId, MinisteringGroup>& groups)
{
    groupsRef(org) = groups;
}

void Document::setWardDirectoryPdfDate(std::optional<QDate> date)
{
    m_wardDirectoryPdfDate = date;
}

void Document::setMinisteringPdfDate(std::optional<QDate> date)
{
    m_ministeringPdfDate = date;
}

// ============================================================================
// Cascading cleanup
// ============================================================================

void Document::cleanupPersonReferences(const PersonId& personId)
{
    // Remove from teams (both as member and leader)
    for (auto it = m_teams.begin(); it != m_teams.end(); ++it)
    {
        if (it->memberIds().contains(personId))
        {
            it->removeMember(personId);
        }
        if (it->leaderId() == personId)
        {
            it->setLeaderId(std::nullopt);
        }
    }

    // Remove from person-level tags
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it)
    {
        if (it->isPersonLevel() && it->hasPerson(personId))
        {
            it->removePerson(personId);
        }
    }

    // Remove from emergency assets
    for (auto it = m_emergencyAssets.begin(); it != m_emergencyAssets.end(); ++it)
    {
        if (it->hasPerson(personId))
        {
            it->removePerson(personId);
        }
    }

    // Remove from ministering groups and districts
    for (MinisteringOrg org : {MinisteringOrg::EldersQuorum, MinisteringOrg::ReliefSociety})
    {
        for (auto it = groupsRef(org).begin(); it != groupsRef(org).end(); ++it)
        {
            if (it->ministerIds().contains(personId))
            {
                it->removeMinister(personId);
            }
            // RS groups also track ministered persons (individual sisters)
            if (org == MinisteringOrg::ReliefSociety
                && it->ministeredPersonIds().contains(personId))
            {
                it->removeMinisteredPerson(personId);
            }
            if (it->presidencyMemberId() == personId)
            {
                it->setPresidencyMemberId(std::nullopt);
            }
        }

        for (auto it = districtsRef(org).begin(); it != districtsRef(org).end(); ++it)
        {
            if (it->presidencyMemberId() == personId)
            {
                it->setPresidencyMemberId(std::nullopt);
            }
        }
    }

    // Clear person-to-family cache entry
    m_personToFamily.remove(personId);
}

// ============================================================================
// Lookup helpers
// ============================================================================

std::optional<Family> Document::findFamilyById(const FamilyId& id) const
{
    auto it = m_families.find(id);
    if (it != m_families.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<Person> Document::findPersonById(const PersonId& id) const
{
    for (const Family& family : m_families)
    {
        for (const Person& member : family.members())
        {
            if (member.id() == id)
            {
                return member;
            }
        }
    }
    return std::nullopt;
}

std::optional<Team> Document::findTeamById(const TeamId& id) const
{
    auto it = m_teams.find(id);
    if (it != m_teams.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<Tag> Document::findTagById(const TagId& id) const
{
    auto it = m_tags.find(id);
    if (it != m_tags.end())
    {
        return *it;
    }
    return std::nullopt;
}

void Document::rebuildPersonToFamilyMap()
{
    m_personToFamily.clear();
    m_maxKnownAge = std::nullopt;

    for (const Family& family : m_families)
    {
        for (const Person& member : family.members())
        {
            m_personToFamily.insert(member.id(), family.id());

            std::optional<int> age = member.age();
            if (age.has_value())
            {
                if (!m_maxKnownAge.has_value() || age.value() > m_maxKnownAge.value())
                {
                    m_maxKnownAge = age.value();
                }
            }
        }
    }
}

std::optional<FamilyId> Document::familyIdForPerson(const PersonId& personId) const
{
    auto it = m_personToFamily.find(personId);
    if (it != m_personToFamily.end())
    {
        return it.value();
    }
    return std::nullopt;
}

std::optional<int> Document::maxKnownAge() const
{
    return m_maxKnownAge;
}

// ============================================================================
// Decoration caches
// ============================================================================

void Document::rebuildDecorationCaches()
{
    m_personResponseAreas.clear();
    for (const EmergencyAsset& asset : m_emergencyAssets)
    {
        ResponseArea area = asset.responseArea();
        if (area != ResponseArea::None)
        {
            for (const PersonId& personId : asset.personIds())
            {
                m_personResponseAreas[personId].insert(area);
            }
        }
    }
}

QSet<ResponseArea> Document::personResponseAreas(const PersonId& personId) const
{
    return m_personResponseAreas.value(personId);
}

void Document::onDocumentChanged(const DocumentChange& change)
{
    // Person-to-family lookup cache
    if (change.action == ChangeAction::Full || change.familyId.has_value())
    {
        rebuildPersonToFamilyMap();
    }

    // Decoration caches for map markers
    if (change.action == ChangeAction::Full || change.assetId.has_value())
    {
        rebuildDecorationCaches();
    }
}

// ============================================================================
// JSON serialization
// ============================================================================

namespace
{

template<typename K, typename T>
void serializeHashToJson(QJsonObject& json, const QString& key, const QHash<K, T>& hash)
{
    if (!hash.isEmpty())
    {
        QJsonArray array;
        for (const T& item : hash)
        {
            array.append(item.toJson());
        }
        json[key] = array;
    }
}

/// A collection written only when not empty, each element read strictly and each
/// id appearing once: a repeated id would otherwise overwrite its twin unseen.
template<typename K, typename T>
bool readCollection(const QJsonObject& json, QLatin1String key, QHash<K, T>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    QList<QJsonObject> objects;
    if (!StrictJson::readObjectArray(json, key, objects) || objects.isEmpty())
    {
        return false;
    }

    QHash<K, T> items;
    for (const QJsonObject& object : objects)
    {
        const std::optional<T> item = T::fromJson(object);
        if (!item.has_value() || items.contains(item.value().id()))
        {
            return false;
        }
        items.insert(item.value().id(), item.value());
    }

    out = items;
    return true;
}

bool readOptionalDate(const QJsonObject& json, QLatin1String key, std::optional<QDate>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    QDate date;
    if (!StrictJson::readDate(json, key, date))
    {
        return false;
    }
    out = date;
    return true;
}

template<typename Id>
bool allKnown(const QSet<Id>& ids, const QSet<Id>& known)
{
    for (const Id& id : ids)
    {
        if (!known.contains(id))
        {
            return false;
        }
    }
    return true;
}

/// Every district names only groups of its own organisation.
bool districtsNameKnownGroups(const QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
                              const QHash<MinisteringGroupId, MinisteringGroup>& groups)
{
    const QList<MinisteringGroupId> groupIds = groups.keys();
    const QSet<MinisteringGroupId> known(groupIds.begin(), groupIds.end());
    for (const MinisteringDistrict& district : districts)
    {
        if (!allKnown(district.groupIds(), known))
        {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

QJsonObject Document::toJson() const
{
    QJsonObject json;

    json["payloadVersion"] = CurrentPayloadVersion;

    // Metadata (wards/stakes)
    m_metadata.writeToJson(json);

    // Access roster
    json["accessRoster"] = m_accessRoster.toJson();

    // Core collections
    serializeHashToJson(json, "families", m_families);
    serializeHashToJson(json, "teams", m_teams);
    serializeHashToJson(json, "tags", m_tags);

    // Emergency assets
    serializeHashToJson(json, "emergencyResources", m_emergencyAssets);

    // Emergency inventory (legacy)
    // Legacy skill/equipment fields omitted (migrated to emergency assets)

    // Ministering
    serializeHashToJson(json, "eqDistricts", m_eqDistricts);
    serializeHashToJson(json, "eqGroups", m_eqGroups);
    serializeHashToJson(json, "rsDistricts", m_rsDistricts);
    serializeHashToJson(json, "rsGroups", m_rsGroups);

    // Import dates
    if (m_wardDirectoryPdfDate.has_value())
    {
        json["wardDirectoryPdfDate"] = StrictJson::writeDate(m_wardDirectoryPdfDate.value());
    }
    if (m_ministeringPdfDate.has_value())
    {
        json["ministeringPdfDate"] = StrictJson::writeDate(m_ministeringPdfDate.value());
    }

    // Emergency response data
    if (m_emergencyResponse.has_value())
    {
        json["responseData"] = m_emergencyResponse->toJson();
    }

    return json;
}

DocumentParse Document::parse(const QJsonObject& json, Document& out)
{
    // A payload version this build does not write is one it must not guess at.
    // An absent version is the same refusal: the writer is claiming nothing.
    const QJsonValue version = json.value("payloadVersion");
    if (!version.isDouble())
    {
        return DocumentParse::Malformed;
    }

    // Judged as the double JSON carries, so a version beyond int range still
    // reads as later rather than wrapping or collapsing to zero.
    const double payloadVersion = version.toDouble();
    if (!std::isfinite(payloadVersion) || std::floor(payloadVersion) != payloadVersion)
    {
        return DocumentParse::Malformed;
    }

    if (payloadVersion > CurrentPayloadVersion)
    {
        return DocumentParse::NeedsNewerVersion;
    }

    if (payloadVersion < 1)
    {
        return DocumentParse::Malformed;
    }

    // In a version this build knows, every top-level field is one it writes.
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("payloadVersion"),
                                  QLatin1String("stakes"),
                                  QLatin1String("wards"),
                                  QLatin1String("accessRoster"),
                                  QLatin1String("families"),
                                  QLatin1String("teams"),
                                  QLatin1String("tags"),
                                  QLatin1String("emergencyResources"),
                                  QLatin1String("eqDistricts"),
                                  QLatin1String("eqGroups"),
                                  QLatin1String("rsDistricts"),
                                  QLatin1String("rsGroups"),
                                  QLatin1String("wardDirectoryPdfDate"),
                                  QLatin1String("ministeringPdfDate"),
                                  QLatin1String("responseData")}))
    {
        return DocumentParse::Malformed;
    }

    // The roster decides who may open the document, so it is read strictly: a
    // roster understood in part would name fewer people than the document
    // actually grants.
    const QJsonValue rosterValue = json.value("accessRoster");
    if (!rosterValue.isObject())
    {
        return DocumentParse::Malformed;
    }

    // Every readable document lists at least the person reading it. The last
    // person's leave writes an empty roster, but nobody can decrypt that payload,
    // so an empty roster that reaches this reader is damage.
    const std::optional<AccessRoster> roster = AccessRoster::fromJson(rosterValue.toObject());
    if (!roster.has_value() || roster.value().isEmpty())
    {
        return DocumentParse::Malformed;
    }

    Document document;
    document.m_accessRoster = roster.value();

    const std::optional<DocumentMetadata> metadata = DocumentMetadata::fromJson(json);
    if (!metadata.has_value())
    {
        return DocumentParse::Malformed;
    }
    document.m_metadata = metadata.value();

    if (!readCollection(json, QLatin1String("families"), document.m_families)
        || !readCollection(json, QLatin1String("teams"), document.m_teams)
        || !readCollection(json, QLatin1String("tags"), document.m_tags)
        || !readCollection(json, QLatin1String("emergencyResources"), document.m_emergencyAssets)
        || !readCollection(json, QLatin1String("eqDistricts"), document.m_eqDistricts)
        || !readCollection(json, QLatin1String("eqGroups"), document.m_eqGroups)
        || !readCollection(json, QLatin1String("rsDistricts"), document.m_rsDistricts)
        || !readCollection(json, QLatin1String("rsGroups"), document.m_rsGroups)
        || !readOptionalDate(json, QLatin1String("wardDirectoryPdfDate"), document.m_wardDirectoryPdfDate)
        || !readOptionalDate(json, QLatin1String("ministeringPdfDate"), document.m_ministeringPdfDate))
    {
        return DocumentParse::Malformed;
    }

    if (json.contains("responseData"))
    {
        QJsonObject responseJson;
        if (!StrictJson::readObject(json, QLatin1String("responseData"), responseJson))
        {
            return DocumentParse::Malformed;
        }
        document.m_emergencyResponse = EmergencyResponse::fromJson(responseJson);
        if (!document.m_emergencyResponse.has_value())
        {
            return DocumentParse::Malformed;
        }
    }

    // A reference to someone or something not in the document is a dangling id
    // the UI would have to guess about.
    QSet<PersonId> people;
    QSet<FamilyId> families;
    for (const Family& family : document.m_families)
    {
        families.insert(family.id());
        for (const Person& member : family.members())
        {
            // One person in one family: a person id is how teams and tags find them.
            if (people.contains(member.id()))
            {
                return DocumentParse::Malformed;
            }
            people.insert(member.id());
        }
    }

    for (const Tag& tag : document.m_tags)
    {
        if (!allKnown(tag.personIds(), people) || !allKnown(tag.familyIds(), families))
        {
            return DocumentParse::Malformed;
        }
    }

    for (const Team& team : document.m_teams)
    {
        if (!allKnown(team.memberIds(), people)
            || (team.leaderId().has_value() && !people.contains(team.leaderId().value())))
        {
            return DocumentParse::Malformed;
        }
    }

    if (!districtsNameKnownGroups(document.m_eqDistricts, document.m_eqGroups)
        || !districtsNameKnownGroups(document.m_rsDistricts, document.m_rsGroups))
    {
        return DocumentParse::Malformed;
    }

    out = document;
    return DocumentParse::Ok;
}

void Document::setEmergencyResponse(const std::optional<EmergencyResponse>& response)
{
    m_emergencyResponse = response;
}

bool Document::operator==(const Document& other) const
{
    return m_metadata == other.m_metadata
        && m_accessRoster == other.m_accessRoster
        && m_families == other.m_families
        && m_teams == other.m_teams
        && m_tags == other.m_tags
        && m_emergencyAssets == other.m_emergencyAssets
        && m_eqDistricts == other.m_eqDistricts
        && m_eqGroups == other.m_eqGroups
        && m_rsDistricts == other.m_rsDistricts
        && m_rsGroups == other.m_rsGroups
        && m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
        && m_ministeringPdfDate == other.m_ministeringPdfDate
        && m_emergencyResponse == other.m_emergencyResponse;
}
