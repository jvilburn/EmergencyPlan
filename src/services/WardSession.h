#pragma once

#include "Command.h"
#include "Coordinates.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Journal.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

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

/// One open ward: its document, journal and undo stack, and the commit step and replay
/// that change them, and the automatic updates (geocoding and ward and stake lookup), whose
/// results change the document outside the journal. Holds no global state, so a test
/// creates as many sessions as it needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    /// geocoder and unitLookup answer the automatic updates. The session does not own them.
    WardSession(Geocoder* geocoder, UnitLookup* unitLookup, QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_journal; }

    /// Opens a ward: its document replaces the open one by assignment, and the journal and
    /// the undo and redo stacks start empty. Automatic updates under way carry on, and apply
    /// to the document as it stands when they answer.
    void open(const Document& document);

    /// The commit step: the command's rule, then execute, then the journal. A command that
    /// applies empties the redo stack, and enters the undo stack when it is an
    /// UndoableCommand. One that conflicts or is refused changes nothing and enters neither.
    /// An empty command is nothing to do and changes nothing. A command that applies and
    /// gives a family an address the batch would request starts the automatic updates; so
    /// does an undo or redo that does.
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
    /// abandoned changes as text, which nothing in the session keeps. A replay starts no
    /// automatic update.
    ReplayResult replayOnto(const Document& version);

    /// Starts the automatic updates. The batch requests, once each, every address some
    /// family has that the table of address coordinates lacks, skipping addresses with no
    /// street, those on the failed list and those it has requested already, and applies when
    /// its last request answers; a batch with nothing to request applies at once. Units are
    /// looked up one at a time: each stake a ward names that the document lacks, and each
    /// ward lacking its details, skipping units on the failed list.
    void runAutomaticUpdates();

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

    /// The batch's requests answered, out of those it has made.
    void geocodingProgress(int completed, int total);

    /// The last request of a batch that made any has answered.
    void geocodingFinished();

    /// An automatic update changed the document. Nothing enters the journal for it.
    void automaticUpdateApplied();

private slots:
    void onGeocoded(const QString& address, const Coordinates& coordinates);
    void onGeocodeFailed(const QString& address);
    void onWardFound(const QString& wardUnitNumber, const Ward& ward);
    void onStakeFound(const QString& stakeUnitNumber, const Stake& stake);
    void onLookupFailed(const QString& unitNumber, const QString& error);

private:
    CommitResult run(const Command& command);

    /// The journal's changes as text for the user to redo: what stopped the replay, then
    /// each entry's heading and lines in order, with the entry at index stopped marked.
    QString toRedoText(int stopped, const CommitResult& stop) const;

    void replaceDocument(const Document& document);
    void clearUndoStack();

    /// True when the batch would request address: it has a street, the table lacks it, and
    /// it is not on the failed list or requested already.
    bool needsCoordinates(const Address& address) const;

    /// One request of the batch has answered; the last one applies the batch.
    void onBatchRequestAnswered();

    /// Each result adds the entry for its address where the table has none, then every
    /// entry no family's address uses is dropped.
    void applyBatch();

    /// Starts the next unit lookup, unless one is under way or none is needed.
    void lookUpNextUnit();

    /// The first stake, by ward unit number, that a ward names and the document lacks and
    /// that is not on the failed list; empty when there is none.
    QString nextStakeToLookUp() const;

    /// The first ward, by unit number, lacking its details and not on the failed list;
    /// empty when there is none.
    QString nextWardToLookUp() const;

    Document m_document;
    Journal m_journal;
    QList<UndoableCommandPtr> m_undoStack;
    QList<UndoableCommandPtr> m_redoStack;

    Geocoder* m_geocoder;
    UnitLookup* m_unitLookup;
    QSet<QString> m_failed;                  ///< addresses and unit numbers that did not resolve
    QSet<QString> m_pending;                 ///< the batch's addresses not yet answered
    QHash<QString, Coordinates> m_resolved;  ///< the batch's results, held by address
    int m_batchTotal = 0;                    ///< the requests the batch has made
    QString m_unitInFlight;                  ///< the unit being looked up; empty when none

    static constexpr int UndoLimit = 100;
};
