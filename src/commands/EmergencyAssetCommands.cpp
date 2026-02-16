#include "EmergencyAssetCommands.h"
#include "Document.h"

// ============================================================================
// AddEmergencyAssetCommand
// ============================================================================

AddEmergencyAssetCommand::AddEmergencyAssetCommand(const EmergencyAsset& asset)
    : m_asset(asset)
{
}

void AddEmergencyAssetCommand::execute(Document& document)
{
    document.addEmergencyAsset(m_asset);
}

void AddEmergencyAssetCommand::undo(Document& document)
{
    document.removeEmergencyAsset(m_asset.id());
}

QString AddEmergencyAssetCommand::description() const
{
    return QObject::tr("Add Asset \"%1\"").arg(m_asset.name());
}

DocumentChange AddEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().added(m_asset.id());
}

// ============================================================================
// UpdateEmergencyAssetCommand
// ============================================================================

UpdateEmergencyAssetCommand::UpdateEmergencyAssetCommand(const EmergencyAsset& oldAsset, const EmergencyAsset& newAsset)
    : m_oldAsset(oldAsset)
    , m_newAsset(newAsset)
{
}

void UpdateEmergencyAssetCommand::execute(Document& document)
{
    document.updateEmergencyAsset(m_newAsset);
}

void UpdateEmergencyAssetCommand::undo(Document& document)
{
    document.updateEmergencyAsset(m_oldAsset);
}

QString UpdateEmergencyAssetCommand::description() const
{
    return QObject::tr("Update Asset \"%1\"").arg(m_oldAsset.name());
}

DocumentChange UpdateEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_newAsset.id());
}

// ============================================================================
// DeleteEmergencyAssetCommand
// ============================================================================

DeleteEmergencyAssetCommand::DeleteEmergencyAssetCommand(const EmergencyAsset& asset)
    : m_asset(asset)
{
}

void DeleteEmergencyAssetCommand::execute(Document& document)
{
    document.removeEmergencyAsset(m_asset.id());
}

void DeleteEmergencyAssetCommand::undo(Document& document)
{
    document.addEmergencyAsset(m_asset);
}

QString DeleteEmergencyAssetCommand::description() const
{
    return QObject::tr("Delete Asset \"%1\"").arg(m_asset.name());
}

DocumentChange DeleteEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().removed(m_asset.id());
}

// ============================================================================
// AssignEmergencyAssetToPersonCommand
// ============================================================================

AssignEmergencyAssetToPersonCommand::AssignEmergencyAssetToPersonCommand(const EmergencyAssetId& assetId, const PersonId& personId)
    : m_assetId(assetId)
    , m_personId(personId)
{
}

void AssignEmergencyAssetToPersonCommand::execute(Document& document)
{
    auto asset = document.findEmergencyAssetById(m_assetId);
    if (asset)
    {
        asset->addPerson(m_personId);
        document.updateEmergencyAsset(*asset);
    }
}

void AssignEmergencyAssetToPersonCommand::undo(Document& document)
{
    auto asset = document.findEmergencyAssetById(m_assetId);
    if (asset)
    {
        asset->removePerson(m_personId);
        document.updateEmergencyAsset(*asset);
    }
}

QString AssignEmergencyAssetToPersonCommand::description() const
{
    return QObject::tr("Assign Asset to Person");
}

DocumentChange AssignEmergencyAssetToPersonCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_assetId);
}

// ============================================================================
// UnassignEmergencyAssetFromPersonCommand
// ============================================================================

UnassignEmergencyAssetFromPersonCommand::UnassignEmergencyAssetFromPersonCommand(const EmergencyAssetId& assetId, const PersonId& personId)
    : m_assetId(assetId)
    , m_personId(personId)
{
}

void UnassignEmergencyAssetFromPersonCommand::execute(Document& document)
{
    auto asset = document.findEmergencyAssetById(m_assetId);
    if (asset)
    {
        asset->removePerson(m_personId);
        document.updateEmergencyAsset(*asset);
    }
}

void UnassignEmergencyAssetFromPersonCommand::undo(Document& document)
{
    auto asset = document.findEmergencyAssetById(m_assetId);
    if (asset)
    {
        asset->addPerson(m_personId);
        document.updateEmergencyAsset(*asset);
    }
}

QString UnassignEmergencyAssetFromPersonCommand::description() const
{
    return QObject::tr("Remove Asset from Person");
}

DocumentChange UnassignEmergencyAssetFromPersonCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_assetId);
}
