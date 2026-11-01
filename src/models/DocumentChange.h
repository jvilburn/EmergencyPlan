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

struct EmergencyResponseScopeBuilder
{
    /// A change to the response as a whole: started, ended, restored, or a category added.
    DocumentChange updated(const EmergencyResponseId& id) const;

    /// A change to one family's attempts or tasks in the response.
    DocumentChange updated(const EmergencyResponseId& id, const FamilyId& familyId) const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // An Updated change sets one of familyId, teamId, assetId and responseId: a change to
    // one family, team, emergency asset or emergency response. A change to one family's
    // attempts or tasks in an emergency response also sets responseFamilyId. An Updated
    // change with none set is to the ward details.
    std::optional<FamilyId> familyId;
    std::optional<TeamId> teamId;
    std::optional<EmergencyAssetId> assetId;
    std::optional<EmergencyResponseId> responseId;
    std::optional<FamilyId> responseFamilyId;

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static MetadataScopeBuilder metadata();
    static TeamScopeBuilder team();
    static EmergencyAssetScopeBuilder emergencyAsset();
    static EmergencyResponseScopeBuilder emergencyResponse();
};

Q_DECLARE_METATYPE(DocumentChange)
