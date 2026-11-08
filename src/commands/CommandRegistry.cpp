#include "CommandRegistry.h"
#include "AddressCoordinatesCommands.h"
#include "EmergencyAssetCommands.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "TeamCommands.h"

const QList<CommandType>& CommandRegistry::types()
{
    static const QList<CommandType> types = {
        {UpdateFamilyCommand::TypeName, &UpdateFamilyCommand::fromJson},
        {DeleteFamilyCommand::TypeName, &DeleteFamilyCommand::fromJson},
        {RestoreFamilyCommand::TypeName, &RestoreFamilyCommand::fromJson},
        {SetAddressCoordinatesCommand::TypeName, &SetAddressCoordinatesCommand::fromJson},
        {RevertAddressCoordinatesCommand::TypeName, &RevertAddressCoordinatesCommand::fromJson},
        {AddTeamCommand::TypeName, &AddTeamCommand::fromJson},
        {UpdateTeamCommand::TypeName, &UpdateTeamCommand::fromJson},
        {DeleteTeamCommand::TypeName, &DeleteTeamCommand::fromJson},
        {AddEmergencyAssetCommand::TypeName, &AddEmergencyAssetCommand::fromJson},
        {UpdateEmergencyAssetCommand::TypeName, &UpdateEmergencyAssetCommand::fromJson},
        {DeleteEmergencyAssetCommand::TypeName, &DeleteEmergencyAssetCommand::fromJson},
        {StartEmergencyCommand::TypeName, &StartEmergencyCommand::fromJson},
        {EndEmergencyCommand::TypeName, &EndEmergencyCommand::fromJson},
        {RestoreEmergencyCommand::TypeName, &RestoreEmergencyCommand::fromJson},
        {AddContactAttemptCommand::TypeName, &AddContactAttemptCommand::fromJson},
        {DeleteContactAttemptCommand::TypeName, &DeleteContactAttemptCommand::fromJson},
        {AddTaskCommand::TypeName, &AddTaskCommand::fromJson},
        {UpdateTaskCommand::TypeName, &UpdateTaskCommand::fromJson},
        {DeleteTaskCommand::TypeName, &DeleteTaskCommand::fromJson},
        {AddTaskCategoryCommand::TypeName, &AddTaskCategoryCommand::fromJson},
        {ImportWardDirectoryCommand::TypeName, &ImportWardDirectoryCommand::fromJson},
        {ImportMinisteringCommand::TypeName, &ImportMinisteringCommand::fromJson},
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

CommandPtr CommandRegistry::fromJson(JsonReader json)
{
    QString name = json.string("type");
    CommandDescription description = CommandDescription::fromJson(json.object("description"));
    JsonReader fields = json.object("fields");
    json.finish();
    for (const CommandType& type : types())
    {
        if (type.name == name)
        {
            return type.fromJson(fields, description);
        }
    }
    json.fail("type", "is not a command this app knows");
    return nullptr;
}

CommandDescription CommandRegistry::descriptionOf(const QJsonObject& json)
{
    return CommandDescription::fromJson(JsonReader(json["description"].toObject()));
}
