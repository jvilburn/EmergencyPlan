#include "EmergencyAssetCommands.h"
#include "Document.h"
#include "FieldChangeJson.h"

#include <QObject>

// ----------------------------------------------------------------------------
// AddEmergencyAssetCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr AddEmergencyAssetCommand::fromNew(const EmergencyAsset& asset, const Document& document)
{
    CommandDescription description(QObject::tr("New asset: %1").arg(asset.name()),
                                   describePeopleChange(IdSetChange<PersonId>(asset.personIds(), {}), document));
    return std::make_shared<AddEmergencyAssetCommand>(asset, description);
}

AddEmergencyAssetCommand::AddEmergencyAssetCommand(const EmergencyAsset& asset, const CommandDescription& description)
    : UndoableCommand(description)
    , m_asset(asset)
{
}

CommandOutcome AddEmergencyAssetCommand::execute(Document& document) const
{
    if (document.findEmergencyAssetById(m_asset.id()).has_value())
    {
        return CommandOutcome::Applied;
    }
    if (!everyPersonExists(m_asset.personIds(), document))
    {
        return CommandOutcome::Conflicted;
    }
    document.setEmergencyAsset(m_asset);
    return CommandOutcome::Applied;
}

UndoableCommandPtr AddEmergencyAssetCommand::inverse() const
{
    return std::make_shared<DeleteEmergencyAssetCommand>(m_asset, description().undone());
}

QJsonObject AddEmergencyAssetCommand::toJson() const
{
    QJsonObject json;
    json["asset"] = m_asset.toJson();
    return json;
}

DocumentChange AddEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_asset.id());
}

CommandPtr AddEmergencyAssetCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyAsset asset = EmergencyAsset::fromJson(json.object("asset"));
    json.finish();
    return std::make_shared<AddEmergencyAssetCommand>(asset, description);
}

// ----------------------------------------------------------------------------
// UpdateEmergencyAssetCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr UpdateEmergencyAssetCommand::fromEdit(const EmergencyAsset& opened,
                                                         const EmergencyAsset& saved,
                                                         const Document& document)
{
    std::optional<FieldChange<QString>> name = changeBetween(opened.name(), saved.name());
    IdSetChange<PersonId> people = IdSetChange<PersonId>::between(opened.personIds(), saved.personIds());
    if (!name.has_value() && people.isEmpty())
    {
        return nullptr;
    }

    QStringList lines;
    if (name.has_value())
    {
        lines.append(QObject::tr("name: %1").arg(name->after()));
    }
    lines.append(describePeopleChange(people, document));
    CommandDescription description(QObject::tr("%1 asset").arg(opened.name()), lines);
    return std::make_shared<UpdateEmergencyAssetCommand>(opened.id(), name, people, description);
}

UpdateEmergencyAssetCommand::UpdateEmergencyAssetCommand(const EmergencyAssetId& assetId,
                                                         const std::optional<FieldChange<QString>>& name,
                                                         const IdSetChange<PersonId>& people,
                                                         const CommandDescription& description)
    : UndoableCommand(description)
    , m_assetId(assetId)
    , m_name(name)
    , m_people(people)
{
}

CommandOutcome UpdateEmergencyAssetCommand::execute(Document& document) const
{
    std::optional<EmergencyAsset> asset = document.findEmergencyAssetById(m_assetId);
    if (!asset.has_value()
        || !changeHolds(m_name, asset->name())
        || !everyPersonExists(m_people.added(), document))
    {
        return CommandOutcome::Conflicted;
    }

    EmergencyAsset updated = *asset;
    updated.setName(changedValue(m_name, asset->name()));
    updated.setPersonIds(m_people.appliedTo(asset->personIds()));
    document.setEmergencyAsset(updated);
    return CommandOutcome::Applied;
}

UndoableCommandPtr UpdateEmergencyAssetCommand::inverse() const
{
    return std::make_shared<UpdateEmergencyAssetCommand>(m_assetId,
                                                         changeSwapped(m_name),
                                                         m_people.swapped(),
                                                         description().undone());
}

QJsonObject UpdateEmergencyAssetCommand::toJson() const
{
    QJsonObject json;
    json["assetId"] = m_assetId.toString();
    writeChange(json, "name", m_name);
    json["people"] = peopleChangeToJson(m_people);
    return json;
}

DocumentChange UpdateEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_assetId);
}

CommandPtr UpdateEmergencyAssetCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyAssetId assetId = json.id<EmergencyAssetId>("assetId");
    std::optional<FieldChange<QString>> name = readChange<QString>(json, "name");
    IdSetChange<PersonId> people = peopleChangeFromJson(json.object("people"));
    json.finish();
    return std::make_shared<UpdateEmergencyAssetCommand>(assetId, name, people, description);
}

// ----------------------------------------------------------------------------
// DeleteEmergencyAssetCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr DeleteEmergencyAssetCommand::fromSeen(const EmergencyAsset& asset, const Document& document)
{
    CommandDescription description(QObject::tr("Delete asset: %1").arg(asset.name()),
                                   describePeopleChange(IdSetChange<PersonId>({}, asset.personIds()), document));
    return std::make_shared<DeleteEmergencyAssetCommand>(asset, description);
}

DeleteEmergencyAssetCommand::DeleteEmergencyAssetCommand(const EmergencyAsset& asset, const CommandDescription& description)
    : UndoableCommand(description)
    , m_asset(asset)
{
}

CommandOutcome DeleteEmergencyAssetCommand::execute(Document& document) const
{
    std::optional<EmergencyAsset> asset = document.findEmergencyAssetById(m_asset.id());
    if (!asset.has_value())
    {
        return CommandOutcome::Applied;
    }
    if (*asset != m_asset)
    {
        return CommandOutcome::Conflicted;
    }
    document.removeEmergencyAsset(m_asset.id());
    return CommandOutcome::Applied;
}

UndoableCommandPtr DeleteEmergencyAssetCommand::inverse() const
{
    return std::make_shared<AddEmergencyAssetCommand>(m_asset, description().undone());
}

QJsonObject DeleteEmergencyAssetCommand::toJson() const
{
    QJsonObject json;
    json["asset"] = m_asset.toJson();
    return json;
}

DocumentChange DeleteEmergencyAssetCommand::documentChange() const
{
    return DocumentChange::emergencyAsset().updated(m_asset.id());
}

CommandPtr DeleteEmergencyAssetCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyAsset asset = EmergencyAsset::fromJson(json.object("asset"));
    json.finish();
    return std::make_shared<DeleteEmergencyAssetCommand>(asset, description);
}
