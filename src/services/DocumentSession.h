#pragma once

#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentFile.h"
#include "DocumentHeader.h"
#include "DocumentRecords.h"
#include "InviteLink.h"
#include "ServerConnection.h"
#include "ServerVersion.h"
#include "SessionTimers.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

/// Why a local file did not open. What the user can act on is small.
enum class LoadFailure
{
    None,
    NotFound,
    NeedsNewerVersion,
    Refused         ///< failed a check; the reason comes with it
};

enum class SyncState
{
    Local,              ///< not shared
    UpToDate,
    PendingUpload,
    Unreachable,
    ServerFault         ///< the server's answer did not check out; the local file is untouched
};

/// One open ward: its document, header, data key, and sync state.
///
/// Saving and publishing are separate operations on separate clocks. An accepted
/// edit restarts the Save timer, whose timeout writes the local file; a written
/// save of a shared ward then publishes. Every write reaches disk through this one
/// path, so the read-only check and the debounce cover all of them.
///
/// The base version is the whole mechanism: it tells "the server moved" from "I
/// moved" with certainty rather than by timestamp. The base moves whenever the
/// local file takes in a server version, and only then - a fast-forward, a silent
/// merge, a resolve, an accepted publish, and the registration that shares the
/// ward. "Unpublished changes" is exact: the local header's save counter is above
/// the base copy's, which the record holds.
///
/// Every save applies the sealing-set rule against the version it builds on: when
/// the roster now lacks a device or an invite that version sealed to, the data key
/// is fresh, so whoever was taken out holds no key to anything written afterwards.
/// The roster changes by a roster edit on some device - an invite made, a person or
/// device removed, a role changed - or by a join, which a new device makes once; this
/// session takes others' changes in like any version.
///
/// One document operation runs at a time - a head check, a fetch, a publish, the
/// registration that shares the ward, or a leave - so a save made during a publish
/// publishes after it, and a head check asked for meanwhile runs when it ends. The
/// lease runs beside them.
///
/// Destroying a session closes it: a scheduled save is written, the lease is
/// released, and every reply still on its way is dropped. Call flush() first to
/// learn whether that save succeeded.
class DocumentSession : public QObject
{
    Q_OBJECT

public:
    enum class Edit
    {
        Directory,
        Response,
        Roster          ///< who has access: never refused, merged entry by entry
    };

    enum class Refusal
    {
        None,
        DirectoryReadOnly,
        RequiresUpdate
    };

    enum class Resolution
    {
        KeepMine,
        KeepTheirs
    };

    enum class Download
    {
        Done,
        Unreachable,
        NeedsNewerVersion,
        AccessLost,
        ServerFault,
        WriteFailed         ///< it validated, but could not be written on this device
    };

    /// How joining a ward through an invite went.
    enum class Join
    {
        Done,
        Unreachable,
        UnknownWard,        ///< the server has no such document
        InviteUsed,         ///< this device could not take the invite's place, and the current version is not its
        NeedsNewerVersion,
        ServerFault,        ///< the version did not check out, could not be sealed here, or was refused; the reason says why
        WriteFailed
    };

    struct Context
    {
        DeviceIdentity identity;
        DocumentRecords& records;
        ServerConnection& server;
    };

    struct Conflict
    {
        QString writerName;         ///< the roster's name for the device that published their version
        QDateTime writeTime;
    };

    static constexpr int SaveDelayMilliseconds = 2 * 1000;
    static constexpr int HeadCheckMilliseconds = 5 * 60 * 1000;
    static constexpr int EmergencyHeadCheckMilliseconds = 30 * 1000;

    /// A third of the server's two-minute lease.
    static constexpr int HeartbeatMilliseconds = 40 * 1000;

    ~DocumentSession() override;

    /// A new, unshared ward, written at once: a first data key, sealed only to this
    /// device.
    static std::unique_ptr<DocumentSession> create(const Context& context,
                                                   std::unique_ptr<SessionTimers> timers,
                                                   const QString& documentId,
                                                   const DocumentPaths& paths,
                                                   QString* errorMessage);

