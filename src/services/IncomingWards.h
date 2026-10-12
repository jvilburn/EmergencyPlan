#pragma once

#include "AppSettings.h"
#include "DocumentLibrary.h"
#include "DocumentSession.h"
#include "ServerConnection.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

/// The wards on their way to this device: each this device asked for and an admin has
/// not yet answered, and each the server lists for this person that the library
/// lacks - after a grant, a link, a reinstall, or cleared app data. Also the wards
/// here that the server has stopped listing for this person: taken away, or left on
/// another device.
///
/// A request lasts until it is answered or the server no longer has it; a granted one
/// becomes a listed ward, readable at once, since the version sealed to the device that
/// asked went up with the grant. A ward a linked device's approver did not hold at the
/// approval waits for access until that approver next writes it.
///
/// Checked at launch, and then every minute only while a request or a ward is waiting,
/// so a device with nothing on its way makes no periodic calls. Requests are kept in
/// AppSettings, so they outlive closing the app.
class IncomingWards : public QObject
{
    Q_OBJECT

public:
    static constexpr int CheckMilliseconds = 60 * 1000;

    IncomingWards(AppSettings& settings, DocumentLibrary& library, ServerConnection& server, QObject* parent);

    /// Asks the server for access to a ward this device lacks.
    void request(const QString& documentId);

    /// The requests not yet answered.
    QStringList pending() const { return m_settings.requestedWards(); }

    /// The wards the server lists for this person that are not here yet, as of the
    /// last check: each waits for a version sealed to this device, or for the server
    /// to be reached again.
    const QStringList& waiting() const { return m_waiting; }

    /// Whether the periodic check is running: it is while anything is pending or waiting.
    bool isWatching() const { return m_timer.isActive(); }

    /// Checks every request, every listed ward this device lacks, and every shared ward
    /// here against the server's list, now.
    void checkNow();

signals:
    void requested(const QString& documentId);
    void requestFailed(const QString& documentId, ServerContract::ServerError error);
    void declined(const QString& documentId);

    /// A ward reached this device, or could not be kept: Done, NeedsNewerVersion, or
    /// WriteFailed. One still waiting for access is not reported.
    void fetched(const QString& documentId, DocumentSession::Download outcome);

    /// A shared ward here has been lost - removed, or left on another device - and the
    /// server says how. Its files and copies are still here for the receiver to delete.
    void lostAccess(const QString& documentId, const ServerContract::LostAccess& lost);

private:
    void onRequested(const QString& documentId, const ServerContract::Result<ServerContract::Done>& result);
    void onStatus(const QString& documentId, const ServerContract::Result<ServerContract::RequestStatus>& status);
    void onListed(const QStringList& onTheirWay);
    void onFetched(const QString& documentId, DocumentSession::Download outcome);

    void add(const QString& documentId);
    void drop(const QString& documentId);

    /// Runs the timer exactly while something is pending or waiting.
    void settleWatch();

    AppSettings& m_settings;
    DocumentLibrary& m_library;
    ServerConnection& m_server;
    QTimer m_timer;
    QStringList m_waiting;
};
