#pragma once

#include "Command.h"

#include <QJsonObject>
#include <QList>
#include <QString>

/// A command type the journal can rebuild.
struct CommandType
{
    QString name;
    CommandPtr (*fromJson)(JsonReader json, const CommandDescription& description);
};

/// The command types, and the serialised form a command takes in the journal.
class CommandRegistry
{
public:
    /// Every command type, each listed once by its TypeName.
    static const QList<CommandType>& types();

    /// A command as the journal holds it: its type, its description and its fields.
    static QJsonObject toJson(const Command& command);

    /// The command a journal entry holds, rebuilt as the command it was. For a type this
    /// app lacks, it fails json and answers an empty pointer.
    static CommandPtr fromJson(JsonReader json);

    /// The description a journal entry holds, read without rebuilding the command. The entry
    /// was read strictly or made by this app.
    static CommandDescription descriptionOf(const QJsonObject& json);
};
