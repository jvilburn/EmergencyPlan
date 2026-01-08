#include "Document.h"

Document Document::empty()
{
    return Document();
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
    rebuildPersonToFamilyMap();
}

void Document::addFamilies(const QList<Family>& families)
{
    for (const Family& family : families)
    {
        m_families.insert(family.id(), family);
    }
    rebuildPersonToFamilyMap();
}

void Document::updateFamily(const Family& family)
{
    m_families.insert(family.id(), family);
    rebuildPersonToFamilyMap();
}

void Document::removeFamily(const QString& id)
{
    m_families.remove(id);
    rebuildPersonToFamilyMap();
}

void Document::setFamilies(const QHash<QString, Family>& families)
{
    m_families = families;
    rebuildPersonToFamilyMap();
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

void Document::removeTeam(const QString& id)
{
    m_teams.remove(id);
}

void Document::addMemberToTeam(const QString& teamId, const QString& memberId)
{
    auto it = m_teams.find(teamId);
    if (it != m_teams.end())
    {
        it->addMember(memberId);
    }
}

void Document::removeMemberFromTeam(const QString& teamId, const QString& memberId)
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

void Document::removeTag(const QString& id)
{
    m_tags.remove(id);
}

void Document::addPersonToTag(const QString& tagId, const QString& personId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->addEntity(personId);
    }
}

void Document::removePersonFromTag(const QString& tagId, const QString& personId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->removeEntity(personId);
    }
}

void Document::addFamilyToTag(const QString& tagId, const QString& familyId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->addEntity(familyId);
    }
}

void Document::removeFamilyFromTag(const QString& tagId, const QString& familyId)
{
    auto it = m_tags.find(tagId);
    if (it != m_tags.end())
    {
        it->removeEntity(familyId);
    }
}

// ============================================================================
// Resource operations
// ============================================================================

void Document::addCategory(const ResourceCategory& category)
{
    m_categories.insert(category.id(), category);
}

void Document::updateCategory(const ResourceCategory& category)
{
    m_categories.insert(category.id(), category);
}

void Document::removeCategory(const QString& id)
{
    m_categories.remove(id);
}

void Document::addResourceType(const ResourceType& resourceType)
{
    m_resourceTypes.insert(resourceType.id(), resourceType);
}

void Document::updateResourceType(const ResourceType& resourceType)
{
    m_resourceTypes.insert(resourceType.id(), resourceType);
}

void Document::removeResourceType(const QString& id)
{
    m_resourceTypes.remove(id);
}

void Document::addPersonToResourceType(const QString& resourceTypeId, const QString& personId)
{
    auto it = m_resourceTypes.find(resourceTypeId);
    if (it != m_resourceTypes.end())
    {
        it->addPerson(personId);
    }
}

void Document::removePersonFromResourceType(const QString& resourceTypeId, const QString& personId)
{
    auto it = m_resourceTypes.find(resourceTypeId);
    if (it != m_resourceTypes.end())
    {
        it->removePerson(personId);
    }
}

void Document::addFamilyToResourceType(const QString& resourceTypeId, const QString& familyId)
{
    auto it = m_resourceTypes.find(resourceTypeId);
    if (it != m_resourceTypes.end())
    {
        it->addFamily(familyId);
    }
}

void Document::removeFamilyFromResourceType(const QString& resourceTypeId, const QString& familyId)
{
    auto it = m_resourceTypes.find(resourceTypeId);
    if (it != m_resourceTypes.end())
    {
        it->removeFamily(familyId);
    }
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

void Document::removeEqDistrict(const QString& id)
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

void Document::removeEqGroup(const QString& id)
{
    m_eqGroups.remove(id);
}

void Document::setEqDistricts(const QHash<QString, MinisteringDistrict>& districts)
{
    m_eqDistricts = districts;
}

void Document::setEqGroups(const QHash<QString, MinisteringGroup>& groups)
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

void Document::removeRsDistrict(const QString& id)
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

void Document::removeRsGroup(const QString& id)
{
    m_rsGroups.remove(id);
}

void Document::setRsDistricts(const QHash<QString, MinisteringDistrict>& districts)
{
    m_rsDistricts = districts;
}

void Document::setRsGroups(const QHash<QString, MinisteringGroup>& groups)
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

void Document::cleanupPersonReferences(const QString& personId)
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
            it->setLeaderId(QString());
        }
    }

    // Remove from person-level tags
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it)
    {
        if (it->isPersonLevel() && it->entityIds().contains(personId))
        {
            it->removeEntity(personId);
        }
    }

    // Remove from resource types
    for (auto it = m_resourceTypes.begin(); it != m_resourceTypes.end(); ++it)
    {
        if (it->personIds().contains(personId))
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

std::optional<Family> Document::findFamilyById(const QString& id) const
{
    auto it = m_families.find(id);
    if (it != m_families.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<Person> Document::findPersonById(const QString& id) const
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

std::optional<Team> Document::findTeamById(const QString& id) const
{
    auto it = m_teams.find(id);
    if (it != m_teams.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<Tag> Document::findTagById(const QString& id) const
{
    auto it = m_tags.find(id);
    if (it != m_tags.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<ResourceCategory> Document::findCategoryById(const QString& id) const
{
    auto it = m_categories.find(id);
    if (it != m_categories.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<ResourceType> Document::findResourceTypeById(const QString& id) const
{
    auto it = m_resourceTypes.find(id);
    if (it != m_resourceTypes.end())
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

QString Document::familyIdForPerson(const QString& personId) const
{
    return m_personToFamily.value(personId);
}

std::optional<int> Document::maxKnownAge() const
{
    return m_maxKnownAge;
}

// ============================================================================
// JSON serialization
// ============================================================================

namespace
{

template<typename T>
void serializeHashToJson(QJsonObject& json, const QString& key, const QHash<QString, T>& hash)
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

template<typename T>
void deserializeJsonToHash(const QJsonObject& json, const QString& key, QHash<QString, T>& hash)
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

    // Resource collections
    serializeHashToJson(json, "categories", m_categories);
    serializeHashToJson(json, "resourceTypes", m_resourceTypes);

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

    // Resource collections
    deserializeJsonToHash(json, "categories", document.m_categories);
    deserializeJsonToHash(json, "resourceTypes", document.m_resourceTypes);

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

    // Build lookup cache
    document.rebuildPersonToFamilyMap();

    return document;
}

bool Document::operator==(const Document& other) const
{
    return m_metadata == other.m_metadata
        && m_families == other.m_families
        && m_teams == other.m_teams
        && m_tags == other.m_tags
        && m_categories == other.m_categories
        && m_resourceTypes == other.m_resourceTypes
        && m_eqDistricts == other.m_eqDistricts
        && m_eqGroups == other.m_eqGroups
        && m_rsDistricts == other.m_rsDistricts
        && m_rsGroups == other.m_rsGroups
        && m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
        && m_ministeringPdfDate == other.m_ministeringPdfDate;
}
