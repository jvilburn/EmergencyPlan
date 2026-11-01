#pragma once

#include "Id.h"
#include <QMetaType>
#include <QString>
#include <optional>

enum class ChangeAction
{
    Full,       // Entire document or collection changed
    Updated     // Single entity modified
};

struct DocumentChange;

// === Concrete scope builders ===

struct FamilyScopeBuilder
{
    DocumentChange updated(const FamilyId& id) const;
};

struct MetadataScopeBuilder
{
    DocumentChange updated() const;
};

struct TeamScopeBuilder
{
    DocumentChange updated(const TeamId& id) const;
};

struct EmergencyAssetScopeBuilder
{
    DocumentChange updated(const EmergencyAssetId& id) const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // At most one of these is set: a change to one family, team or emergency asset. An
    // Updated change with none set is to the ward details.
    std::optional<FamilyId> familyId;
    std::optional<TeamId> teamId;
    std::optional<EmergencyAssetId> assetId;

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static MetadataScopeBuilder metadata();
    static TeamScopeBuilder team();
    static EmergencyAssetScopeBuilder emergencyAsset();
};

Q_DECLARE_METATYPE(DocumentChange)
