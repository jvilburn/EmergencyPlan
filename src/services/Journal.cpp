#include "Journal.h"
#include "CommandRegistry.h"

#include <QJsonArray>

bool JournalEntry::operator==(const JournalEntry& other) const
{
    return sequence == other.sequence
        && command == other.command;
}

void Journal::append(const Command& command)
{
    ++m_lastSequence;
    m_entries.append(JournalEntry{m_lastSequence, CommandRegistry::toJson(command)});
}

void Journal::clear()
{
    m_entries.clear();
}

void Journal::dropLanded(qint64 mark)
{
    m_entries.removeIf([mark](const JournalEntry& entry) { return entry.sequence <= mark; });
    m_lastSequence = qMax(m_lastSequence, mark);
}

QList<CommandDescription> Journal::descriptions() const
{
    QList<CommandDescription> descriptions;
    for (const JournalEntry& entry : m_entries)
    {
        descriptions.append(CommandRegistry::descriptionOf(entry.command));
    }
    return descriptions;
}

QJsonObject Journal::toJson() const
{
    QJsonObject json;
    json["lastSequence"] = m_lastSequence;
    if (!m_entries.isEmpty())
    {
        QJsonArray entries;
        for (const JournalEntry& entry : m_entries)
        {
            QJsonObject entryJson;
            entryJson["sequence"] = entry.sequence;
            entryJson["command"] = entry.command;
            entries.append(entryJson);
        }
        json["entries"] = entries;
    }
    return json;
}

Journal Journal::fromJson(JsonReader json)
{
    Journal journal;
    journal.m_lastSequence = json.integer("lastSequence");
    const QList<JsonReader> entryReaders = json.optionalObjects("entries");
    for (JsonReader entry : entryReaders)
    {
        qint64 sequence = entry.integer("sequence");
        JsonReader command = entry.object("command");
        CommandRegistry::fromJson(command);
        entry.finish();
        journal.m_entries.append(JournalEntry{sequence, command.source()});
    }
    json.finish();
    return journal;
}

bool Journal::operator==(const Journal& other) const
{
    return m_entries == other.m_entries
        && m_lastSequence == other.m_lastSequence;
}
