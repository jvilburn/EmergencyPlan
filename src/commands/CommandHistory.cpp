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

    // Execute the command
    command->execute(document);

    // Add to undo stack
    m_undoStack.push_back(std::move(command));

    // Clear redo stack (new action invalidates redo history)
    m_redoStack.clear();

    emitStateChanges(couldUndo, couldRedo);
}

DocumentChange CommandHistory::undo(Document& document)
{
    if (!canUndo())
    {
        return DocumentChange::full();
    }

    bool couldUndo = canUndo();
    bool couldRedo = canRedo();

    // Pop from undo stack
    CommandPtr command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Get DocumentChange before undoing
    DocumentChange change = command->documentChange();

    // Undo the command
    command->undo(document);

    // Push to redo stack
    m_redoStack.push_back(std::move(command));

    emitStateChanges(couldUndo, couldRedo);

    return change;
}

DocumentChange CommandHistory::redo(Document& document)
{
    if (!canRedo())
    {
        return DocumentChange::full();
    }

    bool couldUndo = canUndo();
    bool couldRedo = canRedo();

    // Pop from redo stack
    CommandPtr command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Get DocumentChange before executing
    DocumentChange change = command->documentChange();

    // Re-execute the command
    command->execute(document);

    // Push to undo stack
    m_undoStack.push_back(std::move(command));

    emitStateChanges(couldUndo, couldRedo);

    return change;
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

void CommandHistory::clear()
{
    bool couldUndo = canUndo();
    bool couldRedo = canRedo();

    m_undoStack.clear();
    m_redoStack.clear();

    emitStateChanges(couldUndo, couldRedo);
}

void CommandHistory::emitStateChanges(bool couldUndo, bool couldRedo)
{
    if (couldUndo != canUndo())
    {
        emit canUndoChanged();
    }
    if (couldRedo != canRedo())
    {
        emit canRedoChanged();
    }
}
