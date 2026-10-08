#pragma once

#include <QObject>
#include <QString>
#include <QSet>

#include <functional>
#include <memory>
#include <optional>

#include "CommandHistory.h"
#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentChange.h"
#include "DocumentLibrary.h"
#include "DocumentSession.h"

class BackgroundGeocodingService;
class RosterChangeQueue;
class ServerConnection;
class UnitLookupService;

/// The ward open in the window: it holds that ward's DocumentSession and keeps
/// what is the window's own - commands, undo, geocoding, and unit lookups. Every
/// change reaches the document through the session's edit, which refuses a
/// directory change while another member holds the lease, before anything moves.
///
/// With no ward open the window shows an empty document, and the first edit makes
/// it a new ward. A copy opened read-only is shown in place of a ward and takes no
/// edit at all.
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    /// How opening a ward by id went.
    enum class Open
    {
        Opened,
        Fetching,           ///< a shared ward with no usable local file, being downloaded
        NotFound,           ///< an unshared ward whose file is gone
        NeedsNewerVersion,
        Refused,            ///< failed a check; the reason comes with it
        NotClosed           ///< the open ward could not be saved, so it stays open
    };

    /// The app's: wards open through the library and talk to the server.
    DocumentManager(const DeviceIdentity& identity,
                    DocumentRecords& records,
                    ServerConnection& server,
                    DocumentLibrary& library,
                    RosterChangeQueue& queue,
                    QObject* parent);

    /// For tests: one session, already open, with no library, server, or queue.
    DocumentManager(const DeviceIdentity& identity, std::unique_ptr<DocumentSession> session, QObject* parent);

    ~DocumentManager() override;

    /// Global accessor. Valid after construction.
    static DocumentManager* instance();

    const DeviceIdentity& identity() const { return m_identity; }

    /// The copy when one is shown, else the open ward's, else an empty document.
    const Document& document() const;

    /// The open ward's; empty with none, and while a copy is shown.
    QString documentId() const;

    /// The open ward's session, for the sync indicator; nullptr with none.
    DocumentSession* session() const { return m_session.get(); }

    /// The copy shown read-only, if any.
    const std::optional<DocumentLibrary::Copy>& copy() const { return m_copy; }

    /// Runs command on the open ward, making the empty document a new ward when
    /// none is open. False when it was refused, with editRefused saying why, and
    /// nothing changed.
    bool executeCommand(CommandPtr command);

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const { return m_commandHistory.canUndo(); }
    bool canRedo() const { return m_commandHistory.canRedo(); }
    QString undoDescription() const { return m_commandHistory.undoDescription(); }
    QString redoDescription() const { return m_commandHistory.redoDescription(); }

    /// The emergency response, changed in place through the session. Any member
    /// may record one at any time, even while another holds the lease; only a copy
    /// shown read-only refuses it. False when refused.
    bool editResponse(const std::function<void(std::optional<EmergencyResponse>&)>& change);

    // Wards
    /// A new ward, its id and files made up front. False, with the reason, when it
    /// cannot be written or the open ward cannot be saved first.
    bool newDocument(QString* errorMessage);
    /// Opening the ward already open leaves it open. A shared ward with no usable
    /// local file is fetched, and fetchFinished reports how opening it went.
    Open openDocument(const QString& documentId, QString* refusal);

    /// Shows a copy read-only in place of the open ward, which is closed.
    bool showCopy(const DocumentLibrary::Copy& copy, QString* errorMessage);

    /// Saves now and publishes. A local failure is the one reported.
    bool publishNow(QString* errorMessage);

    /// Saves and closes the open ward, releasing its lease. False, with the reason,
    /// when the save failed; the ward then stays open.
    bool close(QString* errorMessage);

    /// Unshared, or this device holds the lease: automatic directory updates run
    /// only while it is true.
    bool mayWriteDirectory() const;

    // Geocoding
    void startBatchGeocoding();
    void stopGeocoding();
    bool isGeocoding() const;

signals:
    void documentChanged(const DocumentChange& change);
    void canUndoChanged();
    void canRedoChanged();

    /// Another ward, a new one, a copy, or none is now shown.
    void documentOpened();

    /// An edit that changed nothing, and why.
    void editRefused(const QString& message);

    /// A local write failed; the one modal failure.
    void saveFailed(const QString& errorMessage);

    void readOnlyChanged();
    void syncStateChanged(SyncState state);
    void conflictRaised(const DocumentSession::Conflict& conflict);

    /// This device is off a ward, which has left the library.
    void accessRemoved(const QString& wardName, const QString& byWhom, bool left);

    /// A shared ward being fetched could not be.
    void fetchFailed(const QString& documentId, DocumentSession::Download outcome);

    /// A shared ward was fetched, and opening it went as result says. Not reported
    /// when another ward was opened while it was fetched.
    void fetchFinished(const QString& documentId, DocumentManager::Open result, const QString& reason);

    // Geocoding progress
    void geocodingProgressChanged(int completed, int total);
    void geocodingFinished();

private slots:
    void onWardLookupComplete(const QString& wardUnitNumber, const Ward& ward);
    void onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stake);
    void onLookupFailed(const QString& unitNumber, const QString& error);

    // Geocoding
    void onFamilyGeocoded(const FamilyId& id, double latitude, double longitude);
    void onGeocodingFinished();

private:
    void connectServices();

    /// Opens a ward from its local file; a shared ward whose file will not open is
    /// fetched when mayFetch is set.
    Open open(const QString& documentId, QString* refusal, bool mayFetch);

    /// Takes session as the open ward, with nothing else shown.
    void install(std::unique_ptr<DocumentSession> session);

    /// Closes the open ward or copy; false when the ward could not be saved.
    bool closeCurrent(QString* errorMessage);

    /// The open ward, making the empty document a new ward when none is open.
    DocumentSession* sessionForEdit();

    DocumentSession::Context context() const;

    /// The document after a change from outside an edit, or a new one shown.
    void onDocumentReplaced();
    void afterChange(const DocumentChange& change);
    void onReadOnlyChanged();
    void onSessionRemoved(const QString& byWhom, bool left);
    void onQueueRemoved(const QString& documentId, bool left);

    /// The ward's files, copies, record, and entry go, and the person is told.
    void forgetRemovedWard(const QString& documentId, const QString& byWhom, bool left);
    QString wardNameOf(const QString& documentId) const;

    void fetch(const QString& documentId);
    void onFetched(const QString& documentId, DocumentSession::Download outcome);
    void refuse(DocumentSession::Refusal refusal);
    void resetWard();

    void checkForIncompleteWards();

    DeviceIdentity m_identity;
    DocumentRecords* m_records = nullptr;
    ServerConnection* m_server = nullptr;
    DocumentLibrary* m_library = nullptr;
    RosterChangeQueue* m_queue = nullptr;

    std::unique_ptr<DocumentSession> m_session;
    std::optional<DocumentLibrary::Copy> m_copy;

    /// The shared ward being fetched to open, until it opens or another is shown.
    QString m_fetching;
    Document m_empty;
    CommandHistory m_commandHistory;

    // Unit lookup
    UnitLookupService* m_unitLookupService = nullptr;
    QSet<QString> m_pendingWardLookups;
    QSet<QString> m_pendingStakeLookups;

    // Geocoding
    BackgroundGeocodingService* m_geocodingService = nullptr;

    static DocumentManager* s_instance;
};
