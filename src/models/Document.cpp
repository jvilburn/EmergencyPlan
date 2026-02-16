#include "Document.h"
#include "DocumentChange.h"
#include "ResponseArea.h"

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

void Document::addEqDistrict(const MinisteringDistrict& district)
{
    m_eqDistricts.insert(district.id(), district);
}

void Document::updateEqDistrict(const MinisteringDistrict& district)
{
    m_eqDistricts.insert(district.id(), district);
}

void Document::removeEqDistrict(const MinisteringDistrictId& id)
{
    m_eqDistricts.remove(id);
}

void Document::addEqGroup(const MinisteringGroup& group)
{
    m_eqGroups.insert(group.id(), group);
}

void Document::updateEqGroup(const MinisteringGroup& group)
{
    m_eqGroups.insert(group.id(), group);
}

void Document::removeEqGroup(const MinisteringGroupId& id)
{
    m_eqGroups.remove(id);
}

void Document::setEqDistricts(const QHash<MinisteringDistrictId, MinisteringDistrict>& districts)
{
    m_eqDistricts = districts;
}

void Document::setEqGroups(const QHash<MinisteringGroupId, MinisteringGroup>& groups)
{
    m_eqGroups = groups;
}

void Document::addRsDistrict(const MinisteringDistrict& district)
{
    m_rsDistricts.insert(district.id(), district);
}

void Document::updateRsDistrict(const MinisteringDistrict& district)
{
    m_rsDistricts.insert(district.id(), district);
}

void Document::removeRsDistrict(const MinisteringDistrictId& id)
{
    m_rsDistricts.remove(id);
}

void Document::addRsGroup(const MinisteringGroup& group)
{
    m_rsGroups.insert(group.id(), group);
}

void Document::updateRsGroup(const MinisteringGroup& group)
{
    m_rsGroups.insert(group.id(), group);
}

void Document::removeRsGroup(const MinisteringGroupId& id)
{
    m_rsGroups.remove(id);
}

void Document::setRsDistricts(const QHash<MinisteringDistrictId, MinisteringDistrict>& districts)
{
    m_rsDistricts = districts;
}

void Document::setRsGroups(const QHash<MinisteringGroupId, MinisteringGroup>& groups)
{
    m_rsGroups = groups;
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

    // Remove from EQ groups (ministers only - families are not person IDs)
    for (auto it = m_eqGroups.begin(); it != m_eqGroups.end(); ++it)
    {
        if (it->ministerIds().contains(personId))
        {
            it->removeMinister(personId);
        }
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }

    // Remove from RS groups (both ministers and ministered persons)
    for (auto it = m_rsGroups.begin(); it != m_rsGroups.end(); ++it)
    {
        if (it->ministerIds().contains(personId))
        {
            it->removeMinister(personId);
        }
        if (it->ministeredPersonIds().contains(personId))
        {
            it->removeMinisteredPerson(personId);
        }
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }

    // Clear presidency member ID in districts if matches
    for (auto it = m_eqDistricts.begin(); it != m_eqDistricts.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
        }
    }
    for (auto it = m_rsDistricts.begin(); it != m_rsDistricts.end(); ++it)
    {
        if (it->presidencyMemberId() == personId)
        {
            it->setPresidencyMemberId(std::nullopt);
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

template<typename K, typename T>
void deserializeJsonToHash(const QJsonObject& json, const QString& key, QHash<K, T>& hash)
{
    if (json.contains(key))
    {
        QJsonArray array = json[key].toArray();
        for (const QJsonValue& value : array)
        {
            T item = T::fromJson(value.toObject());
            hash.insert(item.id(), item);
        }
    }
}

} // anonymous namespace

QJsonObject Document::toJson() const
{
    QJsonObject json;

    // Metadata (wards/stakes)
    m_metadata.writeToJson(json);

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
        json["wardDirectoryPdfDate"] = m_wardDirectoryPdfDate->toString(Qt::ISODate);
    }
    if (m_ministeringPdfDate.has_value())
    {
        json["ministeringPdfDate"] = m_ministeringPdfDate->toString(Qt::ISODate);
    }

    return json;
}

Document Document::fromJson(const QJsonObject& json)
{
    Document document;

    // Metadata (wards/stakes)
    document.m_metadata.readFromJson(json);

    // Core collections
    deserializeJsonToHash(json, "families", document.m_families);
    deserializeJsonToHash(json, "teams", document.m_teams);
    deserializeJsonToHash(json, "tags", document.m_tags);

    // Emergency assets
    deserializeJsonToHash(json, "emergencyResources", document.m_emergencyAssets);

    // Emergency inventory (legacy)
    // Legacy skill/equipment fields ignored on load (migrated to emergency assets)

    // Ministering
    deserializeJsonToHash(json, "eqDistricts", document.m_eqDistricts);
    deserializeJsonToHash(json, "eqGroups", document.m_eqGroups);
    deserializeJsonToHash(json, "rsDistricts", document.m_rsDistricts);
    deserializeJsonToHash(json, "rsGroups", document.m_rsGroups);

    // Import dates
    if (json.contains("wardDirectoryPdfDate"))
    {
        QDate date = QDate::fromString(
            json["wardDirectoryPdfDate"].toString(), Qt::ISODate);
        if (date.isValid())
        {
            document.m_wardDirectoryPdfDate = date;
        }
    }
    if (json.contains("ministeringPdfDate"))
    {
        QDate date = QDate::fromString(
            json["ministeringPdfDate"].toString(), Qt::ISODate);
        if (date.isValid())
        {
            document.m_ministeringPdfDate = date;
        }
    }

    // Note: lookup caches are built by DocumentManager::setDocument() via onDocumentChanged()

    return document;
}

bool Document::operator==(const Document& other) const
{
    return m_metadata == other.m_metadata
        && m_families == other.m_families
        && m_teams == other.m_teams
        && m_tags == other.m_tags
        && m_emergencyAssets == other.m_emergencyAssets
        && m_eqDistricts == other.m_eqDistricts
        && m_eqGroups == other.m_eqGroups
        && m_rsDistricts == other.m_rsDistricts
        && m_rsGroups == other.m_rsGroups
        && m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
        && m_ministeringPdfDate == other.m_ministeringPdfDate;
}
