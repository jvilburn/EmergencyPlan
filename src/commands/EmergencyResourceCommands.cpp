#include "EmergencyResourceCommands.h"
#include "Document.h"

// ============================================================================
// AddEmergencyResourceCommand
// ============================================================================

AddEmergencyResourceCommand::AddEmergencyResourceCommand(const EmergencyResource& resource)
    : m_resource(resource)
{
}

void AddEmergencyResourceCommand::execute(Document& document)
{
    document.addEmergencyResource(m_resource);
}

void AddEmergencyResourceCommand::undo(Document& document)
{
    document.removeEmergencyResource(m_resource.id());
}

QString AddEmergencyResourceCommand::description() const
{
    return QObject::tr("Add Resource \"%1\"").arg(m_resource.name());
}

DocumentChange AddEmergencyResourceCommand::documentChange() const
{
    return DocumentChange::emergencyResource().added(m_resource.id());
}

// ============================================================================
// UpdateEmergencyResourceCommand
// ============================================================================

UpdateEmergencyResourceCommand::UpdateEmergencyResourceCommand(const EmergencyResource& oldResource, const EmergencyResource& newResource)
    : m_oldResource(oldResource)
    , m_newResource(newResource)
{
}

void UpdateEmergencyResourceCommand::execute(Document& document)
{
    document.updateEmergencyResource(m_newResource);
}

void UpdateEmergencyResourceCommand::undo(Document& document)
{
    document.updateEmergencyResource(m_oldResource);
}

QString UpdateEmergencyResourceCommand::description() const
{
    return QObject::tr("Update Resource \"%1\"").arg(m_oldResource.name());
}

DocumentChange UpdateEmergencyResourceCommand::documentChange() const
{
    return DocumentChange::emergencyResource().updated(m_newResource.id());
}

// ============================================================================
// DeleteEmergencyResourceCommand
// ============================================================================

DeleteEmergencyResourceCommand::DeleteEmergencyResourceCommand(const EmergencyResource& resource)
    : m_resource(resource)
{
}

void DeleteEmergencyResourceCommand::execute(Document& document)
{
    document.removeEmergencyResource(m_resource.id());
}

void DeleteEmergencyResourceCommand::undo(Document& document)
{
    document.addEmergencyResource(m_resource);
}

QString DeleteEmergencyResourceCommand::description() const
{
    return QObject::tr("Delete Resource \"%1\"").arg(m_resource.name());
}

DocumentChange DeleteEmergencyResourceCommand::documentChange() const
{
    return DocumentChange::emergencyResource().removed(m_resource.id());
}

// ============================================================================
// AssignEmergencyResourceToPersonCommand
// ============================================================================

AssignEmergencyResourceToPersonCommand::AssignEmergencyResourceToPersonCommand(const QString& resourceId, const QString& personId)
    : m_resourceId(resourceId)
    , m_personId(personId)
{
}

void AssignEmergencyResourceToPersonCommand::execute(Document& document)
{
    auto resource = document.findEmergencyResourceById(m_resourceId);
    if (resource)
    {
        resource->addPerson(m_personId);
        document.updateEmergencyResource(*resource);
    }
}

void AssignEmergencyResourceToPersonCommand::undo(Document& document)
{
    auto resource = document.findEmergencyResourceById(m_resourceId);
    if (resource)
    {
        resource->removePerson(m_personId);
        document.updateEmergencyResource(*resource);
    }
}

QString AssignEmergencyResourceToPersonCommand::description() const
{
    return QObject::tr("Assign Resource to Person");
}

DocumentChange AssignEmergencyResourceToPersonCommand::documentChange() const
{
    return DocumentChange::emergencyResource().updated(m_resourceId);
}

// ============================================================================
// UnassignEmergencyResourceFromPersonCommand
// ============================================================================

UnassignEmergencyResourceFromPersonCommand::UnassignEmergencyResourceFromPersonCommand(const QString& resourceId, const QString& personId)
    : m_resourceId(resourceId)
    , m_personId(personId)
{
}

void UnassignEmergencyResourceFromPersonCommand::execute(Document& document)
{
    auto resource = document.findEmergencyResourceById(m_resourceId);
    if (resource)
    {
        resource->removePerson(m_personId);
        document.updateEmergencyResource(*resource);
    }
}

void UnassignEmergencyResourceFromPersonCommand::undo(Document& document)
{
    auto resource = document.findEmergencyResourceById(m_resourceId);
    if (resource)
    {
        resource->addPerson(m_personId);
        document.updateEmergencyResource(*resource);
    }
}

QString UnassignEmergencyResourceFromPersonCommand::description() const
{
    return QObject::tr("Remove Resource from Person");
}

DocumentChange UnassignEmergencyResourceFromPersonCommand::documentChange() const
{
    return DocumentChange::emergencyResource().updated(m_resourceId);
}
