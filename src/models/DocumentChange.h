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
    DocumentChange updated(const QString& value) const;
};

// === DocumentChange struct ===

struct DocumentChange
{
    ChangeAction action = ChangeAction::Full;

    // At most one is set - which field is set determines the scope
    std::optional<FamilyId> familyId;
    QString metadataValue;  // ward/stake unit numbers (external identifiers)

    // Full document change (no ID fields set)
    static DocumentChange full();

    // Scope builders
    static FamilyScopeBuilder family();
    static MetadataScopeBuilder metadata();
};

Q_DECLARE_METATYPE(DocumentChange)
