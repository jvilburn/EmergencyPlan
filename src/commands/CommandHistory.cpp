#include "CommandHistory.h"
#include "Document.h"

CommandHistory::CommandHistory(QObject* parent)
    : QObject(parent)
{
}

void CommandHistory::execute(CommandPtr command, Document& document)
{
    bool couldUndo = canUndo();
    bool couldRedo = canRedo();
    bool wasDirty = isDirty();

    // Execute the command
    command->execute(document);

    // Add to undo stack
    m_undoStack.push_back(std::move(command));

    // Clear redo stack (new action invalidates redo history)
    m_redoStack.clear();

    emitStateChanges(couldUndo, couldRedo, wasDirty);
}

void CommandHistory::undo(Document& document)
{
    if (!canUndo())
    {
        return;
    }

    bool couldUndo = canUndo();
    bool couldRedo = canRedo();
    bool wasDirty = isDirty();

    // Pop from undo stack
    CommandPtr command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Undo the command
    command->undo(document);

    // Push to redo stack
    m_redoStack.push_back(std::move(command));

    emitStateChanges(couldUndo, couldRedo, wasDirty);
}

void CommandHistory::redo(Document& document)
{
    if (!canRedo())
    {
        return;
    }

    bool couldUndo = canUndo();
    bool couldRedo = canRedo();
    bool wasDirty = isDirty();

    // Pop from redo stack
    CommandPtr command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Re-execute the command
    command->execute(document);

    // Push to undo stack
    m_undoStack.push_back(std::move(command));

    emitStateChanges(couldUndo, couldRedo, wasDirty);
}

QString CommandHistory::undoDescription() const
{
    if (m_undoStack.empty())
    {
        return QString();
    }
    return m_undoStack.back()->description();
}

QString CommandHistory::redoDescription() const
{
    if (m_redoStack.empty())
    {
        return QString();
    }
    return m_redoStack.back()->description();
}

void CommandHistory::markSaved()
{
    bool wasDirty = isDirty();
    m_savedPosition = m_undoStack.size();

    if (wasDirty != isDirty())
    {
        emit dirtyChanged();
    }
}

bool CommandHistory::isDirty() const
{
    return m_undoStack.size() != m_savedPosition;
}

void CommandHistory::clear()
{
    bool couldUndo = canUndo();
    bool couldRedo = canRedo();
    bool wasDirty = isDirty();

    m_undoStack.clear();
    m_redoStack.clear();
    m_savedPosition = 0;

    emitStateChanges(couldUndo, couldRedo, wasDirty);
}

void CommandHistory::emitStateChanges(bool couldUndo, bool couldRedo, bool wasDirty)
{
    if (couldUndo != canUndo())
    {
        emit canUndoChanged();
    }
    if (couldRedo != canRedo())
    {
        emit canRedoChanged();
    }
    if (wasDirty != isDirty())
    {
        emit dirtyChanged();
    }
}
