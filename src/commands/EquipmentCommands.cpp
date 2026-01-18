#include "EquipmentCommands.h"
#include "Document.h"

// ============================================================================
// AddEquipmentCategoryCommand
// ============================================================================

AddEquipmentCategoryCommand::AddEquipmentCategoryCommand(const EquipmentCategory& category)
    : m_category(category)
{
}

void AddEquipmentCategoryCommand::execute(Document& document)
{
    document.addEquipmentCategory(m_category);
}

void AddEquipmentCategoryCommand::undo(Document& document)
{
    document.removeEquipmentCategory(m_category.id());
}

QString AddEquipmentCategoryCommand::description() const
{
    return QObject::tr("Add Equipment Category \"%1\"").arg(m_category.name());
}

DocumentChange AddEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().added(m_category.id());
}

// ============================================================================
// UpdateEquipmentCategoryCommand
// ============================================================================

UpdateEquipmentCategoryCommand::UpdateEquipmentCategoryCommand(const EquipmentCategory& oldCategory,
                                                               const EquipmentCategory& newCategory)
    : m_oldCategory(oldCategory)
    , m_newCategory(newCategory)
{
}

void UpdateEquipmentCategoryCommand::execute(Document& document)
{
    document.updateEquipmentCategory(m_newCategory);
}

void UpdateEquipmentCategoryCommand::undo(Document& document)
{
    document.updateEquipmentCategory(m_oldCategory);
}

QString UpdateEquipmentCategoryCommand::description() const
{
    return QObject::tr("Update Equipment Category \"%1\"").arg(m_oldCategory.name());
}

DocumentChange UpdateEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().updated(m_newCategory.id());
}

// ============================================================================
// DeleteEquipmentCategoryCommand
// ============================================================================

DeleteEquipmentCategoryCommand::DeleteEquipmentCategoryCommand(const EquipmentCategory& category)
    : m_category(category)
{
}

void DeleteEquipmentCategoryCommand::execute(Document& document)
{
    document.removeEquipmentCategory(m_category.id());
}

void DeleteEquipmentCategoryCommand::undo(Document& document)
{
    document.addEquipmentCategory(m_category);
}

QString DeleteEquipmentCategoryCommand::description() const
{
    return QObject::tr("Delete Equipment Category \"%1\"").arg(m_category.name());
}

DocumentChange DeleteEquipmentCategoryCommand::documentChange() const
{
    return DocumentChange::equipmentCategory().removed(m_category.id());
}

// ============================================================================
// AddEquipmentCommand
// ============================================================================

AddEquipmentCommand::AddEquipmentCommand(const Equipment& equipment)
    : m_equipment(equipment)
{
}

void AddEquipmentCommand::execute(Document& document)
{
    document.addEquipment(m_equipment);
}

void AddEquipmentCommand::undo(Document& document)
{
    document.removeEquipment(m_equipment.id());
}

QString AddEquipmentCommand::description() const
{
    return QObject::tr("Add Equipment \"%1\"").arg(m_equipment.name());
}

DocumentChange AddEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().added(m_equipment.id());
}

// ============================================================================
// UpdateEquipmentCommand
// ============================================================================

UpdateEquipmentCommand::UpdateEquipmentCommand(const Equipment& oldEquipment,
                                               const Equipment& newEquipment)
    : m_oldEquipment(oldEquipment)
    , m_newEquipment(newEquipment)
{
}

void UpdateEquipmentCommand::execute(Document& document)
{
    document.updateEquipment(m_newEquipment);
}

void UpdateEquipmentCommand::undo(Document& document)
{
    document.updateEquipment(m_oldEquipment);
}

QString UpdateEquipmentCommand::description() const
{
    return QObject::tr("Update Equipment \"%1\"").arg(m_oldEquipment.name());
}

DocumentChange UpdateEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_newEquipment.id());
}

// ============================================================================
// DeleteEquipmentCommand
// ============================================================================

DeleteEquipmentCommand::DeleteEquipmentCommand(const Equipment& equipment)
    : m_equipment(equipment)
{
}

void DeleteEquipmentCommand::execute(Document& document)
{
    document.removeEquipment(m_equipment.id());
}

void DeleteEquipmentCommand::undo(Document& document)
{
    document.addEquipment(m_equipment);
}

QString DeleteEquipmentCommand::description() const
{
    return QObject::tr("Delete Equipment \"%1\"").arg(m_equipment.name());
}

DocumentChange DeleteEquipmentCommand::documentChange() const
{
    return DocumentChange::equipment().removed(m_equipment.id());
}

// ============================================================================
// AssignEquipmentToFamilyCommand
// ============================================================================

AssignEquipmentToFamilyCommand::AssignEquipmentToFamilyCommand(const QString& equipmentId,
                                                               const QString& familyId)
    : m_equipmentId(equipmentId)
    , m_familyId(familyId)
{
}

void AssignEquipmentToFamilyCommand::execute(Document& document)
{
    document.addFamilyToEquipment(m_equipmentId, m_familyId);
}

void AssignEquipmentToFamilyCommand::undo(Document& document)
{
    document.removeFamilyFromEquipment(m_equipmentId, m_familyId);
}

QString AssignEquipmentToFamilyCommand::description() const
{
    return QObject::tr("Assign Equipment to Family");
}

DocumentChange AssignEquipmentToFamilyCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_equipmentId);
}

// ============================================================================
// UnassignEquipmentFromFamilyCommand
// ============================================================================

UnassignEquipmentFromFamilyCommand::UnassignEquipmentFromFamilyCommand(const QString& equipmentId,
                                                                       const QString& familyId)
    : m_equipmentId(equipmentId)
    , m_familyId(familyId)
{
}

void UnassignEquipmentFromFamilyCommand::execute(Document& document)
{
    document.removeFamilyFromEquipment(m_equipmentId, m_familyId);
}

void UnassignEquipmentFromFamilyCommand::undo(Document& document)
{
    document.addFamilyToEquipment(m_equipmentId, m_familyId);
}

QString UnassignEquipmentFromFamilyCommand::description() const
{
    return QObject::tr("Remove Equipment from Family");
}

DocumentChange UnassignEquipmentFromFamilyCommand::documentChange() const
{
    return DocumentChange::equipment().updated(m_equipmentId);
}
