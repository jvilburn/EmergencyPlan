#include "ResourceCommands.h"
#include "Document.h"

// ============================================================================
// AddCategoryCommand
// ============================================================================

AddCategoryCommand::AddCategoryCommand(const ResourceCategory& category)
    : m_category(category)
{
}

void AddCategoryCommand::execute(Document& document)
{
    document.addCategory(m_category);
}

void AddCategoryCommand::undo(Document& document)
{
    document.removeCategory(m_category.id());
}

QString AddCategoryCommand::description() const
{
    return QObject::tr("Add Category \"%1\"").arg(m_category.name());
}

// ============================================================================
// UpdateCategoryCommand
// ============================================================================

UpdateCategoryCommand::UpdateCategoryCommand(const ResourceCategory& oldCategory,
                                              const ResourceCategory& newCategory)
    : m_oldCategory(oldCategory)
    , m_newCategory(newCategory)
{
}

void UpdateCategoryCommand::execute(Document& document)
{
    document.updateCategory(m_newCategory);
}

void UpdateCategoryCommand::undo(Document& document)
{
    document.updateCategory(m_oldCategory);
}

QString UpdateCategoryCommand::description() const
{
    return QObject::tr("Update Category \"%1\"").arg(m_oldCategory.name());
}

// ============================================================================
// DeleteCategoryCommand
// ============================================================================

DeleteCategoryCommand::DeleteCategoryCommand(const ResourceCategory& category)
    : m_category(category)
{
}

void DeleteCategoryCommand::execute(Document& document)
{
    document.removeCategory(m_category.id());
}

void DeleteCategoryCommand::undo(Document& document)
{
    document.addCategory(m_category);
}

QString DeleteCategoryCommand::description() const
{
    return QObject::tr("Delete Category \"%1\"").arg(m_category.name());
}

// ============================================================================
// AddResourceTypeCommand
// ============================================================================

AddResourceTypeCommand::AddResourceTypeCommand(const ResourceType& resourceType)
    : m_resourceType(resourceType)
{
}

void AddResourceTypeCommand::execute(Document& document)
{
    document.addResourceType(m_resourceType);
}

void AddResourceTypeCommand::undo(Document& document)
{
    document.removeResourceType(m_resourceType.id());
}

QString AddResourceTypeCommand::description() const
{
    return QObject::tr("Add Resource \"%1\"").arg(m_resourceType.name());
}

// ============================================================================
// UpdateResourceTypeCommand
// ============================================================================

UpdateResourceTypeCommand::UpdateResourceTypeCommand(const ResourceType& oldType,
                                                      const ResourceType& newType)
    : m_oldType(oldType)
    , m_newType(newType)
{
}

void UpdateResourceTypeCommand::execute(Document& document)
{
    document.updateResourceType(m_newType);
}

void UpdateResourceTypeCommand::undo(Document& document)
{
    document.updateResourceType(m_oldType);
}

QString UpdateResourceTypeCommand::description() const
{
    return QObject::tr("Update Resource \"%1\"").arg(m_oldType.name());
}

// ============================================================================
// DeleteResourceTypeCommand
// ============================================================================

DeleteResourceTypeCommand::DeleteResourceTypeCommand(const ResourceType& resourceType)
    : m_resourceType(resourceType)
{
}

void DeleteResourceTypeCommand::execute(Document& document)
{
    document.removeResourceType(m_resourceType.id());
}

void DeleteResourceTypeCommand::undo(Document& document)
{
    document.addResourceType(m_resourceType);
}

QString DeleteResourceTypeCommand::description() const
{
    return QObject::tr("Delete Resource \"%1\"").arg(m_resourceType.name());
}

// ============================================================================
// AssignResourceToPersonCommand
// ============================================================================

AssignResourceToPersonCommand::AssignResourceToPersonCommand(const QString& resourceTypeId,
                                                              const QString& personId)
    : m_resourceTypeId(resourceTypeId)
    , m_personId(personId)
{
}

void AssignResourceToPersonCommand::execute(Document& document)
{
    document.addPersonToResourceType(m_resourceTypeId, m_personId);
}

void AssignResourceToPersonCommand::undo(Document& document)
{
    document.removePersonFromResourceType(m_resourceTypeId, m_personId);
}

QString AssignResourceToPersonCommand::description() const
{
    return QObject::tr("Assign Resource to Person");
}

// ============================================================================
// AssignResourceToFamilyCommand
// ============================================================================

AssignResourceToFamilyCommand::AssignResourceToFamilyCommand(const QString& resourceTypeId,
                                                              const QString& familyId)
    : m_resourceTypeId(resourceTypeId)
    , m_familyId(familyId)
{
}

void AssignResourceToFamilyCommand::execute(Document& document)
{
    document.addFamilyToResourceType(m_resourceTypeId, m_familyId);
}

void AssignResourceToFamilyCommand::undo(Document& document)
{
    document.removeFamilyFromResourceType(m_resourceTypeId, m_familyId);
}

QString AssignResourceToFamilyCommand::description() const
{
    return QObject::tr("Assign Resource to Family");
}

// ============================================================================
// UnassignResourceFromPersonCommand
// ============================================================================

UnassignResourceFromPersonCommand::UnassignResourceFromPersonCommand(const QString& resourceTypeId,
                                                                      const QString& personId)
    : m_resourceTypeId(resourceTypeId)
    , m_personId(personId)
{
}

void UnassignResourceFromPersonCommand::execute(Document& document)
{
    document.removePersonFromResourceType(m_resourceTypeId, m_personId);
}

void UnassignResourceFromPersonCommand::undo(Document& document)
{
    document.addPersonToResourceType(m_resourceTypeId, m_personId);
}

QString UnassignResourceFromPersonCommand::description() const
{
    return QObject::tr("Remove Resource from Person");
}

// ============================================================================
// UnassignResourceFromFamilyCommand
// ============================================================================

UnassignResourceFromFamilyCommand::UnassignResourceFromFamilyCommand(const QString& resourceTypeId,
                                                                      const QString& familyId)
    : m_resourceTypeId(resourceTypeId)
    , m_familyId(familyId)
{
}

void UnassignResourceFromFamilyCommand::execute(Document& document)
{
    document.removeFamilyFromResourceType(m_resourceTypeId, m_familyId);
}

void UnassignResourceFromFamilyCommand::undo(Document& document)
{
    document.addFamilyToResourceType(m_resourceTypeId, m_familyId);
}

QString UnassignResourceFromFamilyCommand::description() const
{
    return QObject::tr("Remove Resource from Family");
}
