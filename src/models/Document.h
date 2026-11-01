#pragma once

#include <QString>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>

#include "Coordinates.h"
#include "DocumentMetadata.h"
#include "Family.h"
#include "Team.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "ItemType.h"
#include "EmergencyAsset.h"
#include "ResponseArea.h"
#include <QSet>

class Document
{
public:
    Document() = default;

    // Factory method for creating empty documents
    static Document empty();

    // ========================================================================
    // Metadata (wards/stakes)
    // ========================================================================
    DocumentMetadata& metadata() { return m_metadata; }
    const DocumentMetadata& metadata() const { return m_metadata; }

    // Convenience accessors that delegate to metadata
    const QHash<QString, Ward>& wards() const { return m_metadata.wards(); }
    const QHash<QString, Stake>& stakes() const { return m_metadata.stakes(); }
    QString wardName() const { return m_metadata.wardName(); }

    // ========================================================================
    // Core collection getters
    // ========================================================================
    const QHash<FamilyId, Family>& families() const { return m_families; }
    const QHash<TeamId, Team>& teams() const { return m_teams; }

    // ========================================================================
    // Ministering getters
    // ========================================================================
    const QHash<MinisteringDistrictId, MinisteringDistrict>& districts(MinisteringOrg org) const;
    const QHash<MinisteringGroupId, MinisteringGroup>& groups(MinisteringOrg org) const;

    // ========================================================================
    // Import date tracking
    // ========================================================================
    std::optional<QDate> wardDirectoryPdfDate() const { return m_wardDirectoryPdfDate; }
    std::optional<QDate> ministeringPdfDate() const { return m_ministeringPdfDate; }

    // ========================================================================
    // Mutating operations - families
    // ========================================================================
    void addFamily(const Family& family);
    void updateFamily(const Family& family);
    void removeFamily(const FamilyId& id);

    // ========================================================================
    // Mutating operations - teams
    // ========================================================================
    void addTeam(const Team& team);
    void updateTeam(const Team& team);
    void removeTeam(const TeamId& id);

    // ========================================================================
    // Address coordinates
    // ========================================================================

    /// A family's place on the map is its address's entry, keyed by Address::full().
    const QHash<QString, Coordinates>& addressCoordinates() const { return m_addressCoordinates; }

    /// The entry for address, or none when the table has none. An empty address has none.
    std::optional<Coordinates> coordinatesFor(const Address& address) const;

    /// Sets the entry for address to coordinates, or removes it when coordinates is none.
    void setAddressCoordinates(const QString& address, const std::optional<Coordinates>& coordinates);

    // ========================================================================
    // EmergencyAsset operations
    // ========================================================================
    const QHash<EmergencyAssetId, EmergencyAsset>& emergencyAssets() const { return m_emergencyAssets; }
    void addEmergencyAsset(const EmergencyAsset& asset);
    void updateEmergencyAsset(const EmergencyAsset& asset);
    void removeEmergencyAsset(const EmergencyAssetId& id);
    std::optional<EmergencyAsset> findEmergencyAssetById(const EmergencyAssetId& id) const;
    QList<EmergencyAsset> emergencyAssetsByArea(ResponseArea area) const;

    // ========================================================================
    // Lookup helpers
    // ========================================================================
    std::optional<Family> findFamilyById(const FamilyId& id) const;
    std::optional<Person> findPersonById(const PersonId& id) const;
    std::optional<FamilyId> familyIdForPerson(const PersonId& personId) const;
    std::optional<Team> findTeamById(const TeamId& id) const;

    /// Get response areas for a person based on their emergency asset assignments (from cache)
    QSet<ResponseArea> personResponseAreas(const PersonId& personId) const;

    /// Rebuilds the lookup caches the change touches.
    void onDocumentChanged(const struct DocumentChange& change);

    // JSON serialization
    QJsonObject toJson() const;
    static Document fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Document& other) const;
    bool operator!=(const Document& other) const { return !(*this == other); }

private:
    // Initialize default emergency assets for a new document
    void initializeDefaultAssets();

    void rebuildPersonToFamilyMap();
    void rebuildDecorationCaches();

    // Ward/Stake hierarchy and document naming
    DocumentMetadata m_metadata;

    // Core collections
    QHash<FamilyId, Family> m_families;
    QHash<TeamId, Team> m_teams;

    // Address text -> its place on the map
    QHash<QString, Coordinates> m_addressCoordinates;

    // Lookup cache: personId -> familyId
    QHash<PersonId, FamilyId> m_personToFamily;

    // Emergency assets
    QHash<EmergencyAssetId, EmergencyAsset> m_emergencyAssets;

    // Ministering
    QHash<MinisteringDistrictId, MinisteringDistrict> m_eqDistricts;
    QHash<MinisteringGroupId, MinisteringGroup> m_eqGroups;
    QHash<MinisteringDistrictId, MinisteringDistrict> m_rsDistricts;
    QHash<MinisteringGroupId, MinisteringGroup> m_rsGroups;

    // Import dates (for conflict resolution)
    std::optional<QDate> m_wardDirectoryPdfDate;
    std::optional<QDate> m_ministeringPdfDate;

    // Decoration cache: personId -> set of response areas (rebuilt when emergency assets change)
    QHash<PersonId, QSet<ResponseArea>> m_personResponseAreas;
};
