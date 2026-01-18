#pragma once

#include "Command.h"
#include "EquipmentCategory.h"
#include "Equipment.h"

// ============================================================================
// Category Commands
// ============================================================================

class AddEquipmentCategoryCommand : public Command
{
public:
    explicit AddEquipmentCategoryCommand(const EquipmentCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_category;
};

class UpdateEquipmentCategoryCommand : public Command
{
public:
    UpdateEquipmentCategoryCommand(const EquipmentCategory& oldCategory,
                                   const EquipmentCategory& newCategory);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_oldCategory;
    EquipmentCategory m_newCategory;
};

class DeleteEquipmentCategoryCommand : public Command
{
public:
    explicit DeleteEquipmentCategoryCommand(const EquipmentCategory& category);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EquipmentCategory m_category;
};

// ============================================================================
// Equipment Commands
// ============================================================================

class AddEquipmentCommand : public Command
{
public:
    explicit AddEquipmentCommand(const Equipment& equipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_equipment;
};

class UpdateEquipmentCommand : public Command
{
public:
    UpdateEquipmentCommand(const Equipment& oldEquipment, const Equipment& newEquipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_oldEquipment;
    Equipment m_newEquipment;
};

class DeleteEquipmentCommand : public Command
{
public:
    explicit DeleteEquipmentCommand(const Equipment& equipment);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Equipment m_equipment;
};

// ============================================================================
// Assignment Commands
// ============================================================================

class AssignEquipmentToFamilyCommand : public Command
{
public:
    AssignEquipmentToFamilyCommand(const QString& equipmentId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_equipmentId;
    QString m_familyId;
};

class UnassignEquipmentFromFamilyCommand : public Command
{
public:
    UnassignEquipmentFromFamilyCommand(const QString& equipmentId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_equipmentId;
    QString m_familyId;
};
