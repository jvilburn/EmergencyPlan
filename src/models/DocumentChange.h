#pragma once

#include "Id.h"
#include <QString>
#include <optional>

enum class ChangeAction
{
    Full,       // Entire document or collection changed (replaces old Full + BatchModified)
    Updated,    // Single entity modified
    Added,      // Single entity added
    Removed     // Single entity removed
};

struct DocumentChange;

// === Concrete scope builders ===

struct FamilyScopeBuilder
{
    DocumentChange updated(const FamilyId& id) const;
    DocumentChange added(const FamilyId& id) const;
    DocumentChange removed(const FamilyId& id) const;
    DocumentChange full() const;
};

struct TeamScopeBuilder
{
    DocumentChange updated(const TeamId& id) const;
    DocumentChange added(const TeamId& id) const;
    DocumentChange removed(const TeamId& id) const;
    DocumentChange full() const;
};

struct TagScopeBuilder
{
    DocumentChange updated(const TagId& id) const;
    DocumentChange added(const TagId& id) const;
    DocumentChange removed(const TagId& id) const;
    DocumentChange full() const;
};

struct AssetScopeBuilder
{
    DocumentChange updated(const EmergencyAssetId& id) const;
    DocumentChange added(const EmergencyAssetId& id) const;
    DocumentChange removed(const EmergencyAssetId& id) const;
    DocumentChange full() const;
};

struct EqDistrictScopeBuilder
{
    DocumentChange updated(const MinisteringDistrictId& id) const;
    DocumentChange added(const MinisteringDistrictId& id) const;
    DocumentChange removed(const MinisteringDistrictId& id) const;
    DocumentChange full() const;
};

struct RsDistrictScopeBuilder
{
    DocumentChange updated(const MinisteringDistrictId& id) const;
    DocumentChange added(const MinisteringDistrictId& id) const;
    DocumentChange removed(const MinisteringDistrictId& id) const;
    DocumentChange full() const;
};

struct EqGroupScopeBuilder
{
    DocumentChange updated(const MinisteringGroupId& id) const;
    DocumentChange added(const MinisteringGroupId& id) const;
    DocumentChange removed(const MinisteringGroupId& id) const;
    DocumentChange full() const;
};

struct RsGroupScopeBuilder
{
    DocumentChange updated(const MinisteringGroupId& id) const;
    DocumentChange added(const MinisteringGroupId& id) const;
    DocumentChange removed(const MinisteringGroupId& id) const;
    DocumentChange full() const;
};

struct MetadataScopeBuilder
{
    DocumentChange updated(const QString& value) const;
    DocumentChange full() const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // At most one is set — which field is set determines the scope
    std::optional<FamilyId> familyId;
    std::optional<TeamId> teamId;
    std::optional<TagId> tagId;
    std::optional<EmergencyAssetId> assetId;
    std::optional<MinisteringDistrictId> eqDistrictId;
    std::optional<MinisteringDistrictId> rsDistrictId;
    std::optional<MinisteringGroupId> eqGroupId;
    std::optional<MinisteringGroupId> rsGroupId;
    QString metadataValue;  // ward/stake unit numbers (external identifiers)

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static TeamScopeBuilder team();
    static TagScopeBuilder tag();
    static AssetScopeBuilder emergencyAsset();
    static EqDistrictScopeBuilder eqDistrict();
    static RsDistrictScopeBuilder rsDistrict();
    static EqGroupScopeBuilder eqGroup();
    static RsGroupScopeBuilder rsGroup();
    static MetadataScopeBuilder metadata();
};
