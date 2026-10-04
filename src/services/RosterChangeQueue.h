#pragma once

#include "AccessControl.h"
#include "DeviceIdentity.h"
#include "DocumentRecords.h"
#include "DocumentSession.h"
#include "RosterChange.h"
#include "ServerConnection.h"
#include "ServerVersion.h"
#include "SessionTimers.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

enum class RosterChangeOutcome
{
    Published,
    AlreadyDone,        ///< the server's version already reflects it
    NoLongerApplies,    ///< refused against the server's version; not retried
    ValidationFailed    ///< the server's version did not validate, or refused ours; this change stops
};

/// Every change to who has access goes through here - grant, link and unlink,
/// removal, role changes, and leaving - and none needs the lease. A roster change
/// is never blocked by whoever holds the lease, so a member cannot hold off their
/// own removal and a thief cannot hold off an unlink.
///
/// The queue works on the server's version directly. For each change, one at a
/// time: fetch and validate the server's version, apply the matching
/// AccessControl call to it with this device as writer, seal it on their header,
/// and upload it conditional on their version. One change per publish, because a
/// version may advance the key generation by only one.
///
/// Changes are kept in each ward's DocumentRecord until the server accepts them or
/// they are dropped, so a queue made over existing records resumes them. A ward
/// that cannot publish now waits for the next Retry while the others go ahead.
///
/// The ward open in the window is the queue's to keep current: after a publish to
/// it the session checks the server, and when this device is off it the session
/// finishes removed. Every other ward's local file catches up by the ordinary
/// route the next time it opens.
class RosterChangeQueue : public QObject
{
    Q_OBJECT

public:
    RosterChangeQueue(const DeviceIdentity& identity,
                      DocumentRecords& records,
                      ServerConnection& server,
                      std::unique_ptr<SessionTimers> timers,
                      std::function<DocumentPaths(const QString&)> pathsFor,
                      QObject* parent);

    /// The ward open in the window, or nullptr.
    void setOpenSession(DocumentSession* session);

    void enqueue(const QString& documentId, const RosterChange& change);
    void enqueueForEveryWard(const RosterChange& change, const QStringList& documentIds);
    QList<RosterChange> pending(const QString& documentId) const;

    /// Shares the open ward, with this person's pending Link changes for it joining
    /// the first header. An unshared ward keeps its changes pending until then;
    /// only a Link can reach one.
    void shareOpenWard();

signals:
    void pendingChanged();

    /// The server accepted a change to this ward.
    void published(const QString& documentId);

    void changeFinished(const QString& documentId, const RosterChange& change, RosterChangeOutcome outcome);

    /// This device is off the ward - proven removed, or its own leave accepted -
    /// and its files and record are gone.
    void removed(const QString& documentId, bool left);

private:
    void onTimer(SessionTimers::Timer timer);

    /// Starts the oldest change of the first shared ward that has one and is not
    /// waiting for the next Retry.
    void processNext();

    void onFetched(const QString& documentId, const RosterChange& change, const ServerVersion::Fetched& theirs);
    void onUploaded(const QString& documentId, const RosterChange& change, const ServerContract::Result<qint64>& result);
    void onRemovalChecked(const QString& documentId, const RosterChange& change, const ServerVersion::Fetched& proof);
    void onShared();

    /// Whether their version already has the change.
    bool alreadyDone(const RosterChange& change, const AccessRoster& roster) const;
    std::optional<AccessChange> apply(const RosterChange& change, const ServerVersion::Fetched& theirs) const;

    void finish(const QString& documentId, const RosterChange& change, RosterChangeOutcome outcome);
    void forgetWard(const QString& documentId, const Fingerprint& remover, bool left);

    /// This ward waits for the next Retry; the others go ahead.
    void defer(const QString& documentId);

    DeviceIdentity m_identity;
    DocumentRecords& m_records;
    ServerConnection& m_server;
    std::unique_ptr<SessionTimers> m_timers;
    std::function<DocumentPaths(const QString&)> m_pathsFor;
    QPointer<DocumentSession> m_openSession;

    bool m_busy = false;
    QSet<QString> m_deferred;
    bool m_retryWaiting = false;
    int m_retryDelay = SessionTimers::FirstRetryMilliseconds;
};
