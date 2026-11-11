#pragma once

#include "AccessList.h"
#include "AutomaticUpdates.h"
#include "Command.h"
#include "DeviceState.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Identity.h"
#include "Journal.h"
#include "WardFile.h"
#include "WardHeader.h"
#include "WardPayload.h"
#include "WardStore.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
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
    QString toRedo;               ///< when status is Abandoned, the abandoned changes as plain text
    QStringList stillHaveAccess;  ///< when status is Abandoned, each user an abandoned removal named whom the version holds
};

/// Whether an update file was taken.
enum class TakeStatus
{
    Taken,                  ///< its version is now the one this device holds, with the journal replayed on it
    NotNewer,               ///< it does not rank above the version this device holds
    OtherWard,              ///< it is a version of a ward other than the open one
    UnknownWriter,          ///< the access list this device holds names no device or invite as its writer
    NotNamed,               ///< it names no recipient, or others and not this device: the ward is no longer this device's
    InviteUsedOrWithdrawn,  ///< it names neither this device nor its invite while the journal still holds this device's join: the join is refused, and the ward is no longer this device's
    Newer,                  ///< a newer Ward Planning wrote it
    Unreadable              ///< it fails another check; problem says which
};

/// How taking an update file ended. NotNamed and InviteUsedOrWithdrawn end this device's share
/// of the ward; every status but those and Taken changes nothing. When it was taken, a replay
/// that is Replayed put this device's own changes on top, which the other devices lack until
/// it sends an update.
struct TakeResult
{
    TakeStatus status;
    QString problem;      ///< what went wrong, for the log, when Unreadable
    ReplayResult replay;  ///< when Taken
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
    VersionRank version;                 ///< when result is Opened: the version its header names
};

/// What an invitation file holds, as this device reads it.
enum class InvitationStatus
{
    Joinable,          ///< this device can join through its invite
    AlreadyHasAccess,  ///< this device holds the ward as its own
    NotAnInvitation,   ///< the file is not an invitation, or its version fails a check; problem says which
    Newer              ///< a newer Ward Planning wrote it
};

/// An invitation's ward, read for the user to confirm by the ward's own name.
struct InvitedWard
{
    InvitationStatus status;
    QString problem;                     ///< when NotAnInvitation
    DocumentId document;                 ///< when Joinable or AlreadyHasAccess
    QByteArray seed;                     ///< when Joinable
    std::optional<WardRead> version;     ///< when Joinable: the version it carries, read
    std::optional<AccessInvite> invite;  ///< when Joinable: the open invite it joins through
    QString wardName;                    ///< when Joinable or AlreadyHasAccess
};

/// How making an invite ended.
struct InviteResult
{
    CommitResult commit;
    QByteArray invite;  ///< the invite's fingerprint, when the commit applied
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

    /// The version this device holds: the one it last took or wrote, which its local file's
    /// header names.
    const VersionRank& version() const { return m_version; }

    /// The ward, its landed marks and this device's state, as a save writes them.
    WardPayload payload() const { return WardPayload{m_document, m_landed, m_device}; }
    AutomaticUpdates& automaticUpdates() { return m_automaticUpdates; }

    const QByteArray& deviceFingerprint() const { return m_identity->fingerprint(); }

    /// Creates a ward holding content under a fresh document id and data key, and saves it as
    /// its version 1, written by this device. Its access list, whatever content held, names
    /// one user: userName, an admin under a new user id, with this device alone, named
    /// deviceName. The journal, the undo and redo stacks, the rest of this device's state and
    /// the failed list start empty.
    void createWard(const Document& content, const QString& userName, const QString& deviceName);

    /// Opens the ward this device holds under document: its document with its access list,
    /// the version it holds, its landed marks and this device's state come from its local
    /// file, and the undo stack and the failed list start empty. Anything but Opened leaves
    /// the session as it was and writes nothing.
    OpenResult openWard(const DocumentId& document);

    /// How document's local file in store reads for identity: opened, or why not.
    static LocalRead readLocal(const WardStore& store, const Identity& identity, const DocumentId& document);

    const DocumentId& documentId() const { return m_documentId; }