    /// Opens the local file, which must exist; a shared ward then takes the lease
    /// and checks the server. A refusal's reason goes to refusal.
    static std::unique_ptr<DocumentSession> open(const Context& context,
                                                 std::unique_ptr<SessionTimers> timers,
                                                 const QString& documentId,
                                                 const DocumentPaths& paths,
                                                 LoadFailure& failure,
                                                 QString* refusal);

    /// What a download reports: how it went, and on Done the document it wrote.
    using Downloaded = std::function<void(Download outcome, const std::optional<Document>& document)>;

    /// Fetches a shared ward whose local file is missing or refused, writes it and
    /// its base once it validates, and reports; the caller then opens it. done is
    /// called only while receiver is alive.
    static void download(const Context& context,
                         const QString& documentId,
                         const DocumentPaths& paths,
                         QObject* receiver,
                         Downloaded done);

    /// Joins the ward an invite link names: fetches the current version as the invite,
    /// validates it as a download is validated, and writes the next version with this
    /// device in the invite's place, signed by the invite, which the current header
    /// names. The current version is then downloaded as this device and written as
    /// the local file and base, so a link opened again after a lost answer or a failed
    /// write finishes the same way. The caller then adds the ward to its library and
    /// opens it. done is called only while receiver is alive; reason is set for
    /// ServerFault, and document on Done.
    using Joined =
        std::function<void(Join outcome, const QString& reason, const std::optional<Document>& document)>;
    static void join(const Context& context,
                     const InviteLink& link,
                     const DocumentPaths& paths,
                     QObject* receiver,
                     Joined done);

    const Document& document() const { return m_document; }
    const DocumentHeader& header() const { return m_header; }
    const QString& documentId() const { return m_documentId; }
    bool isShared() const;

    /// Applies change unless refused, then schedules a save. A refusal changes
    /// nothing and never calls change.
    Refusal edit(Edit kind, const std::function<void(Document&)>& change);

    /// Writes a scheduled save now. A save that fails stays scheduled.
    bool flush(QString* errorMessage);

    /// Flushes, then publishes without waiting on the retry timer.
    void publishNow();

    /// A head check now, or as soon as the operation in flight ends.
    void checkServer();

    bool isDirectoryReadOnly() const;
    bool requiresUpdate() const { return m_requiresUpdate; }

    /// Unshared, or this device holds the lease. Automatic directory updates -
    /// geocoding, unit lookups - run only while it is true.
    bool mayWriteDirectory() const;

    SyncState syncState() const { return m_syncState; }

    /// Why the state is ServerFault, for the sync indicator; empty otherwise.
    const QString& faultReason() const { return m_faultReason; }

    void setEmergencyActive(bool active);

    /// The latest head the server answered, if any: its version, who published it,
    /// and the lease holder.
    const std::optional<ServerContract::Head>& lastHead() const { return m_head; }

    /// Registers the ward under its existing id, sealed to this device alone.
    void share();

    /// This person as the roster lists it; nullopt when the roster does not.
    std::optional<Holder> self() const;

    /// Publishes a version without this person, built from what is in memory, and
    /// once it lands finishes as Left: the local file is never written without this
    /// device. leaveFailed says why it did not land: the last person cannot leave,
    /// and another server call may be in flight.
    void leave();

    /// The conflict waiting for a choice, if any. Head checks and publishing wait
    /// with it, so it can be asked about again until it is settled.
    std::optional<Conflict> pendingConflict() const;

    /// Settles the pending conflict. False, with the reason, when the ward could not
    /// be written; a conflict whose local file was not written is still pending.
    bool resolve(Resolution resolution, QString* errorMessage);

signals:
    /// The in-memory document was replaced from outside an edit - a fast-forward,
    /// a merge, a resolve: undo no longer applies.
    void documentReplaced();

    /// A head check answered; lastHead() holds what it said.
    void headChanged();

    void syncStateChanged(SyncState state);

    /// isDirectoryReadOnly() or mayWriteDirectory() changed.
    void readOnlyChanged();

    void requiresUpdateChanged();
    void conflictRaised(const Conflict& conflict);

    /// A local write; the one modal failure.
    void saveFailed(const QString& errorMessage);

    void shared();
    void shareFailed(ServerContract::ServerError error);

