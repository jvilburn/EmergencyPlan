#include "CommandRegistry.h"
#include "AddressCoordinatesCommands.h"
#include "EmergencyAssetCommands.h"
#include "FamilyCommands.h"
#include "TeamCommands.h"

const QList<CommandType>& CommandRegistry::types()
{
    static const QList<CommandType> types = {
        {UpdateFamilyCommand::TypeName, &UpdateFamilyCommand::fromJson},
        {SetAddressCoordinatesCommand::TypeName, &SetAddressCoordinatesCommand::fromJson},
        {RevertAddressCoordinatesCommand::TypeName, &RevertAddressCoordinatesCommand::fromJson},
        {AddTeamCommand::TypeName, &AddTeamCommand::fromJson},
        {UpdateTeamCommand::TypeName, &UpdateTeamCommand::fromJson},
        {DeleteTeamCommand::TypeName, &DeleteTeamCommand::fromJson},
        {AddEmergencyAssetCommand::TypeName, &AddEmergencyAssetCommand::fromJson},
        {UpdateEmergencyAssetCommand::TypeName, &UpdateEmergencyAssetCommand::fromJson},
        {DeleteEmergencyAssetCommand::TypeName, &DeleteEmergencyAssetCommand::fromJson},
    };
    return types;
}

QJsonObject CommandRegistry::toJson(const Command& command)
{
    QJsonObject json;
    json["type"] = command.type();
    json["description"] = command.description().toJson();
    json["fields"] = command.toJson();
    return json;
}

CommandPtr CommandRegistry::fromJson(const QJsonObject& json)
{
    QString name = json["type"].toString();
    for (const CommandType& type : types())
    {
        if (type.name == name)
        {
            return type.fromJson(json["fields"].toObject(), descriptionOf(json));
        }
    }
    Q_ASSERT_X(false, "CommandRegistry::fromJson", "a journal entry names a type the registry lacks");
    return nullptr;
}

CommandDescription CommandRegistry::descriptionOf(const QJsonObject& json)
{
    return CommandDescription::fromJson(json["description"].toObject());
}
