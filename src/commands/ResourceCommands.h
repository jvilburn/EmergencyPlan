#pragma once

#include "Command.h"
#include "ResourceCategory.h"
#include "ResourceType.h"

// ============================================================================
// Category Commands
// ============================================================================

class AddCategoryCommand : public Command
{
public:
    explicit AddCategoryCommand(const ResourceCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceCategory m_category;
};

class UpdateCategoryCommand : public Command
{
public:
    UpdateCategoryCommand(const ResourceCategory& oldCategory, const ResourceCategory& newCategory);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceCategory m_oldCategory;
    ResourceCategory m_newCategory;
};

class DeleteCategoryCommand : public Command
{
public:
    explicit DeleteCategoryCommand(const ResourceCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceCategory m_category;
};

// ============================================================================
// ResourceType Commands
// ============================================================================

class AddResourceTypeCommand : public Command
{
public:
    explicit AddResourceTypeCommand(const ResourceType& resourceType);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceType m_resourceType;
};

class UpdateResourceTypeCommand : public Command
{
public:
    UpdateResourceTypeCommand(const ResourceType& oldType, const ResourceType& newType);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceType m_oldType;
    ResourceType m_newType;
};

class DeleteResourceTypeCommand : public Command
{
public:
    explicit DeleteResourceTypeCommand(const ResourceType& resourceType);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ResourceType m_resourceType;
};

// ============================================================================
// Assignment Commands
// ============================================================================

class AssignResourceToPersonCommand : public Command
{
public:
    AssignResourceToPersonCommand(const QString& resourceTypeId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceTypeId;
    QString m_personId;
};

class AssignResourceToFamilyCommand : public Command
{
public:
    AssignResourceToFamilyCommand(const QString& resourceTypeId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceTypeId;
    QString m_familyId;
};

class UnassignResourceFromPersonCommand : public Command
{
public:
    UnassignResourceFromPersonCommand(const QString& resourceTypeId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceTypeId;
    QString m_personId;
};

class UnassignResourceFromFamilyCommand : public Command
{
public:
    UnassignResourceFromFamilyCommand(const QString& resourceTypeId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceTypeId;
    QString m_familyId;
};
