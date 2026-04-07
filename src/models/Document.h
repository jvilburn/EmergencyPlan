#pragma once

#include <QString>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>

#include "DocumentMetadata.h"
#include "Family.h"
#include "Team.h"
#include "Tag.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "ItemType.h"
#include "EmergencyAsset.h"
#include "EmergencyResponse.h"
#include "ResponseArea.h"
#include <QSet>

class Document
{
public:
    Document() = default;

    // Factory method for creating empty documents
    static Document empty();

    // Initialize default emergency assets for a new document
    void initializeDefaultAssets();

    // ========================================================================
    // Metadata (wards/stakes)
    // ========================================================================
    DocumentMetadata& metadata() { return m_metadata; }
    const DocumentMetadata& metadata() const { return m_metadata; }

    // Convenience accessors that delegate to metadata
    const QHash<QString, Ward>& wards() const { return m_metadata.wards(); }
    const QHash<QString, Stake>& stakes() const { return m_metadata.stakes(); }
    QString suggestedFilename() const { return m_metadata.suggestedFilename(); }

    // ========================================================================
    // Core collection getters
    // ========================================================================
    const QHash<FamilyId, Family>& families() const { return m_families; }
    const QHash<TeamId, Team>& teams() const { return m_teams; }
    const QHash<TagId, Tag>& tags() const { return m_tags; }

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
    void setWardDirectoryPdfDate(std::optional<QDate> date);
    void setMinisteringPdfDate(std::optional<QDate> date);

    // ========================================================================
    // Mutating operations - families
    // ========================================================================
    void addFamily(const Family& family);
    void addFamilies(const QList<Family>& families);
    void updateFamily(const Family& family);
    void removeFamily(const FamilyId& id);
    void setFamilies(const QHash<FamilyId, Family>& families);

    QList<Family> familiesInStake(const QString& stakeUnitNumber) const;
    QList<Family> familiesInWard(const QString& wardUnitNumber) const;

    // ========================================================================
    // Mutating operations - teams
    // ========================================================================
    void addTeam(const Team& team);
    void updateTeam(const Team& team);
    void removeTeam(const TeamId& id);

    void addMemberToTeam(const TeamId& teamId, const PersonId& memberId);
    void removeMemberFromTeam(const TeamId& teamId, const PersonId& memberId);

    // ========================================================================
    // Mutating operations - tags
    // ========================================================================
    void addTag(const Tag& tag);
    void updateTag(const Tag& tag);
    void removeTag(const TagId& id);

    void addPersonToTag(const TagId& tagId, const PersonId& personId);
    void removePersonFromTag(const TagId& tagId, const PersonId& personId);
    void addFamilyToTag(const TagId& tagId, const FamilyId& familyId);
    void removeFamilyFromTag(const TagId& tagId, const FamilyId& familyId);

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
    // Mutating operations - ministering
    // ========================================================================
    void addDistrict(MinisteringOrg org, const MinisteringDistrict& district);
    void updateDistrict(MinisteringOrg org, const MinisteringDistrict& district);
    void removeDistrict(MinisteringOrg org, const MinisteringDistrictId& id);
    void setDistricts(MinisteringOrg org, const QHash<MinisteringDistrictId, MinisteringDistrict>& districts);

    void addGroup(MinisteringOrg org, const MinisteringGroup& group);
    void updateGroup(MinisteringOrg org, const MinisteringGroup& group);
    void removeGroup(MinisteringOrg org, const MinisteringGroupId& id);
    void setGroups(MinisteringOrg org, const QHash<MinisteringGroupId, MinisteringGroup>& groups);

    // ========================================================================
    // Cascading cleanup
    // ========================================================================

    /// Remove a person from all references (teams, tags, assets, ministering).
    /// Does NOT remove the person from their family - caller must handle that.
    void cleanupPersonReferences(const PersonId& personId);

    // ========================================================================
    // Emergency response data (optional - only present during active emergency)
    // ========================================================================
    const std::optional<EmergencyResponse>& emergencyResponse() const { return m_emergencyResponse; }
    void setEmergencyResponse(const std::optional<EmergencyResponse>& response);

    // ========================================================================
    // Lookup helpers
    // ========================================================================
    std::optional<Family> findFamilyById(const FamilyId& id) const;
    std::optional<Person> findPersonById(const PersonId& id) const;
    std::optional<FamilyId> familyIdForPerson(const PersonId& personId) const;
    std::optional<Team> findTeamById(const TeamId& id) const;
    std::optional<Tag> findTagById(const TagId& id) const;

    /// Returns the maximum known age across all persons, or nullopt if no ages are known.
    /// Used by UI to determine available age filter options.
    std::optional<int> maxKnownAge() const;

    /// Get response areas for a person based on their emergency asset assignments (from cache)
    QSet<ResponseArea> personResponseAreas(const PersonId& personId) const;

    /// Called after each command to rebuild caches as needed based on what changed.
    /// This is more efficient than rebuilding in each command's execute/undo.
    void onDocumentChanged(const struct DocumentChange& change);

    // JSON serialization
    QJsonObject toJson() const;
    static Document fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Document& other) const;
    bool operator!=(const Document& other) const { return !(*this == other); }

private:
    void rebuildPersonToFamilyMap();
    void rebuildDecorationCaches();

    QHash<MinisteringDistrictId, MinisteringDistrict>& districtsRef(MinisteringOrg org);
    QHash<MinisteringGroupId, MinisteringGroup>& groupsRef(MinisteringOrg org);

    // Ward/Stake hierarchy and document naming
    DocumentMetadata m_metadata;

    // Core collections
    QHash<FamilyId, Family> m_families;
    QHash<TeamId, Team> m_teams;
    QHash<TagId, Tag> m_tags;

    // Lookup cache: personId -> familyId
    QHash<PersonId, FamilyId> m_personToFamily;

    // Cached max known age (computed in rebuildPersonToFamilyMap)
    std::optional<int> m_maxKnownAge;

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

    // Emergency response data (only present during active emergency)
    std::optional<EmergencyResponse> m_emergencyResponse;

    // Decoration cache: personId -> set of response areas (rebuilt when emergency assets change)
    QHash<PersonId, QSet<ResponseArea>> m_personResponseAreas;
};
