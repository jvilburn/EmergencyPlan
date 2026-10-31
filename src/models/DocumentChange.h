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

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // Set for a change to one family; an Updated change without it is to the ward details
    std::optional<FamilyId> familyId;

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static MetadataScopeBuilder metadata();
};

Q_DECLARE_METATYPE(DocumentChange)
