#include "FamilyCommands.h"
#include "Document.h"

// ============================================================================
// AddFamilyCommand
// ============================================================================

AddFamilyCommand::AddFamilyCommand(const Family& family)
    : m_family(family)
{
}

void AddFamilyCommand::execute(Document& document)
{
    document.addFamily(m_family);
}

void AddFamilyCommand::undo(Document& document)
{
    document.removeFamily(m_family.id());
}

QString AddFamilyCommand::description() const
{
    return QObject::tr("Add Family \"%1\"").arg(m_family.displayName());
}

DocumentChange AddFamilyCommand::documentChange() const
{
    return DocumentChange::family().added(m_family.id());
}

// ============================================================================
// UpdateFamilyCommand
// ============================================================================

UpdateFamilyCommand::UpdateFamilyCommand(const Family& oldFamily, const Family& newFamily)
    : m_oldFamily(oldFamily)
    , m_newFamily(newFamily)
{
}

void UpdateFamilyCommand::execute(Document& document)
{
    document.updateFamily(m_newFamily);
}

void UpdateFamilyCommand::undo(Document& document)
{
    document.updateFamily(m_oldFamily);
}

QString UpdateFamilyCommand::description() const
{
    return QObject::tr("Update Family \"%1\"").arg(m_oldFamily.displayName());
}

DocumentChange UpdateFamilyCommand::documentChange() const
{
    return DocumentChange::family().updated(m_newFamily.id());
}

// ============================================================================
// DeleteFamilyCommand
// ============================================================================

DeleteFamilyCommand::DeleteFamilyCommand(const Family& family)
    : m_family(family)
{
}

void DeleteFamilyCommand::execute(Document& document)
{
    document.removeFamily(m_family.id());
}

void DeleteFamilyCommand::undo(Document& document)
{
    document.addFamily(m_family);
}

QString DeleteFamilyCommand::description() const
{
    return QObject::tr("Delete Family \"%1\"").arg(m_family.displayName());
}

DocumentChange DeleteFamilyCommand::documentChange() const
{
    return DocumentChange::family().removed(m_family.id());
}

// ============================================================================
// SetFamiliesCommand
// ============================================================================

SetFamiliesCommand::SetFamiliesCommand(const QHash<QString, Family>& families,
                                       const QString& description)
    : m_newFamilies(families)
    , m_description(description)
{
}

void SetFamiliesCommand::execute(Document& document)
{
    m_oldFamilies = document.families();
    document.setFamilies(m_newFamilies);
}

void SetFamiliesCommand::undo(Document& document)
{
    document.setFamilies(m_oldFamilies);
}

QString SetFamiliesCommand::description() const
{
    if (!m_description.isEmpty())
    {
        return m_description;
    }
    return QObject::tr("Set %1 Families").arg(m_newFamilies.size());
}

DocumentChange SetFamiliesCommand::documentChange() const
{
    return DocumentChange::family().batchModified();
}
