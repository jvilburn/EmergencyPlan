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
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("First Aid"), ResponseArea::Medical));
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("CPR Certified"), ResponseArea::Medical));
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("Nurse / Doctor"), ResponseArea::Medical));

    setEmergencyAsset(EmergencyAsset::create(QObject::tr("Ham Radio"), ResponseArea::Communications));
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("CERT Trained"), ResponseArea::Communications));

    setEmergencyAsset(EmergencyAsset::create(QObject::tr("Chainsaw"), ResponseArea::Recovery));
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("Generator"), ResponseArea::Recovery));
    setEmergencyAsset(EmergencyAsset::create(QObject::tr("4WD Vehicle"), ResponseArea::Recovery));
}

// ============================================================================
// Family operations
// ============================================================================

void Document::setFamily(const Family& family)
{
    m_families.insert(family.id(), family);
}

void Document::removeFamily(const FamilyId& id)
{
    m_families.remove(id);
}

// ============================================================================
// Team operations
// ============================================================================

void Document::setTeam(const Team& team)
{
    m_teams.insert(team.id(), team);
}

void Document::removeTeam(const TeamId& id)
{
    m_teams.remove(id);
}

// ============================================================================
// Address coordinates
// ============================================================================

std::optional<Coordinates> Document::coordinatesFor(const Address& address) const
{
    auto it = m_addressCoordinates.constFind(address.full());
    if (it != m_addressCoordinates.constEnd())
    {
        return *it;
    }
    return std::nullopt;
}

void Document::setAddressCoordinates(const QString& address, const std::optional<Coordinates>& coordinates)
{
    if (coordinates.has_value())
    {
        m_addressCoordinates.insert(address, *coordinates);
    }
    else
    {
        m_addressCoordinates.remove(address);
    }
}

// ============================================================================
// EmergencyAsset operations
// ============================================================================

void Document::setEmergencyAsset(const EmergencyAsset& asset)
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

void Document::setDistricts(MinisteringOrg org, const QHash<MinisteringDistrictId, MinisteringDistrict>& districts)
{
    if (org == MinisteringOrg::EldersQuorum)
    {
        m_eqDistricts = districts;
    }
    else
    {
        m_rsDistricts = districts;
    }
}

void Document::setGroups(MinisteringOrg org, const QHash<MinisteringGroupId, MinisteringGroup>& groups)
{
    if (org == MinisteringOrg::EldersQuorum)
    {
        m_eqGroups = groups;
    }
    else
    {
        m_rsGroups = groups;
    }
}

void Document::setDistrict(MinisteringOrg org, const MinisteringDistrict& district)
{
    if (org == MinisteringOrg::EldersQuorum)
    {
        m_eqDistricts.insert(district.id(), district);
    }
    else
    {
        m_rsDistricts.insert(district.id(), district);
    }
}

void Document::setGroup(MinisteringOrg org, const MinisteringGroup& group)
{
    if (org == MinisteringOrg::EldersQuorum)
    {
        m_eqGroups.insert(group.id(), group);
    }
    else
    {
        m_rsGroups.insert(group.id(), group);
    }
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

void Document::rebuildPersonToFamilyMap()
{
    m_personToFamily.clear();

    for (const Family& family : m_families)
    {
        for (const Person& member : family.members())
        {
            m_personToFamily.insert(member.id(), family.id());
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

    // Emergency status for map markers and lists
    if (change.action == ChangeAction::Full || change.emergencyChanged)
    {
        m_emergencyStatuses.clear();
        if (m_emergencyResponse.has_value())
        {
            m_emergencyStatuses = m_emergencyResponse->statusByFamily();
        }
    }
    else if (change.emergencyFamilyId.has_value())
    {
        updateEmergencyStatus(*change.emergencyFamilyId);
    }
}

void Document::updateEmergencyStatus(const FamilyId& familyId)
{
    EffectiveContactStatus status = m_emergencyResponse.has_value()
        ? m_emergencyResponse->statusOf(familyId)
        : EffectiveContactStatus::NotContacted;
    if (status == EffectiveContactStatus::NotContacted)
    {
        m_emergencyStatuses.remove(familyId);
    }
    else
    {
        m_emergencyStatuses.insert(familyId, status);
    }
}

EffectiveContactStatus Document::emergencyStatus(const FamilyId& familyId) const
{
    return m_emergencyStatuses.value(familyId, EffectiveContactStatus::NotContacted);
}

// ============================================================================
// JSON serialization
// ============================================================================

QJsonObject Document::toJson() const
{
    QJsonObject json;
    m_metadata.writeToJson(json);
    writeObjects(json, "families", m_families);
    writeObjects(json, "teams", m_teams);
    writeAddressCoordinates(json, "addressCoordinates", m_addressCoordinates);
    writeObjects(json, "emergencyResources", m_emergencyAssets);
    if (m_emergencyResponse.has_value())
    {
        json["emergencyResponse"] = m_emergencyResponse->toJson();
    }
    writeObjects(json, "eqDistricts", m_eqDistricts);
    writeObjects(json, "eqGroups", m_eqGroups);
    writeObjects(json, "rsDistricts", m_rsDistricts);
    writeObjects(json, "rsGroups", m_rsGroups);
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

Document Document::fromJson(JsonReader json)
{
    Document document;
    document.m_metadata.readFromJson(json);
    document.m_families = json.optionalHash<FamilyId, Family>("families");
    document.m_teams = json.optionalHash<TeamId, Team>("teams");
    document.m_addressCoordinates = readAddressCoordinates(json, "addressCoordinates");
    document.m_emergencyAssets = json.optionalHash<EmergencyAssetId, EmergencyAsset>("emergencyResources");
    std::optional<JsonReader> response = json.optionalObject("emergencyResponse");
    if (response.has_value())
    {
        document.m_emergencyResponse = EmergencyResponse::fromJson(*response);
    }
    document.m_eqDistricts = json.optionalHash<MinisteringDistrictId, MinisteringDistrict>("eqDistricts");
    document.m_eqGroups = json.optionalHash<MinisteringGroupId, MinisteringGroup>("eqGroups");
    document.m_rsDistricts = json.optionalHash<MinisteringDistrictId, MinisteringDistrict>("rsDistricts");
    document.m_rsGroups = json.optionalHash<MinisteringGroupId, MinisteringGroup>("rsGroups");
    document.m_wardDirectoryPdfDate = json.optionalDate("wardDirectoryPdfDate");
    document.m_ministeringPdfDate = json.optionalDate("ministeringPdfDate");
    json.finish();

    // Lookup caches are built by WardSession, through onDocumentChanged(), when it takes the document

    return document;
}

bool Document::operator==(const Document& other) const
{
    return m_metadata == other.m_metadata
        && m_families == other.m_families
        && m_teams == other.m_teams
        && m_addressCoordinates == other.m_addressCoordinates
        && m_emergencyAssets == other.m_emergencyAssets
        && m_emergencyResponse == other.m_emergencyResponse
        && m_eqDistricts == other.m_eqDistricts
        && m_eqGroups == other.m_eqGroups
        && m_rsDistricts == other.m_rsDistricts
        && m_rsGroups == other.m_rsGroups
        && m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
        && m_ministeringPdfDate == other.m_ministeringPdfDate
        && m_access == other.m_access;
}
