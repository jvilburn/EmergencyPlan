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
#include "EmergencyAsset.h"
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
    const QHash<QString, Family>& families() const { return m_families; }
    const QHash<QString, Team>& teams() const { return m_teams; }
    const QHash<QString, Tag>& tags() const { return m_tags; }

    // ========================================================================
    // Ministering getters
    // ========================================================================
    const QHash<QString, MinisteringDistrict>& eqDistricts() const { return m_eqDistricts; }
    const QHash<QString, MinisteringGroup>& eqGroups() const { return m_eqGroups; }
    const QHash<QString, MinisteringDistrict>& rsDistricts() const { return m_rsDistricts; }
    const QHash<QString, MinisteringGroup>& rsGroups() const { return m_rsGroups; }

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
    void removeFamily(const QString& id);
    void setFamilies(const QHash<QString, Family>& families);

    QList<Family> familiesInStake(const QString& stakeUnitNumber) const;
    QList<Family> familiesInWard(const QString& wardUnitNumber) const;

    // ========================================================================
    // Mutating operations - teams
    // ========================================================================
    void addTeam(const Team& team);
    void updateTeam(const Team& team);
    void removeTeam(const QString& id);

    void addMemberToTeam(const QString& teamId, const QString& memberId);
    void removeMemberFromTeam(const QString& teamId, const QString& memberId);

    // ========================================================================
    // Mutating operations - tags
    // ========================================================================
    void addTag(const Tag& tag);
    void updateTag(const Tag& tag);
    void removeTag(const QString& id);

    void addPersonToTag(const QString& tagId, const QString& personId);
    void removePersonFromTag(const QString& tagId, const QString& personId);
    void addFamilyToTag(const QString& tagId, const QString& familyId);
    void removeFamilyFromTag(const QString& tagId, const QString& familyId);

    // ========================================================================
    // EmergencyAsset operations
    // ========================================================================
    const QHash<QString, EmergencyAsset>& emergencyAssets() const { return m_emergencyAssets; }
    void addEmergencyAsset(const EmergencyAsset& asset);
    void updateEmergencyAsset(const EmergencyAsset& asset);
    void removeEmergencyAsset(const QString& id);
    std::optional<EmergencyAsset> findEmergencyAssetById(const QString& id) const;
    QList<EmergencyAsset> emergencyAssetsByArea(ResponseArea area) const;

    // ========================================================================
    // Mutating operations - ministering
    // ========================================================================
    void addEqDistrict(const MinisteringDistrict& district);
    void updateEqDistrict(const MinisteringDistrict& district);
    void removeEqDistrict(const QString& id);

    void addEqGroup(const MinisteringGroup& group);
    void updateEqGroup(const MinisteringGroup& group);
    void removeEqGroup(const QString& id);

    void setEqDistricts(const QHash<QString, MinisteringDistrict>& districts);
    void setEqGroups(const QHash<QString, MinisteringGroup>& groups);

    void addRsDistrict(const MinisteringDistrict& district);
    void updateRsDistrict(const MinisteringDistrict& district);
    void removeRsDistrict(const QString& id);

    void addRsGroup(const MinisteringGroup& group);
    void updateRsGroup(const MinisteringGroup& group);
    void removeRsGroup(const QString& id);

    void setRsDistricts(const QHash<QString, MinisteringDistrict>& districts);
    void setRsGroups(const QHash<QString, MinisteringGroup>& groups);

    // ========================================================================
    // Cascading cleanup
    // ========================================================================

    /// Remove a person from all references (teams, tags, assets, ministering).
    /// Does NOT remove the person from their family - caller must handle that.
    void cleanupPersonReferences(const QString& personId);

    // ========================================================================
    // Lookup helpers
    // ========================================================================
    std::optional<Family> findFamilyById(const QString& id) const;
    std::optional<Person> findPersonById(const QString& id) const;
    QString familyIdForPerson(const QString& personId) const;
    std::optional<Team> findTeamById(const QString& id) const;
    std::optional<Tag> findTagById(const QString& id) const;

    /// Returns the maximum known age across all persons, or nullopt if no ages are known.
    /// Used by UI to determine available age filter options.
    std::optional<int> maxKnownAge() const;

    /// Get response areas for a person based on their emergency asset assignments (from cache)
    QSet<ResponseArea> personResponseAreas(const QString& personId) const;

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

    // Ward/Stake hierarchy and document naming
    DocumentMetadata m_metadata;

    // Core collections
    QHash<QString, Family> m_families;
    QHash<QString, Team> m_teams;
    QHash<QString, Tag> m_tags;

    // Lookup cache: personId -> familyId
    QHash<QString, QString> m_personToFamily;

    // Cached max known age (computed in rebuildPersonToFamilyMap)
    std::optional<int> m_maxKnownAge;

    // Emergency assets
    QHash<QString, EmergencyAsset> m_emergencyAssets;

    // Ministering
    QHash<QString, MinisteringDistrict> m_eqDistricts;
    QHash<QString, MinisteringGroup> m_eqGroups;
    QHash<QString, MinisteringDistrict> m_rsDistricts;
    QHash<QString, MinisteringGroup> m_rsGroups;

    // Import dates (for conflict resolution)
    std::optional<QDate> m_wardDirectoryPdfDate;
    std::optional<QDate> m_ministeringPdfDate;

    // Decoration cache: personId -> set of response areas (rebuilt when emergency assets change)
    QHash<QString, QSet<ResponseArea>> m_personResponseAreas;
};
