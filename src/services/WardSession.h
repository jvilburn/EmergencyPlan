#pragma once

#include "Command.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Journal.h"

#include <QList>
#include <QObject>
#include <QString>

/// How the commit step ended for one command.
enum class CommitStatus
{
    Applied,
    Conflicted,
    Refused,
    NothingToDo  ///< there was no command, as from an edit that changed nothing
};

struct CommitResult
{
    CommitStatus status;
    QString refusal;  ///< the rule's wording, when status is Refused
};

/// How a replay onto a version ended.
enum class ReplayStatus
{
    FastForwarded,  ///< the journal was empty, and the version was taken as it stands
    Replayed,       ///< every entry applied on top of the version
    Abandoned       ///< one entry stopped it, and the version was taken as it stands
};

struct ReplayResult
{
    ReplayStatus status;
    QString toRedo;  ///< when status is Abandoned, the abandoned changes as plain text
};

/// One open ward: its document, journal and undo stack, and the commit step and replay
/// that change them. Holds no global state, so a test creates as many sessions as it
/// needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    explicit WardSession(QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_journal; }

    /// Opens a ward: its document replaces the open one by assignment, and the journal and
    /// the undo and redo stacks start empty.
    void open(const Document& document);

    /// The commit step: the command's rule, then execute, then the journal. A command that
    /// applies empties the redo stack, and enters the undo stack when it is an
    /// UndoableCommand. One that conflicts or is refused changes nothing and enters neither.
    /// An empty command is nothing to do and changes nothing.
    CommitResult commit(CommandPtr command);

    /// Builds the inverse of the command on top of the undo stack and runs it through the
    /// commit step. When the inverse applies, the command moves to the redo stack;
    /// otherwise it stays on the undo stack.
    CommitResult undo();

    /// Runs the command on top of the redo stack again through the commit step. When it
    /// applies, it moves back to the undo stack; otherwise it stays on the redo stack.
    CommitResult redo();

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    QString undoHeading() const;
    QString redoHeading() const;

    /// Replays the journal onto version. With the journal empty, version becomes the
    /// document as it stands. Otherwise each entry is rebuilt from its serialised form and
    /// put through the rule and execute, in order, on a copy of version, and when every
    /// entry applies the copy becomes the document. The first entry that conflicts or is
    /// refused abandons the replay: version becomes the document as it stands, the journal
    /// and the undo and redo stacks are emptied, and the result's toRedo holds the
    /// abandoned changes as text, which nothing in the session keeps.
    ReplayResult replayOnto(const Document& version);

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

private:
    CommitResult run(const Command& command);

    /// The journal's changes as text for the user to redo: what stopped the replay, then
    /// each entry's heading and lines in order, with the entry at index stopped marked.
    QString toRedoText(int stopped, const CommitResult& stop) const;

    void replaceDocument(const Document& document);
    void clearUndoStack();

    Document m_document;
    Journal m_journal;
    QList<UndoableCommandPtr> m_undoStack;
    QList<UndoableCommandPtr> m_redoStack;

    static constexpr int UndoLimit = 100;
};
