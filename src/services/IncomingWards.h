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
/// lacks - after a grant, a link, a reinstall, or cleared app data.
///
/// A request lasts until it is answered or the server no longer has it; a granted one
/// becomes a listed ward. A listed ward arrives once a version sealed to this device
/// does. The device that approved it seals it - the granting admin's, or the one that
/// linked this device - so it may lag the grant or the link a little; until then it
/// waits for access.
///
/// Checked every minute, so a ward arrives soon after it is sealed. Requests are kept
/// in AppSettings, so they outlive closing the app.
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

    /// The listed wards not yet sealed to this device, as of the last check.
    const QStringList& waiting() const { return m_waiting; }

    /// Checks every request, and fetches every listed ward this device lacks, now.
    void checkNow();

signals:
    void requested(const QString& documentId);
    void requestFailed(const QString& documentId, ServerContract::ServerError error);
    void declined(const QString& documentId);

    /// A ward reached this device, or could not be kept: Done, NeedsNewerVersion, or
    /// WriteFailed. One still waiting for access is not reported.
    void fetched(const QString& documentId, DocumentSession::Download outcome);

private:
    void onRequested(const QString& documentId, const ServerContract::Result<ServerContract::Done>& result);
    void onStatus(const QString& documentId, const ServerContract::Result<ServerContract::RequestStatus>& status);
    void onFetched(const QString& documentId, DocumentSession::Download outcome);

    void add(const QString& documentId);
    void drop(const QString& documentId);

    AppSettings& m_settings;
    DocumentLibrary& m_library;
    ServerConnection& m_server;
    QTimer m_timer;
    QStringList m_waiting;
};
