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

struct EmergencyScopeBuilder
{
    /// A change to the emergency as a whole: started, ended, restored, or a category added.
    DocumentChange updated() const;

    /// A change to one family's attempts or tasks in the emergency.
    DocumentChange updated(const FamilyId& familyId) const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // An Updated change sets one of familyId, teamId, assetId, emergencyChanged and
    // emergencyFamilyId: a change to one family, team or emergency asset, to the emergency as
    // a whole, or to one family's attempts or tasks in the emergency. familyId means the
    // family itself changed, so an emergency change never sets it. An Updated change with
    // none set is to the ward details.
    std::optional<FamilyId> familyId;
    std::optional<TeamId> teamId;
    std::optional<EmergencyAssetId> assetId;
    bool emergencyChanged = false;
    std::optional<FamilyId> emergencyFamilyId;

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static MetadataScopeBuilder metadata();
    static TeamScopeBuilder team();
    static EmergencyAssetScopeBuilder emergencyAsset();
    static EmergencyScopeBuilder emergency();
};

Q_DECLARE_METATYPE(DocumentChange)
