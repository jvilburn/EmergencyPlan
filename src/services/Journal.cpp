#include "Journal.h"
#include "CommandRegistry.h"

void Journal::append(const Command& command)
{
    ++m_lastSequence;
    m_entries.append(JournalEntry{m_lastSequence, CommandRegistry::toJson(command)});
}

void Journal::clear()
{
    m_entries.clear();
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
