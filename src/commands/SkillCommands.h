#pragma once

#include "Command.h"
#include "SkillCategory.h"
#include "Skill.h"

// ============================================================================
// Category Commands
// ============================================================================

class AddSkillCategoryCommand : public Command
{
public:
    explicit AddSkillCategoryCommand(const SkillCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_category;
};

class UpdateSkillCategoryCommand : public Command
{
public:
    UpdateSkillCategoryCommand(const SkillCategory& oldCategory, const SkillCategory& newCategory);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_oldCategory;
    SkillCategory m_newCategory;
};

class DeleteSkillCategoryCommand : public Command
{
public:
    explicit DeleteSkillCategoryCommand(const SkillCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SkillCategory m_category;
};

// ============================================================================
// Skill Commands
// ============================================================================

class AddSkillCommand : public Command
{
public:
    explicit AddSkillCommand(const Skill& skill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_skill;
};

class UpdateSkillCommand : public Command
{
public:
    UpdateSkillCommand(const Skill& oldSkill, const Skill& newSkill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_oldSkill;
    Skill m_newSkill;
};

class DeleteSkillCommand : public Command
{
public:
    explicit DeleteSkillCommand(const Skill& skill);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Skill m_skill;
};

// ============================================================================
// Assignment Commands
// ============================================================================

class AssignSkillToPersonCommand : public Command
{
public:
    AssignSkillToPersonCommand(const QString& skillId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_skillId;
    QString m_personId;
};

class UnassignSkillFromPersonCommand : public Command
{
public:
    UnassignSkillFromPersonCommand(const QString& skillId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_skillId;
    QString m_personId;
};
