#pragma once

#include "AppSettings.h"
#include "DocumentLibrary.h"
#include "DocumentSession.h"
#include "ServerConnection.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

/// The wards this device asked for and has not yet received. Each is asked for once,
/// then checked on an interval: an admin grants it, and the ward arrives once a
/// version sealed to this device does - the granting admin's app seals it, so it may
/// lag the grant a little - or the request is declined.
///
/// Kept in AppSettings, so a request outlives closing the app.
class PendingRequests : public QObject
{
    Q_OBJECT

public:
    static constexpr int CheckMilliseconds = 60 * 1000;

    PendingRequests(AppSettings& settings, DocumentLibrary& library, ServerConnection& server, QObject* parent);

    /// Asks the server for access to a ward this device lacks.
    void request(const QString& documentId);

    QStringList pending() const { return m_settings.requestedWards(); }

    /// Checks every pending request now, as the interval does.
    void checkNow();

signals:
    void requested(const QString& documentId);
    void requestFailed(const QString& documentId, ServerContract::ServerError error);

    /// Granted, and on this device.
    void arrived(const QString& documentId);
    void declined(const QString& documentId);

private:
    void onRequested(const QString& documentId, const ServerContract::Result<ServerContract::Done>& result);
    void onStatus(const QString& documentId, const ServerContract::Result<ServerContract::RequestStatus>& status);
    void onFetched(const QString& documentId, DocumentSession::Download outcome);

    void add(const QString& documentId);
    void drop(const QString& documentId);
    void fetch(const QString& documentId);

    AppSettings& m_settings;
    DocumentLibrary& m_library;
    ServerConnection& m_server;
    QTimer m_timer;
};
