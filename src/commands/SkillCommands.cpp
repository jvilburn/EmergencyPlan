#include "SkillCommands.h"
#include "Document.h"

// ============================================================================
// AddSkillCategoryCommand
// ============================================================================

AddSkillCategoryCommand::AddSkillCategoryCommand(const SkillCategory& category)
    : m_category(category)
{
}

void AddSkillCategoryCommand::execute(Document& document)
{
    document.addSkillCategory(m_category);
}

void AddSkillCategoryCommand::undo(Document& document)
{
    document.removeSkillCategory(m_category.id());
}

QString AddSkillCategoryCommand::description() const
{
    return QObject::tr("Add Skill Category \"%1\"").arg(m_category.name());
}

DocumentChange AddSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().added(m_category.id());
}

// ============================================================================
// UpdateSkillCategoryCommand
// ============================================================================

UpdateSkillCategoryCommand::UpdateSkillCategoryCommand(const SkillCategory& oldCategory, const SkillCategory& newCategory)
    : m_oldCategory(oldCategory)
    , m_newCategory(newCategory)
{
}

void UpdateSkillCategoryCommand::execute(Document& document)
{
    document.updateSkillCategory(m_newCategory);
}

void UpdateSkillCategoryCommand::undo(Document& document)
{
    document.updateSkillCategory(m_oldCategory);
}

QString UpdateSkillCategoryCommand::description() const
{
    return QObject::tr("Update Skill Category \"%1\"").arg(m_oldCategory.name());
}

DocumentChange UpdateSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().updated(m_newCategory.id());
}

// ============================================================================
// DeleteSkillCategoryCommand
// ============================================================================

DeleteSkillCategoryCommand::DeleteSkillCategoryCommand(const SkillCategory& category)
    : m_category(category)
{
}

void DeleteSkillCategoryCommand::execute(Document& document)
{
    document.removeSkillCategory(m_category.id());
}

void DeleteSkillCategoryCommand::undo(Document& document)
{
    document.addSkillCategory(m_category);
}

QString DeleteSkillCategoryCommand::description() const
{
    return QObject::tr("Delete Skill Category \"%1\"").arg(m_category.name());
}

DocumentChange DeleteSkillCategoryCommand::documentChange() const
{
    return DocumentChange::skillCategory().removed(m_category.id());
}

// ============================================================================
// AddSkillCommand
// ============================================================================

AddSkillCommand::AddSkillCommand(const Skill& skill)
    : m_skill(skill)
{
}

void AddSkillCommand::execute(Document& document)
{
    document.addSkill(m_skill);
}

void AddSkillCommand::undo(Document& document)
{
    document.removeSkill(m_skill.id());
}

QString AddSkillCommand::description() const
{
    return QObject::tr("Add Skill \"%1\"").arg(m_skill.name());
}

DocumentChange AddSkillCommand::documentChange() const
{
    return DocumentChange::skill().added(m_skill.id());
}

// ============================================================================
// UpdateSkillCommand
// ============================================================================

UpdateSkillCommand::UpdateSkillCommand(const Skill& oldSkill, const Skill& newSkill)
    : m_oldSkill(oldSkill)
    , m_newSkill(newSkill)
{
}

void UpdateSkillCommand::execute(Document& document)
{
    document.updateSkill(m_newSkill);
}

void UpdateSkillCommand::undo(Document& document)
{
    document.updateSkill(m_oldSkill);
}

QString UpdateSkillCommand::description() const
{
    return QObject::tr("Update Skill \"%1\"").arg(m_oldSkill.name());
}

DocumentChange UpdateSkillCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_newSkill.id());
}

// ============================================================================
// DeleteSkillCommand
// ============================================================================

DeleteSkillCommand::DeleteSkillCommand(const Skill& skill)
    : m_skill(skill)
{
}

void DeleteSkillCommand::execute(Document& document)
{
    document.removeSkill(m_skill.id());
}

void DeleteSkillCommand::undo(Document& document)
{
    document.addSkill(m_skill);
}

QString DeleteSkillCommand::description() const
{
    return QObject::tr("Delete Skill \"%1\"").arg(m_skill.name());
}

DocumentChange DeleteSkillCommand::documentChange() const
{
    return DocumentChange::skill().removed(m_skill.id());
}

// ============================================================================
// AssignSkillToPersonCommand
// ============================================================================

AssignSkillToPersonCommand::AssignSkillToPersonCommand(const QString& skillId, const QString& personId)
    : m_skillId(skillId)
    , m_personId(personId)
{
}

void AssignSkillToPersonCommand::execute(Document& document)
{
    document.addPersonToSkill(m_skillId, m_personId);
}

void AssignSkillToPersonCommand::undo(Document& document)
{
    document.removePersonFromSkill(m_skillId, m_personId);
}

QString AssignSkillToPersonCommand::description() const
{
    return QObject::tr("Assign Skill to Person");
}

DocumentChange AssignSkillToPersonCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_skillId);
}

// ============================================================================
// UnassignSkillFromPersonCommand
// ============================================================================

UnassignSkillFromPersonCommand::UnassignSkillFromPersonCommand(const QString& skillId, const QString& personId)
    : m_skillId(skillId)
    , m_personId(personId)
{
}

void UnassignSkillFromPersonCommand::execute(Document& document)
{
    document.removePersonFromSkill(m_skillId, m_personId);
}

void UnassignSkillFromPersonCommand::undo(Document& document)
{
    document.addPersonToSkill(m_skillId, m_personId);
}

QString UnassignSkillFromPersonCommand::description() const
{
    return QObject::tr("Remove Skill from Person");
}

DocumentChange UnassignSkillFromPersonCommand::documentChange() const
{
    return DocumentChange::skill().updated(m_skillId);
}
