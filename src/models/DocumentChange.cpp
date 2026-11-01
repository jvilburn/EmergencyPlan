#include "DocumentChange.h"

// FamilyScopeBuilder
DocumentChange FamilyScopeBuilder::updated(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.familyId = id;
    return c;
}

// MetadataScopeBuilder
DocumentChange MetadataScopeBuilder::updated() const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    return c;
}

// TeamScopeBuilder
DocumentChange TeamScopeBuilder::updated(const TeamId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.teamId = id;
    return c;
}

// EmergencyAssetScopeBuilder
DocumentChange EmergencyAssetScopeBuilder::updated(const EmergencyAssetId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.assetId = id;
    return c;
}

// DocumentChange factory methods
DocumentChange DocumentChange::full()
{
    return DocumentChange{ChangeAction::Full};
}

FamilyScopeBuilder DocumentChange::family() { return FamilyScopeBuilder{}; }
MetadataScopeBuilder DocumentChange::metadata() { return MetadataScopeBuilder{}; }
TeamScopeBuilder DocumentChange::team() { return TeamScopeBuilder{}; }
EmergencyAssetScopeBuilder DocumentChange::emergencyAsset() { return EmergencyAssetScopeBuilder{}; }
