#include "WardSession.h"
#include "CommandRegistry.h"

#include <QStringList>

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal, command.description()};
    }
    if (command.execute(document) == CommandOutcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString(), command.description()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString(), command.description()};
}

}  // namespace

WardSession::WardSession(QObject* parent)
    : QObject(parent)
{
}

void WardSession::open(const Document& document)
{
    m_journal = Journal();
    clearUndoStack();
    replaceDocument(document);
}

CommitResult WardSession::commit(CommandPtr command)
{
    if (!command)
    {
        return CommitResult{CommitStatus::NothingToDo, QString(), CommandDescription()};
    }
    CommitResult result = run(*command);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }

    m_redoStack.clear();
    UndoableCommandPtr undoable = std::dynamic_pointer_cast<const UndoableCommand>(command);
    if (undoable)
    {
        m_undoStack.append(undoable);
        if (m_undoStack.size() > UndoLimit)
        {
            m_undoStack.removeFirst();
        }
    }
    emit undoStackChanged();
    return result;
}

CommitResult WardSession::undo()
{
    Q_ASSERT(canUndo());
    CommitResult result = run(*m_undoStack.last()->inverse());
    if (result.status == CommitStatus::Applied)
    {
        m_redoStack.append(m_undoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

CommitResult WardSession::redo()
{
    Q_ASSERT(canRedo());
    CommitResult result = run(*m_redoStack.last());
    if (result.status == CommitStatus::Applied)
    {
        m_undoStack.append(m_redoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

QString WardSession::undoHeading() const
{
    if (m_undoStack.isEmpty())
    {
        return QString();
    }
    return m_undoStack.last()->description().heading();
}

QString WardSession::redoHeading() const
{
    if (m_redoStack.isEmpty())
    {
        return QString();
    }
    return m_redoStack.last()->description().heading();
}

ReplayResult WardSession::replayOnto(const Document& version)
{
    if (m_journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayResult{ReplayStatus::FastForwarded, QString()};
    }

    Document replayed = version;
    replayed.onDocumentChanged(DocumentChange::full());
    const QList<JournalEntry>& entries = m_journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(entries[i].command);
        CommitResult result = attempt(*command, replayed);
        if (result.status != CommitStatus::Applied)
        {
            QString toRedo = toRedoText(i, result);
            m_journal.clear();
            clearUndoStack();
            replaceDocument(version);
            return ReplayResult{ReplayStatus::Abandoned, toRedo};
        }
    }
    replaceDocument(replayed);
    return ReplayResult{ReplayStatus::Replayed, QString()};
}

CommitResult WardSession::run(const Command& command)
{
    CommitResult result = attempt(command, m_document);
    if (result.status == CommitStatus::Applied)
    {
        m_journal.append(command);
        emit documentChanged(command.documentChange());
    }
    return result;
}

QString WardSession::toRedoText(int stopped, const CommitResult& stop) const
{
    QStringList text;
    if (stop.status == CommitStatus::Refused)
    {
        text.append(stop.refusal);
    }
    else
    {
        text.append(tr("Your changes could not be applied, because the ward changed first."));
    }
    text.append(tr("These need doing again:"));

    QList<CommandDescription> descriptions = m_journal.descriptions();
    for (int i = 0; i < descriptions.size(); ++i)
    {
        text.append(QString());
        if (i == stopped)
        {
            text.append(tr("%1 (stopped here)").arg(descriptions[i].heading()));
        }
        else
        {
            text.append(descriptions[i].heading());
        }
        for (const QString& line : descriptions[i].lines())
        {
            text.append(QString("- %1").arg(line));
        }
    }
    return text.join("\n") + "\n";
}

void WardSession::replaceDocument(const Document& document)
{
    m_document = document;
    m_document.onDocumentChanged(DocumentChange::full());
    emit documentChanged(DocumentChange::full());
}

void WardSession::clearUndoStack()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStackChanged();
}
