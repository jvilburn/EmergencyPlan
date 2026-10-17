#include "CommandRegistry.h"
#include "FamilyCommands.h"

const QList<CommandType>& CommandRegistry::types()
{
    static const QList<CommandType> types = {
        {UpdateFamilyCommand::TypeName, &UpdateFamilyCommand::fromJson},
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

Description CommandRegistry::descriptionOf(const QJsonObject& json)
{
    return Description::fromJson(json["description"].toObject());
}
