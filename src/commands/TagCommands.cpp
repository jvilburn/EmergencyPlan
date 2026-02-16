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

DocumentChange AddTagCommand::documentChange() const
{
    return DocumentChange::tag().added(m_tag.id());
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

DocumentChange UpdateTagCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_newTag.id());
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

DocumentChange DeleteTagCommand::documentChange() const
{
    return DocumentChange::tag().removed(m_tag.id());
}

// ============================================================================
// AssignTagToPersonCommand
// ============================================================================

AssignTagToPersonCommand::AssignTagToPersonCommand(const TagId& tagId, const PersonId& personId)
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

DocumentChange AssignTagToPersonCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

// ============================================================================
// AssignTagToFamilyCommand
// ============================================================================

AssignTagToFamilyCommand::AssignTagToFamilyCommand(const TagId& tagId, const FamilyId& familyId)
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

DocumentChange AssignTagToFamilyCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

// ============================================================================
// UnassignTagFromPersonCommand
// ============================================================================

UnassignTagFromPersonCommand::UnassignTagFromPersonCommand(const TagId& tagId, const PersonId& personId)
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

DocumentChange UnassignTagFromPersonCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}

// ============================================================================
// UnassignTagFromFamilyCommand
// ============================================================================

UnassignTagFromFamilyCommand::UnassignTagFromFamilyCommand(const TagId& tagId, const FamilyId& familyId)
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

DocumentChange UnassignTagFromFamilyCommand::documentChange() const
{
    return DocumentChange::tag().updated(m_tagId);
}