    /// True while the latest save failed: memory holds changes the file does not.
    bool savePending() const { return m_savePending; }

    /// Saves again when a save is pending; true when nothing is left unsaved.
    bool retryPendingSave();

    /// True once a version this device took left it out: the ward is no longer this device's,
    /// and its copy is read-only.
    bool isNoLongerShared() const { return m_device.noLongerShared; }

    /// True when journal holds an entry above device's own mark in landed: a change no version
    /// held with those marks carries.
    static bool changesToShare(const Journal& journal, const QHash<QByteArray, qint64>& landed,
                               const QByteArray& device);

    /// You have changes to share: changes no version this device holds carries.
    bool hasChangesToShare() const { return changesToShare(m_device.journal, m_landed, m_identity->fingerprint()); }

    /// The commit step: the command's rule, then execute, then the journal and a save. A
    /// command that applies empties the redo stack, and enters the undo stack when it is an
    /// UndoableCommand. One that conflicts or is refused changes nothing and enters neither.
    /// An empty command is nothing to do and changes nothing. Every command is refused while
    /// the ward is no longer this device's. A command that applies is handed to the automatic
    /// updates, which start a run when it gives them work; an undo or redo is handed on the
    /// same way.
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

    /// Writes the next version as an update file, signed by its writer, and this device then
    /// holds that version: see nextVersion. The journal keeps its entries until a version this
    /// device takes carries them.
    QByteArray writeUpdate();

    /// Takes the version an update file carries, checked in this order: its format; that it
    /// is a version of the open ward; that its signature verifies under the verify key the
    /// access list this device holds gives its writer; that it ranks above the version this
    /// device holds; that it names this device, or, for a device that joined through an
    /// invite, that invite, which it then reads with; that the data key unseals and the payload
    /// reads as a version whose access list the header matches; and that it descends from the
    /// copy this device holds, by sharing a key with it: the key either is under, or one either
    /// names among its former keys. A version that names neither ends this device's share: see
    /// becomeNoLongerShared. A version that names the invite and not this device comes from a
    /// member who has not taken the join, so the join runs on it again (JoinCommand::rejoin)
    /// before anything replays, and enters the journal, so it goes out with the next update.
    /// Taking it drops the journal entries its landed mark for this device covers, raising the
    /// counter to that mark, replays the rest onto it as replayOnto does, and makes it the
    /// version this device holds, under its data key, with its landed marks and its recipients.
    /// Then it saves, and starts a run of the automatic updates for what the version lacks.
    TakeResult takeUpdate(const QByteArray& file);

    /// Makes an invite under a fresh seed, which this device's state keeps while the access
    /// list names the invite, and puts InviteCommand through the commit step: for user when it
    /// is set, otherwise for another of this device's user's devices. A refused invite keeps no
    /// seed.
    InviteResult invite(const std::optional<InvitedUser>& user);

    /// The fingerprints of the open invites this device made and keeps the seed of: those it
    /// can write an invitation for.
    QSet<QByteArray> invitesMadeHere() const;

    /// An invitation for invite, one of invitesMadeHere: its seed and the version nextVersion
    /// would write, which names the invite while it is open. This device does not hold that
    /// version, so its changes stay to share until it sends an update.
    QByteArray writeInvitation(const QByteArray& invite) const;

    /// What file holds as an invitation, read by this device: its line, then its version read
    /// as the invite its seed gives. It is AlreadyHasAccess when this device holds the version's
    /// ward as its own: its copy opens and is still this device's. The caller then opens that
    /// copy with openWard. Otherwise it is Joinable. Changes nothing.
    InvitedWard readInvitation(const QByteArray& file) const;

    /// Joins the ward invited carries, which readInvitation found Joinable: its version becomes
    /// this device's copy as a first download, replacing any copy held before, with nothing of
    /// that copy kept - under the version's key, with its landed marks and its recipients, and
    /// an empty journal counting on from this device's mark in it - and JoinCommand runs on it
    /// through the commit step, naming this device deviceName and, for a user invite, its new
    /// user userName. This device keeps the invite's seed for as long as it holds the ward.
    void join(const InvitedWard& invited, const QString& userName, const QString& deviceName);

