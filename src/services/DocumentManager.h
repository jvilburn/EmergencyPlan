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
#include "SealingSet.h"

class BackgroundGeocodingService;
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
                    QObject* parent);

    /// For tests: one session, already open, with no library or server.
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

    /// The open ward's conflict waiting for a choice, if any.
    std::optional<DocumentSession::Conflict> pendingConflict() const;

    /// Writes the open ward's document, which keeping theirs would discard, as a
    /// recovery copy in the person's Documents folder, and gives its path.
    bool saveRecoveryCopy(QString* path, QString* errorMessage);

    /// Settles the open ward's pending conflict. False, with the reason, when it
    /// could not be written; the conflict then waits to be asked about again.
    bool resolveConflict(DocumentSession::Resolution resolution, QString* errorMessage);

    /// The ward's name as the library lists it; empty for one not there.
    QString wardNameOf(const QString& documentId) const;

    /// A shared ward is being fetched to open.
    bool isFetching() const { return !m_fetching.isEmpty(); }

    /// Who has access to the open ward, from the ward itself; empty with none open.
    SealingSet roster() const;

    /// This person in the open ward's roster; nullopt with no ward open.
    std::optional<Holder> self() const;

    /// What changed in who has access since the notice was last dismissed, one line
    /// each; this device's own changes count as seen.
    const QStringList& unseenRosterChanges() const { return m_rosterChanges; }
    void dismissRosterChanges();

    /// Registers the open ward with the server, sealed to this device alone; shared
    /// or shareFailed reports how it went.
    void share();

    /// An invite to the open ward: a Device invite joins this person, a User invite
    /// makes a new member. Whether this person may invite is the window's rule.
    /// nullopt for a ward that is not shared.
    std::optional<FileOpenRouter::InviteLink> invite(Invite::For purpose, const QString& label);

    /// What the person copies and sends for link.
    QString inviteMessage(const FileOpenRouter::InviteLink& link) const;

    /// Each a write of the open ward; the next version is sealed without whoever
    /// was taken out, under a fresh key. Neither takes out this person or this
    /// device: that is leave.
    void removePerson(const MemberId& id);
    void removeDevice(const Fingerprint& device);
    void revokeInvite(const Fingerprint& invite);
    void setRole(const MemberId& id, Role role);

    /// This person leaves the open ward: a version without them goes up, and once
    /// it has, the ward and its copies go from this device, reported as accessLost.
    void leave();

    /// Joins the ward an invite link names and opens it. fetchFinished reports the
    /// open; joinFailed reports a join that did not get that far.
    void joinWard(const FileOpenRouter::InviteLink& link);

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

    /// The pending conflict was settled.
    void conflictSettled();

    /// The open ward's head answered: who has access, and the lease holder, as the
    /// server now says.
    void headChanged();

    /// unseenRosterChanges() changed.
    void accessChanged();

    /// The open ward is now shared.
    void shared(const QString& documentId);
    void shareFailed(ServerContract::ServerError error);

    /// Joining through an invite link did not reach an open ward.
    void joinFailed(DocumentSession::Join outcome, const QString& reason);

    /// Leaving did not go through; the ward stays.
    void leaveFailed(ServerContract::ServerError error);

    /// The server holds a version this build cannot read, or no longer does.
    void requiresUpdateChanged();

    /// This device lost access to a ward, which has left the library.
    void accessLost(const QString& wardName, const ServerVersion::LostAccess& lost);

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
    void onHeadChanged();
    void onShared();
    /// Applies change to the open ward's roster and publishes; nothing when change
    /// refuses. This device's own change counts as seen.
    void editRoster(const std::function<bool(SealingSet&)>& change);
    void refreshRosterChanges();
    void onJoined(const QString& documentId, DocumentSession::Join outcome, const QString& reason);
    void onSessionLost(const ServerVersion::LostAccess& lost);

    /// The ward's files, copies, record, and entry go, and the person is told.
    void forgetLostWard(const QString& documentId, const ServerVersion::LostAccess& lost);

    void fetch(const QString& documentId);
    void onFetched(const QString& documentId, DocumentSession::Download outcome);
    void refuse(DocumentSession::Refusal refusal);
    void resetWard();

    void checkForIncompleteWards();

    DeviceIdentity m_identity;
    DocumentRecords* m_records = nullptr;
    ServerConnection* m_server = nullptr;
    DocumentLibrary* m_library = nullptr;

    std::unique_ptr<DocumentSession> m_session;
    std::optional<DocumentLibrary::Copy> m_copy;

    /// The shared ward being fetched to open, until it opens or another is shown.
    QString m_fetching;

    QStringList m_rosterChanges;
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
