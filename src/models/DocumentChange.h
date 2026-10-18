#pragma once

#include "Id.h"
#include <QMetaType>
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
};

struct MetadataScopeBuilder
{
    DocumentChange updated(const QString& value) const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // At most one is set - which field is set determines the scope
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
    static MetadataScopeBuilder metadata();
};

Q_DECLARE_METATYPE(DocumentChange)
