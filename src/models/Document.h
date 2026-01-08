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
#include "ResourceCategory.h"
#include "ResourceType.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

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
    QString suggestedFilename() const { return m_metadata.suggestedFilename(); }

    // ========================================================================
    // Core collection getters
    // ========================================================================
    const QHash<QString, Family>& families() const { return m_families; }
    const QHash<QString, Team>& teams() const { return m_teams; }
    const QHash<QString, Tag>& tags() const { return m_tags; }

    // ========================================================================
    // Resource collection getters
    // ========================================================================
    const QHash<QString, ResourceCategory>& categories() const { return m_categories; }
    const QHash<QString, ResourceType>& resourceTypes() const { return m_resourceTypes; }

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
    // Mutating operations - resource collections
    // ========================================================================
    void addCategory(const ResourceCategory& category);
    void updateCategory(const ResourceCategory& category);
    void removeCategory(const QString& id);

    void addResourceType(const ResourceType& resourceType);
    void updateResourceType(const ResourceType& resourceType);
    void removeResourceType(const QString& id);

    void addPersonToResourceType(const QString& resourceTypeId, const QString& personId);
    void removePersonFromResourceType(const QString& resourceTypeId, const QString& personId);
    void addFamilyToResourceType(const QString& resourceTypeId, const QString& familyId);
    void removeFamilyFromResourceType(const QString& resourceTypeId, const QString& familyId);

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

    /// Remove a person from all references (teams, tags, resources, ministering).
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
    std::optional<ResourceCategory> findCategoryById(const QString& id) const;
    std::optional<ResourceType> findResourceTypeById(const QString& id) const;

    /// Returns the maximum known age across all persons, or nullopt if no ages are known.
    /// Used by UI to determine available age filter options.
    std::optional<int> maxKnownAge() const;

    // JSON serialization
    QJsonObject toJson() const;
    static Document fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Document& other) const;
    bool operator!=(const Document& other) const { return !(*this == other); }

private:
    void rebuildPersonToFamilyMap();

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

    // Resource collections
    QHash<QString, ResourceCategory> m_categories;
    QHash<QString, ResourceType> m_resourceTypes;

    // Ministering
    QHash<QString, MinisteringDistrict> m_eqDistricts;
    QHash<QString, MinisteringGroup> m_eqGroups;
    QHash<QString, MinisteringDistrict> m_rsDistricts;
    QHash<QString, MinisteringGroup> m_rsGroups;

    // Import dates (for conflict resolution)
    std::optional<QDate> m_wardDirectoryPdfDate;
    std::optional<QDate> m_ministeringPdfDate;
};
