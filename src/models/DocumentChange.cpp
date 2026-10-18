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
DocumentChange MetadataScopeBuilder::updated(const QString& value) const
{
    DocumentChange c;
    c.action = ChangeAction::Updated;
    c.metadataValue = value;
    return c;
}

// DocumentChange factory methods
DocumentChange DocumentChange::full()
{
    return DocumentChange{ChangeAction::Full};
}

FamilyScopeBuilder DocumentChange::family() { return FamilyScopeBuilder{}; }
MetadataScopeBuilder DocumentChange::metadata() { return MetadataScopeBuilder{}; }
