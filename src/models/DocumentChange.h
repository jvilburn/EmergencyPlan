#pragma once

#include <QString>

enum class ChangeScope
{
    Full,             // Entire document (open, new, bulk import)
    Family,
    Team,
    Tag,
    EmergencyAsset,
    EqDistrict,
    EqGroup,
    RsDistrict,
    RsGroup,
    Metadata          // Wards/Stakes
};

enum class ChangeAction
{
    BatchModified,    // Collection changed (import, set all)
    Updated,          // Single entity modified
    Added,            // Single entity added
    Removed           // Single entity removed
};

struct DocumentChange;

struct ScopeBuilder
{
    ChangeScope scope;

    DocumentChange updated(const QString& id) const;
    DocumentChange added(const QString& id) const;
    DocumentChange removed(const QString& id) const;
    DocumentChange batchModified() const;
};

struct DocumentChange
{
    ChangeScope scope = ChangeScope::Full;
    ChangeAction action = ChangeAction::BatchModified;
    QString entityId;

    // Full document change
    static DocumentChange full();

    // Scope selectors return builders
    static ScopeBuilder family();
    static ScopeBuilder team();
    static ScopeBuilder tag();
    static ScopeBuilder emergencyAsset();
    static ScopeBuilder eqDistrict();
    static ScopeBuilder eqGroup();
    static ScopeBuilder rsDistrict();
    static ScopeBuilder rsGroup();
    static ScopeBuilder metadata();
};
