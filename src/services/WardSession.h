#pragma once

#include "Command.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Journal.h"

#include <QList>
#include <QObject>
#include <QString>
#include <vector>

/// How the commit step ended for one command.
enum class CommitStatus
{
    Applied,
    Conflicted,
    Refused
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

/// The descriptions of one abandoned journal, kept for the user to work from.
struct RedoGroup
{
    QList<Description> descriptions;
    int clashed;        ///< index in descriptions of the entry that stopped the replay
    CommitResult stop;  ///< Conflicted, or Refused with the rule's wording
};

/// One open ward: its document, journal, undo stack and to-redo list, and the commit step
/// and replay that change them. Holds no global state, so a test creates as many sessions
/// as it needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    explicit WardSession(QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_journal; }
    const QList<RedoGroup>& toRedo() const { return m_toRedo; }

    /// Opens a ward: its document replaces the open one by assignment, and the journal, the
    /// to-redo list and the undo stack start empty.
    void open(const Document& document);

    /// The commit step: the command's rule, then execute, then the journal. A command that
    /// applies clears the redo side, and enters the undo stack when it has an inverse. One
    /// that conflicts or is refused changes nothing and enters neither.
    CommitResult commit(CommandPtr command);

    /// Runs the inverse of the latest undoable command through the commit step. When it
    /// applies, the command moves to the redo side; otherwise it stays where it is.
    CommitResult undo();

    /// Runs the latest undone command again through the commit step. When it applies, the
    /// command moves back to the undo side.
    CommitResult redo();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    QString undoHeading() const;
    QString redoHeading() const;

    /// Replays the journal onto version. An empty journal takes the version as it stands.
    /// Otherwise each entry is rebuilt from its serialised form and put through the rule and
    /// execute, in order, on a copy of the version; when every entry applies, the copy
    /// becomes the document. One conflict or refusal abandons the whole replay: the version
    /// is taken as it stands, the journal's descriptions become a group on the to-redo list,
    /// and the journal and the undo stack are emptied.
    ReplayStatus replayOnto(const Document& version);

    /// Empties the to-redo list, once the user has dismissed it.
    void dismissToRedo();

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

private:
    /// A command on the undo or redo side, with the inverse built when it was committed.
    struct UndoEntry
    {
        CommandPtr command;
        CommandPtr inverse;
    };

    CommitResult run(const Command& command);
    void abandon(const Document& version, int clashed, const CommitResult& stop);
    void replaceDocument(const Document& document);
    void clearUndoStack();

    Document m_document;
    Journal m_journal;
    std::vector<UndoEntry> m_undoStack;
    std::vector<UndoEntry> m_redoStack;
    QList<RedoGroup> m_toRedo;

    static constexpr int UndoLimit = 100;
};
