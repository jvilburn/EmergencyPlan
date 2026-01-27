#include "DocumentChange.h"

// ScopeBuilder implementations

DocumentChange ScopeBuilder::updated(const QString& id) const
{
    return DocumentChange{scope, ChangeAction::Updated, id};
}

DocumentChange ScopeBuilder::added(const QString& id) const
{
    return DocumentChange{scope, ChangeAction::Added, id};
}

DocumentChange ScopeBuilder::removed(const QString& id) const
{
    return DocumentChange{scope, ChangeAction::Removed, id};
}

DocumentChange ScopeBuilder::batchModified() const
{
    return DocumentChange{scope, ChangeAction::BatchModified, QString()};
}

// DocumentChange factory methods

DocumentChange DocumentChange::full()
{
    return DocumentChange{ChangeScope::Full, ChangeAction::BatchModified, QString()};
}

ScopeBuilder DocumentChange::family()
{
    return ScopeBuilder{ChangeScope::Family};
}

ScopeBuilder DocumentChange::team()
{
    return ScopeBuilder{ChangeScope::Team};
}

ScopeBuilder DocumentChange::tag()
{
    return ScopeBuilder{ChangeScope::Tag};
}

ScopeBuilder DocumentChange::emergencyResource()
{
    return ScopeBuilder{ChangeScope::EmergencyResource};
}

ScopeBuilder DocumentChange::eqDistrict()
{
    return ScopeBuilder{ChangeScope::EqDistrict};
}

ScopeBuilder DocumentChange::eqGroup()
{
    return ScopeBuilder{ChangeScope::EqGroup};
}

ScopeBuilder DocumentChange::rsDistrict()
{
    return ScopeBuilder{ChangeScope::RsDistrict};
}

ScopeBuilder DocumentChange::rsGroup()
{
    return ScopeBuilder{ChangeScope::RsGroup};
}

ScopeBuilder DocumentChange::metadata()
{
    return ScopeBuilder{ChangeScope::Metadata};
}
