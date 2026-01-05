#include "TagCommands.h"
#include "Document.h"

// ============================================================================
// AddTagCommand
// ============================================================================

AddTagCommand::AddTagCommand(const Tag& tag)
    : m_tag(tag)
{
}

void AddTagCommand::execute(Document& document)
{
    document.addTag(m_tag);
}

void AddTagCommand::undo(Document& document)
{
    document.removeTag(m_tag.id());
}

QString AddTagCommand::description() const
{
    return QObject::tr("Add Tag \"%1\"").arg(m_tag.name());
}

// ============================================================================
// UpdateTagCommand
// ============================================================================

UpdateTagCommand::UpdateTagCommand(const Tag& oldTag, const Tag& newTag)
    : m_oldTag(oldTag)
    , m_newTag(newTag)
{
}

void UpdateTagCommand::execute(Document& document)
{
    document.updateTag(m_newTag);
}

void UpdateTagCommand::undo(Document& document)
{
    document.updateTag(m_oldTag);
}

QString UpdateTagCommand::description() const
{
    return QObject::tr("Update Tag \"%1\"").arg(m_oldTag.name());
}

// ============================================================================
// DeleteTagCommand
// ============================================================================

DeleteTagCommand::DeleteTagCommand(const Tag& tag)
    : m_tag(tag)
{
}

void DeleteTagCommand::execute(Document& document)
{
    document.removeTag(m_tag.id());
}

void DeleteTagCommand::undo(Document& document)
{
    document.addTag(m_tag);
}

QString DeleteTagCommand::description() const
{
    return QObject::tr("Delete Tag \"%1\"").arg(m_tag.name());
}

// ============================================================================
// AssignTagToPersonCommand
// ============================================================================

AssignTagToPersonCommand::AssignTagToPersonCommand(const QString& tagId, const QString& personId)
    : m_tagId(tagId)
    , m_personId(personId)
{
}

void AssignTagToPersonCommand::execute(Document& document)
{
    document.addPersonToTag(m_tagId, m_personId);
}

void AssignTagToPersonCommand::undo(Document& document)
{
    document.removePersonFromTag(m_tagId, m_personId);
}

QString AssignTagToPersonCommand::description() const
{
    return QObject::tr("Assign Tag to Person");
}

// ============================================================================
// AssignTagToFamilyCommand
// ============================================================================

AssignTagToFamilyCommand::AssignTagToFamilyCommand(const QString& tagId, const QString& familyId)
    : m_tagId(tagId)
    , m_familyId(familyId)
{
}

void AssignTagToFamilyCommand::execute(Document& document)
{
    document.addFamilyToTag(m_tagId, m_familyId);
}

void AssignTagToFamilyCommand::undo(Document& document)
{
    document.removeFamilyFromTag(m_tagId, m_familyId);
}

QString AssignTagToFamilyCommand::description() const
{
    return QObject::tr("Assign Tag to Family");
}

// ============================================================================
// UnassignTagFromPersonCommand
// ============================================================================

UnassignTagFromPersonCommand::UnassignTagFromPersonCommand(const QString& tagId, const QString& personId)
    : m_tagId(tagId)
    , m_personId(personId)
{
}

void UnassignTagFromPersonCommand::execute(Document& document)
{
    document.removePersonFromTag(m_tagId, m_personId);
}

void UnassignTagFromPersonCommand::undo(Document& document)
{
    document.addPersonToTag(m_tagId, m_personId);
}

QString UnassignTagFromPersonCommand::description() const
{
    return QObject::tr("Remove Tag from Person");
}

// ============================================================================
// UnassignTagFromFamilyCommand
// ============================================================================

UnassignTagFromFamilyCommand::UnassignTagFromFamilyCommand(const QString& tagId, const QString& familyId)
    : m_tagId(tagId)
    , m_familyId(familyId)
{
}

void UnassignTagFromFamilyCommand::execute(Document& document)
{
    document.removeFamilyFromTag(m_tagId, m_familyId);
}

void UnassignTagFromFamilyCommand::undo(Document& document)
{
    document.addFamilyToTag(m_tagId, m_familyId);
}

QString UnassignTagFromFamilyCommand::description() const
{
    return QObject::tr("Remove Tag from Family");
}
