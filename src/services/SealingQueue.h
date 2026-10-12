#pragma once

#include "DeviceIdentity.h"
#include "DocumentRecords.h"
#include "DocumentSession.h"
#include "ServerConnection.h"
#include "ServerVersion.h"
#include "SessionTimers.h"

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

/// Seals each ward's data key to the devices this device approved - a device it
/// linked, a person it granted - that the server lists and the ward's current
/// version does not yet seal to.
///
/// Prompted only by additions: removals take effect on the server at once, and the
/// next write in each ward prunes them. The version it writes applies the whole rule,
/// so it prunes too. Nothing here needs the lease, so no one holding it can hold off a
/// link or a grant.
///
/// For the ward open in the window it asks the session to publish, which reseals; for
/// any other it fetches the server's version, reseals it, and uploads it conditional
/// on that version, leaving the local file to catch up when the ward next opens.
///
/// What is owed lives in the records - each ward's granted devices and this device's
/// linked ones - so a queue made afresh owes the same, and finds it by checking every
/// ward.
class SealingQueue : public QObject
{
    Q_OBJECT

public:
    SealingQueue(const DeviceIdentity& identity,
                 DocumentRecords& records,
                 ServerConnection& server,
                 std::unique_ptr<SessionTimers> timers,
                 std::function<DocumentPaths(const QString&)> pathsFor,
                 QObject* parent);

    /// The ward open in the window, or nullptr.
    void setOpenSession(DocumentSession* session);

    /// Checks every shared ward and seals where needed. Called at start, after a link
    /// or a grant, and on Retry until every ward is done.
    void sealEverywhere();

    /// Wards still to be checked or sealed.
    int pendingCount() const { return int(m_pending.size()); }

signals:
    void pendingChanged();

    /// The server told this device it lost access to a ward that is not open; its
    /// files and record are gone.
    void accessLost(const QString& documentId, const ServerContract::LostAccess& lost);

private:
    void onTimer(SessionTimers::Timer timer);

    /// Starts the first pending ward not waiting for the next Retry.
    void processNext();

    void onHead(const QString& documentId, const ServerContract::Result<ServerContract::Head>& head);
    void onFetched(const QString& documentId, const ServerVersion::Access& access, const ServerVersion::Fetched& theirs);
    void onUploaded(const QString& documentId,
                    const ServerVersion::Access& access,
                    const DocumentHeader& sealed,
                    const ServerContract::Result<qint64>& result);

    /// This ward is done until the next sealEverywhere.
    void finish(const QString& documentId);

    /// This ward is checked again from the start.
    void again(const QString& documentId);

    /// This ward waits for the next Retry; the others go ahead.
    void defer(const QString& documentId);

    void lose(const QString& documentId, const ServerContract::LostAccess& lost);

    DeviceIdentity m_identity;
    DocumentRecords& m_records;
    ServerConnection& m_server;
    std::unique_ptr<SessionTimers> m_timers;
    std::function<DocumentPaths(const QString&)> m_pathsFor;
    QPointer<DocumentSession> m_openSession;

    QStringList m_pending;
    QSet<QString> m_deferred;
    bool m_busy = false;
    bool m_retryWaiting = false;
    int m_retryDelay = SessionTimers::FirstRetryMilliseconds;
};
