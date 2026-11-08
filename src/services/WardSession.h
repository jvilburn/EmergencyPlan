#pragma once

#include "AutomaticUpdates.h"
#include "Command.h"
#include "DeviceState.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Identity.h"
#include "Journal.h"
#include "WardPayload.h"
#include "WardStore.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <optional>

class Geocoder;
class UnitLookup;

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
    CommandDescription description;  ///< the description of the command that ran
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

/// How ending an emergency went.
enum class EndResult
{
    Ended,         ///< the report was written, and the emergency removed
    AlreadyEnded,  ///< no emergency is in progress, so nothing was written
    ReportFailed   ///< the report could not be written, so the emergency stays
};

enum class OpenStatus
{
    Opened,
    Missing,     ///< this device holds no file for the ward
    CannotRead,  ///< the bytes were read and do not open as this ward on this device
    Newer,       ///< a newer Ward Planning wrote the file
    Fault        ///< the bytes could not be read at all; nothing changed, and opening again retries
};

struct OpenResult
{
    OpenStatus status;
    QString problem;  ///< what went wrong, for the log, when not Opened
};

/// How a ward's local file reads on this device.
struct LocalRead
{
    OpenResult result;
    std::optional<WardPayload> payload;  ///< when result is Opened
    QByteArray dataKey;                  ///< when result is Opened
};

/// One open ward: its document, journal and undo stack, and the commit step and replay
/// that change them. Every change to the document or this device's state is saved, the two
/// together in one write of the ward's local file. It owns the ward's AutomaticUpdates,
/// which change the same document outside the journal. Holds no global state, so a test
/// creates as many sessions as it needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    /// identity is this device's, which opens its local files, and store holds them.
    /// geocoder and unitLookup answer the automatic updates. The session owns none of them.
    WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_device.journal; }

    /// The ward, its landed marks and this device's state, as a save writes them.
    WardPayload payload() const { return WardPayload{m_document, m_landed, m_device}; }
    AutomaticUpdates& automaticUpdates() { return m_automaticUpdates; }

    /// Creates a ward holding content under a fresh document id and data key, and saves it.
    /// Its access list, whatever content held, names one user: userName, an admin under a new
    /// user id, with this device alone, named deviceName. The journal, the undo and redo
    /// stacks, the rest of this device's state and the failed list start empty.
    void createWard(const Document& content, const QString& userName, const QString& deviceName);

    /// Opens the ward this device holds under document: its document with its access list,
    /// its landed marks and this device's state come from its local file, and the undo stack
    /// and the failed list start empty. Anything but Opened leaves the session as it was and
    /// writes nothing.
    OpenResult openWard(const DocumentId& document);

    /// How document's local file in store reads for identity: opened, or why not.
    static LocalRead readLocal(const WardStore& store, const Identity& identity, const DocumentId& document);

    const DocumentId& documentId() const { return m_documentId; }

    /// True while the latest save failed: memory holds changes the file does not.
    bool savePending() const { return m_savePending; }

    /// Saves again when a save is pending; true when nothing is left unsaved.
    bool retryPendingSave();

    /// The commit step: the command's rule, then execute, then the journal and a save. A
    /// command that applies empties the redo stack, and enters the undo stack when it is an
    /// UndoableCommand. One that conflicts or is refused changes nothing and enters neither.
    /// An empty command is nothing to do and changes nothing. A command that applies is
    /// handed to the automatic updates, which start a run when it gives them work; an undo
    /// or redo is handed on the same way.
    CommitResult commit(CommandPtr command);

    /// Builds the inverse of the command on top of the undo stack and runs it through the
    /// commit step. When the inverse applies, the command moves to the redo stack; otherwise
    /// it stays on the undo stack.
    CommitResult undo();

    /// Runs the command on top of the redo stack again through the commit step. When it
    /// applies, it moves back to the undo stack; otherwise it stays on the redo stack.
    CommitResult redo();

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    QString undoHeading() const;
    QString redoHeading() const;

    /// Ends the emergency in progress: reads it, writes its report to reportPath, and
    /// commits EndEmergencyCommand with the emergency as the report was written from.
    /// Nothing runs between the read and the commit, so the commit applies.
    EndResult endEmergency(const QString& reportPath);

    /// Replays the journal onto version. With the journal empty, version becomes the
    /// document as it stands. Otherwise each entry is rebuilt from its serialised form and
    /// put through the rule and execute, in order, on a copy of version, and when every
    /// entry applies the copy becomes the document. The first entry that conflicts or is
    /// refused abandons the replay: version becomes the document as it stands, the journal
    /// and the undo and redo stacks are emptied, and the result's toRedo holds the
    /// abandoned changes as text, which nothing in the session keeps. Each outcome is saved.
    /// A replay starts no automatic update.
    ReplayResult replayOnto(const Document& version);

signals:
    /// The document changed, through a command, a replay, an open or an automatic update.
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

    /// A save failed after the one before it succeeded; told once until a save succeeds again.
    void saveFailed(const QString& problem);

private:
    CommitResult run(const Command& command);

    /// The replay replayOnto runs before it saves.
    ReplayResult replay(const Document& version);

    /// One write of the document and this device's state together, so a crash cannot leave
    /// one without the other. The file is sealed to exactly the access list, under the data
    /// key the ward already has.
    void save();

    /// The journal's changes as text for the user to redo: what stopped the replay, then
    /// each entry's heading and lines in order, with the entry at index stopped marked.
    QString toRedoText(int stopped, const CommitResult& stop) const;

    void replaceDocument(const Document& document);
    void clearUndoStack();

    const Identity* m_identity;
    WardStore* m_store;
    DocumentId m_documentId;
    QByteArray m_dataKey;
    Document m_document;
    QHash<QByteArray, qint64> m_landed;
    DeviceState m_device;
    bool m_savePending = false;
    QList<UndoableCommandPtr> m_undoStack;
    QList<UndoableCommandPtr> m_redoStack;
    AutomaticUpdates m_automaticUpdates;  ///< declared after m_document, which it works on

    static constexpr int UndoLimit = 100;
};
