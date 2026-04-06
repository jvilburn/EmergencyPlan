#include "DocumentChange.h"

// FamilyScopeBuilder
DocumentChange FamilyScopeBuilder::updated(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::added(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::removed(const FamilyId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.familyId = id;
    return c;
}
DocumentChange FamilyScopeBuilder::full() const
{
    return DocumentChange::full();
}

// TeamScopeBuilder
DocumentChange TeamScopeBuilder::updated(const TeamId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.teamId = id;
    return c;
}
DocumentChange TeamScopeBuilder::added(const TeamId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.teamId = id;
    return c;
}
DocumentChange TeamScopeBuilder::removed(const TeamId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.teamId = id;
    return c;
}
DocumentChange TeamScopeBuilder::full() const
{
    return DocumentChange::full();
}

// TagScopeBuilder
DocumentChange TagScopeBuilder::updated(const TagId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.tagId = id;
    return c;
}
DocumentChange TagScopeBuilder::added(const TagId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.tagId = id;
    return c;
}
DocumentChange TagScopeBuilder::removed(const TagId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.tagId = id;
    return c;
}
DocumentChange TagScopeBuilder::full() const
{
    return DocumentChange::full();
}

// AssetScopeBuilder
DocumentChange AssetScopeBuilder::updated(const EmergencyAssetId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.assetId = id;
    return c;
}
DocumentChange AssetScopeBuilder::added(const EmergencyAssetId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.assetId = id;
    return c;
}
DocumentChange AssetScopeBuilder::removed(const EmergencyAssetId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.assetId = id;
    return c;
}
DocumentChange AssetScopeBuilder::full() const
{
    return DocumentChange::full();
}

// EqDistrictScopeBuilder
DocumentChange EqDistrictScopeBuilder::updated(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.eqDistrictId = id;
    return c;
}
DocumentChange EqDistrictScopeBuilder::added(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.eqDistrictId = id;
    return c;
}
DocumentChange EqDistrictScopeBuilder::removed(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.eqDistrictId = id;
    return c;
}
DocumentChange EqDistrictScopeBuilder::full() const
{
    return DocumentChange::full();
}

// RsDistrictScopeBuilder
DocumentChange RsDistrictScopeBuilder::updated(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.rsDistrictId = id;
    return c;
}
DocumentChange RsDistrictScopeBuilder::added(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.rsDistrictId = id;
    return c;
}
DocumentChange RsDistrictScopeBuilder::removed(const MinisteringDistrictId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.rsDistrictId = id;
    return c;
}
DocumentChange RsDistrictScopeBuilder::full() const
{
    return DocumentChange::full();
}

// EqGroupScopeBuilder
DocumentChange EqGroupScopeBuilder::updated(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.eqGroupId = id;
    return c;
}
DocumentChange EqGroupScopeBuilder::added(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.eqGroupId = id;
    return c;
}
DocumentChange EqGroupScopeBuilder::removed(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.eqGroupId = id;
    return c;
}
DocumentChange EqGroupScopeBuilder::full() const
{
    return DocumentChange::full();
}

// RsGroupScopeBuilder
DocumentChange RsGroupScopeBuilder::updated(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.rsGroupId = id;
    return c;
}
DocumentChange RsGroupScopeBuilder::added(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Added;
    c.rsGroupId = id;
    return c;
}
DocumentChange RsGroupScopeBuilder::removed(const MinisteringGroupId& id) const
{
    DocumentChange c;
    c.action = ChangeAction::Removed;
    c.rsGroupId = id;
    return c;
}
DocumentChange RsGroupScopeBuilder::full() const
{
    return DocumentChange::full();
}

// MetadataScopeBuilder
DocumentChange MetadataScopeBuilder::updated(const QString& value) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.metadataValue = value;
    return c;
}
DocumentChange MetadataScopeBuilder::full() const
{
    return DocumentChange::full();
}

// DocumentChange factory methods
DocumentChange DocumentChange::full()
{
    return DocumentChange{ChangeAction::Full};
}

FamilyScopeBuilder DocumentChange::family() { return FamilyScopeBuilder{}; }
TeamScopeBuilder DocumentChange::team() { return TeamScopeBuilder{}; }
TagScopeBuilder DocumentChange::tag() { return TagScopeBuilder{}; }
AssetScopeBuilder DocumentChange::emergencyAsset() { return AssetScopeBuilder{}; }
EqDistrictScopeBuilder DocumentChange::eqDistrict() { return EqDistrictScopeBuilder{}; }
RsDistrictScopeBuilder DocumentChange::rsDistrict() { return RsDistrictScopeBuilder{}; }
EqGroupScopeBuilder DocumentChange::eqGroup() { return EqGroupScopeBuilder{}; }
RsGroupScopeBuilder DocumentChange::rsGroup() { return RsGroupScopeBuilder{}; }
MetadataScopeBuilder DocumentChange::metadata() { return MetadataScopeBuilder{}; }