    /// The name of this device's user in the open ward.
    QString ownUserName() const;

    /// Renames this device to deviceName in every ward the store holds that is still this
    /// device's and names it otherwise: the open ward through this session's commit step,
    /// each other ward through a session opened on it here. Each rename is a command in its
    /// ward's journal.
    void renameDeviceEverywhere(const QString& deviceName);

    /// Renames this device's user to userName in every ward the store holds that is still
    /// this device's and names them otherwise, as renameDeviceEverywhere does.
    void renameUserEverywhere(const QString& userName);

    /// When this device's user in the open ward is named other than yourName, renames the
    /// user to that name in every other ward this device holds that names them otherwise, and
    /// answers the name, which is Your name from then on. Otherwise answers empty.
    QString followUserName(const QString& yourName);

signals:
    /// The document changed, through a command, a replay, an open or an automatic update.
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

    /// A save failed after the one before it succeeded; told once until a save succeeds again.
    void saveFailed(const QString& problem);

private:
    /// The next version this device writes, and what holding it changes.
    struct NextVersion
    {
        QByteArray bytes;  ///< in a ward file's framing, with no device part
        VersionRank version;
        QByteArray dataKey;
        QList<QByteArray> formerKeys;
        QHash<QByteArray, qint64> landed;
        QSet<QByteArray> recipients;
        Identity writer;  ///< the header's writer, which signs the update file too
    };

    CommitResult run(const Command& command);

    /// The replay replayOnto runs before it saves.
    ReplayResult replay(const Document& version);

    /// One write of the document and this device's state together, so a crash cannot leave
    /// one without the other. The file is sealed to exactly the access list, under the data
    /// key the ward already has, and its header names the version this device holds. The seed
    /// of an invite the access list no longer names leaves the state as it is written.
    void save();

    /// The journal's changes as text for the user to redo: what stopped the replay, then
    /// each entry's heading and lines in order, with the entry at index stopped marked.
    QString toRedoText(int stopped, const CommitResult& stop) const;

    /// The name of each user a removal in the journal names whom access holds.
    QStringList stillHavingAccess(const AccessList& access) const;

    /// One above the version this device holds, written by writer(), holding the document,
    /// its access list and the landed marks, with this device's set to its journal's last
    /// sequence number and those of devices the access list no longer holds dropped. When
    /// the access list has lost anyone the version this device holds seals to, it rotates: a
    /// fresh data key, and the hash of the key this device holds appended to the former keys.
    NextVersion nextVersion() const;

    /// Who writes this device's next version: the invite it joined through, while the version
    /// it holds does not name this device, since a member who has not taken the join lists the
    /// invite and not the device; otherwise this device.
    Identity writer() const;

    /// True while the journal holds this device's join: no version it took carries it yet.
    bool joinUnlanded() const;

    /// version read as this device, or as the invite this device joined through when the
    /// version names that and not this device.
    WardRead readVersion(const QByteArray& version) const;

    /// version, read, becomes this device's copy of document as it stands: its key, landed
    /// marks and recipients, a fresh device state whose counter starts at this device's mark
    /// in it, and empty undo stacks and failed list. Saves nothing.
    void takeFirstDownload(const DocumentId& document, const WardRead& version);

    /// The ward is no longer this device's: unsent changes are dropped, the undo stacks
    /// cleared, the copy kept read-only with the flag saved, so a device reopened still knows.
    void becomeNoLongerShared();

    /// Commits, in every ward the store holds, the command rename builds from that ward's
    /// access list for this device and name: the open ward through this session, each other
    /// ward through a session opened on it here. A ward that names it so already gets no
    /// command, and one that is no longer this device's refuses it.
    void renameEverywhere(CommandPtr (*rename)(const QByteArray&, const AccessList&, const QString&),
                          const QString& name);

    void replaceDocument(const Document& document);
    void clearUndoStack();

    const Identity* m_identity;
    WardStore* m_store;
    Geocoder* m_geocoder;
    UnitLookup* m_unitLookup;
    DocumentId m_documentId;
    VersionRank m_version;
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
