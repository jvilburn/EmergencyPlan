#include "Document.h"
#include "DocumentChange.h"
#include "ResponseArea.h"
#include "StrictJson.h"


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


} // anonymous namespace

QJsonObject Document::toJson() const
{
    QJsonObject json;

    // Metadata (wards/stakes)
    m_metadata.writeToJson(json);

    json["sealingSet"] = m_sealingSet.toJson();

    // Core collections
    serializeHashToJson(json, "families", m_families);
    serializeHashToJson(json, "teams", m_teams);
    serializeHashToJson(json, "tags", m_tags);

    // Emergency assets
    serializeHashToJson(json, "emergencyResources", m_emergencyAssets);


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

std::optional<Document> Document::parse(const QJsonObject& json)
{
    // Every top-level field is one the writer writes.
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("stakes"),
                                  QLatin1String("wards"),
                                  QLatin1String("sealingSet"),
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
        return std::nullopt;
    }

    // The sealing set decides whom every later write seals to, so it is read
    // strictly; fromJson refuses an empty one, since a readable document is sealed
    // at least to the device reading it.
    const QJsonValue setValue = json.value("sealingSet");
    if (!setValue.isObject())
    {
        return std::nullopt;
    }

    const std::optional<SealingSet> sealingSet = SealingSet::fromJson(setValue.toObject());
    if (!sealingSet.has_value())
    {
        return std::nullopt;
    }

    Document document;
    document.m_sealingSet = sealingSet.value();

    const std::optional<DocumentMetadata> metadata = DocumentMetadata::fromJson(json);
    if (!metadata.has_value())
    {
        return std::nullopt;
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
        return std::nullopt;
    }

    if (json.contains("responseData"))
    {
        QJsonObject responseJson;
        if (!StrictJson::readObject(json, QLatin1String("responseData"), responseJson))
        {
            return std::nullopt;
        }
        document.m_emergencyResponse = EmergencyResponse::fromJson(responseJson);
        if (!document.m_emergencyResponse.has_value())
        {
            return std::nullopt;
        }
    }

    // A document read is complete: its lookups answer before any command runs.
    document.onDocumentChanged(DocumentChange::full());
    return document;
}

void Document::setEmergencyResponse(const std::optional<EmergencyResponse>& response)
{
    m_emergencyResponse = response;
}

bool Document::operator==(const Document& other) const
{
    return m_metadata == other.m_metadata
        && m_sealingSet == other.m_sealingSet
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