    /// Files and record gone; the session is finished.
    void accessLost(const ServerVersion::LostAccess& lost);

    /// The version that leaves was refused, or could not be built.
    void leaveFailed(ServerContract::ServerError error);

    /// Who has access changed: a roster edit, a version taken in, or a save that
    /// rotated the key.
    void rosterChanged();

private:
    DocumentSession(const Context& context,
                    std::unique_ptr<SessionTimers> timers,
                    const QString& documentId,
                    const DocumentPaths& paths,
                    const DocumentFile::Sealed& sealed,
                    const DataKey& dataKey,
                    const Document& document);

    // --- Saving ---

    void onTimer(SessionTimers::Timer timer);
    bool writeSave(QString* errorMessage);

    /// The name the roster gives a device's person, for a conflict's other side.
    QString nameOf(const Document& document, const Fingerprint& device) const;

    /// Seals document on previous with dataKey and writes it as the local file.
    /// The session's header, key, and document follow only on success.
    bool writeLocal(const Document& document, const DocumentHeader& previous, const DataKey& dataKey);

    /// writeLocal with the sealing-set rule applied against previous first: every
    /// save, and every write of a merged or resolved document.
    bool writeResealed(const Document& document, const DocumentHeader& previous, const DataKey& dataKey);

    /// This device no longer holds the ward: the files and record go, accessLost is
    /// emitted, and the session does nothing further.
    void finishLost(const ServerVersion::LostAccess& lost);

    /// The exact bytes of a server version as the local file.
    bool writeLocal(const ServerVersion::Fetched& theirs);

    /// Takes document as the in-memory one, and says so when it came from outside
    /// an edit.
    void install(const Document& document);

    bool hasUnpublishedChanges() const;

    // --- The server ---

    void startShared();
    void onHead(const ServerContract::Result<ServerContract::Head>& head);
    ServerVersion::LostAccess whyLost(const Fingerprint& publisher, const QSet<QString>& recipients) const;
    void fetchTheirs();
    void onFetched(const ServerVersion::Fetched& theirs);
    void publish();
    void onPublished(const ServerContract::Result<qint64>& result, const DocumentFile::Sealed& published);
    void scheduleRetry();
    void onLeft(const ServerContract::Result<qint64>& result);
    void onRegistered(const ServerContract::Result<qint64>& result, const DocumentFile::Sealed& registered);
    void finishOperation();

    // --- Taking in their version ---

    void adopt(const ServerVersion::Fetched& theirs);
    void fastForward(const ServerVersion::Fetched& theirs);
    void mergeWith(const ServerVersion::Fetched& theirs, const Document& merged);
    void raiseConflict(const ServerVersion::Fetched& theirs);
    bool moveBase(const ServerVersion::Fetched& theirs, QString* errorMessage);

    // --- The lease ---

    void takeLease();
    void onLease(const ServerContract::Result<ServerContract::Lease>& lease);
    void setLease(bool held, bool heldElsewhere);

    // --- State ---

    void setSyncState(SyncState state);
    void setFault(const QString& reason);
    void settleSyncState();
    void setRequiresUpdate(bool requiresUpdate);
    void restartHeadCheck();
    void stopEveryTimer();

    Context m_context;
    std::unique_ptr<SessionTimers> m_timers;
    QString m_documentId;
    DocumentPaths m_paths;

    Document m_document;
    DocumentHeader m_header;
    DataKey m_dataKey;

    std::optional<ServerVersion::BaseCopy> m_base;
    std::optional<ServerVersion::Fetched> m_conflict;
    std::optional<ServerContract::Head> m_head;

    SyncState m_syncState = SyncState::Local;
    QString m_faultReason;
    bool m_saveScheduled = false;
    bool m_busy = false;
    bool m_publishAgain = false;
    bool m_checkAgain = false;
    int m_retryDelay = SessionTimers::FirstRetryMilliseconds;
    bool m_holdsLease = false;
    bool m_leaseHeldElsewhere = false;
    bool m_leaseRequested = false;
    bool m_requiresUpdate = false;
    bool m_emergencyActive = false;
    bool m_lost = false;
};
