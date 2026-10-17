#include "WardSession.h"
#include "CommandRegistry.h"

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal};
    }
    if (command.execute(document) == Outcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString()};
}

}  // namespace

WardSession::WardSession(QObject* parent)
    : QObject(parent)
{
}

void WardSession::open(const Document& document)
{
    m_journal = Journal();
    m_toRedo.clear();
    clearUndoStack();
    replaceDocument(document);
}

CommitResult WardSession::commit(CommandPtr command)
{
    CommitResult result = run(*command);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }

    m_redoStack.clear();
    CommandPtr inverse = command->inverse();
    if (inverse)
    {
        m_undoStack.push_back(UndoEntry{std::move(command), std::move(inverse)});
        if (static_cast<int>(m_undoStack.size()) > UndoLimit)
        {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    emit undoStackChanged();
    return result;
}

CommitResult WardSession::undo()
{
    Q_ASSERT(canUndo());
    CommitResult result = run(*m_undoStack.back().inverse);
    if (result.status == CommitStatus::Applied)
    {
        m_redoStack.push_back(std::move(m_undoStack.back()));
        m_undoStack.pop_back();
        emit undoStackChanged();
    }
    return result;
}

CommitResult WardSession::redo()
{
    Q_ASSERT(canRedo());
    CommitResult result = run(*m_redoStack.back().command);
    if (result.status == CommitStatus::Applied)
    {
        m_undoStack.push_back(std::move(m_redoStack.back()));
        m_redoStack.pop_back();
        emit undoStackChanged();
    }
    return result;
}

QString WardSession::undoHeading() const
{
    if (m_undoStack.empty())
    {
        return QString();
    }
    return m_undoStack.back().command->description().heading();
}

QString WardSession::redoHeading() const
{
    if (m_redoStack.empty())
    {
        return QString();
    }
    return m_redoStack.back().command->description().heading();
}

ReplayStatus WardSession::replayOnto(const Document& version)
{
    if (m_journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayStatus::FastForwarded;
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
            abandon(version, i, result);
            return ReplayStatus::Abandoned;
        }
    }
    replaceDocument(replayed);
    return ReplayStatus::Replayed;
}

void WardSession::dismissToRedo()
{
    m_toRedo.clear();
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

void WardSession::abandon(const Document& version, int clashed, const CommitResult& stop)
{
    m_toRedo.append(RedoGroup{m_journal.descriptions(), clashed, stop});
    m_journal.clear();
    clearUndoStack();
    replaceDocument(version);
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
