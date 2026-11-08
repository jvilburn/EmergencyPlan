#pragma once

#include "Command.h"
#include "JsonReader.h"

#include <QJsonObject>
#include <QList>

/// One command this device ran, serialised once when it entered the journal and kept as
/// it is.
struct JournalEntry
{
    qint64 sequence;
    QJsonObject command;

    bool operator==(const JournalEntry& other) const;
};

/// This device's commands that it has not yet seen land, in the order they were made. Each
/// entry takes the next sequence number from a counter kept with the journal.
class Journal
{
public:
    Journal() = default;

    /// Appends the command's serialised form under the next sequence number.
    void append(const Command& command);

    /// Empties the journal. The sequence counter keeps its value, so entries made after
    /// number above every entry made before.
    void clear();

    /// Drops the entries numbered at or below mark, this device's landed mark in a version it
    /// takes, and raises the counter to mark when it is below, so the next entry numbers above
    /// everything that landed.
    void dropLanded(qint64 mark);

    /// The last sequence number given to an entry.
    qint64 lastSequence() const { return m_lastSequence; }

    bool isEmpty() const { return m_entries.isEmpty(); }
    const QList<JournalEntry>& entries() const { return m_entries; }

    /// The entries' descriptions, in order.
    QList<CommandDescription> descriptions() const;

    QJsonObject toJson() const;

    /// The journal a local file holds, each entry's command kept as it was written. Each
    /// entry's command is read as the command it names, so an entry this app cannot rebuild
    /// refuses the file.
    static Journal fromJson(JsonReader json);

    bool operator==(const Journal& other) const;

private:
    QList<JournalEntry> m_entries;
    qint64 m_lastSequence = 0;
};
