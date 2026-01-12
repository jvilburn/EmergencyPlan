#pragma once

#include <QObject>
#include <vector>
#include "Command.h"
#include "DocumentChange.h"

class Document;

class CommandHistory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY dirtyChanged)

public:
    explicit CommandHistory(QObject* parent = nullptr);

    // Execute a command and add to history
    void execute(CommandPtr command, Document& document);

    // Undo/Redo operations - return DocumentChange describing what changed
    DocumentChange undo(Document& document);
    DocumentChange redo(Document& document);

    // State queries
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    // Description for menu items
    QString undoDescription() const;
    QString redoDescription() const;

    // Dirty state tracking
    void markSaved();
    bool isDirty() const;

    // Clear all history
    void clear();

    // Stack sizes for debugging/display
    int undoCount() const { return static_cast<int>(m_undoStack.size()); }
    int redoCount() const { return static_cast<int>(m_redoStack.size()); }

signals:
    void canUndoChanged();
    void canRedoChanged();
    void dirtyChanged();

private:
    void emitStateChanges(bool couldUndo, bool couldRedo, bool wasDirty);

    std::vector<CommandPtr> m_undoStack;
    std::vector<CommandPtr> m_redoStack;
    size_t m_savedPosition = 0;
};
