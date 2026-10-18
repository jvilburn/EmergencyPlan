#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "DocumentChange.h"

class Document;

/// What running a command did to the version in front of it. A command whose result
/// already holds answers Applied too: nothing downstream tells the two apart.
enum class CommandOutcome
{
    Applied,
    Conflicted
};

/// What the user did, in their words: a heading, and a line per value set. Rendered
/// through tr() when the command is built and stored with it, so it stays right after
/// the things it names are renamed or deleted.
class CommandDescription
{
public:
    CommandDescription() = default;
    CommandDescription(const QString& heading, const QStringList& lines);

    const QString& heading() const { return m_heading; }
    const QStringList& lines() const { return m_lines; }

    /// "Undo: <heading>" over the same lines, for an inverse.
    CommandDescription undone() const;

    QJsonObject toJson() const;
    static CommandDescription fromJson(const QJsonObject& json);

    bool operator==(const CommandDescription& other) const;
    bool operator!=(const CommandDescription& other) const { return !(*this == other); }

private:
    QString m_heading;
    QStringList m_lines;
};

class Command;
using CommandPtr = std::shared_ptr<const Command>;

/// A write to the document, run the same way the first time and on replay.
///
/// A command is immutable once constructed. Its start condition (what the user saw), its
/// result and its description are fixed by the constructor; execute reads the document in
/// front of it and leaves the command as it was.
class Command
{
public:
    virtual ~Command() = default;

    /// The name this type has in the journal and in CommandRegistry.
    virtual QString type() const = 0;

    /// Applies when the start condition holds or the result already holds, and answers
    /// Applied. Otherwise answers Conflicted and leaves the document untouched.
    virtual CommandOutcome execute(Document& document) const = 0;

    /// The wording of the rule that refuses this command on this document, or an empty
    /// string when no rule does. Content commands have no rules.
    virtual QString refusal(const Document& document) const;

    /// This command's own fields. CommandRegistry adds the type and the description.
    virtual QJsonObject toJson() const = 0;

    /// What a run changes, for targeted view updates and cache rebuilds.
    virtual DocumentChange documentChange() const = 0;

    const CommandDescription& description() const { return m_description; }

protected:
    explicit Command(const CommandDescription& description);

private:
    CommandDescription m_description;
};

class UndoableCommand;
using UndoableCommandPtr = std::shared_ptr<const UndoableCommand>;

/// A command undo can take back. The undo and redo stacks hold only these; a command that
/// cannot be taken back derives from Command and never enters them.
class UndoableCommand : public Command
{
public:
    /// The command that takes this one back, described by description().undone(). Never
    /// empty.
    virtual UndoableCommandPtr inverse() const = 0;

protected:
    explicit UndoableCommand(const CommandDescription& description);
};
