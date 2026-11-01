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
// Emergency responses
// ============================================================================

void Document::setEmergencyResponse(const EmergencyResponse& response)
{
    m_emergencyResponses.insert(response.id(), response);
}

void Document::removeEmergencyResponse(const EmergencyResponseId& id)
{
    m_emergencyResponses.remove(id);
}

std::optional<EmergencyResponse> Document::findEmergencyResponseById(const EmergencyResponseId& id) const
{
    auto it = m_emergencyResponses.constFind(id);
    if (it != m_emergencyResponses.constEnd())
    {
        return *it;
    }
    return std::nullopt;
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

    // Address coordinates
    if (!m_addressCoordinates.isEmpty())
    {
        QJsonArray array;
        for (auto it = m_addressCoordinates.constBegin(); it != m_addressCoordinates.constEnd(); ++it)
        {
            QJsonObject entry = it.value().toJson();
            entry["address"] = it.key();
            array.append(entry);
        }
        json["addressCoordinates"] = array;
    }

    // Emergency assets
    serializeHashToJson(json, "emergencyResources", m_emergencyAssets);

    // Emergency responses in progress
    serializeHashToJson(json, "emergencyResponses", m_emergencyResponses);

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

    // Address coordinates
    QJsonArray addressCoordinates = json["addressCoordinates"].toArray();
    for (const QJsonValue& value : addressCoordinates)
    {
        QJsonObject entry = value.toObject();
        document.m_addressCoordinates.insert(entry["address"].toString(), Coordinates::fromJson(entry));
    }

    // Emergency assets
    deserializeJsonToHash(json, "emergencyResources", document.m_emergencyAssets);

    // Emergency responses in progress
    deserializeJsonToHash(json, "emergencyResponses", document.m_emergencyResponses);

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
        && m_emergencyResponses == other.m_emergencyResponses
        && m_eqDistricts == other.m_eqDistricts
        && m_eqGroups == other.m_eqGroups
        && m_rsDistricts == other.m_rsDistricts
        && m_rsGroups == other.m_rsGroups
        && m_wardDirectoryPdfDate == other.m_wardDirectoryPdfDate
        && m_ministeringPdfDate == other.m_ministeringPdfDate;
}
