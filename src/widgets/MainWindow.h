#pragma once

#include <QDateTime>
#include <QMainWindow>
#include <QSplitter>
#include <QStringList>

#include <optional>

#include "DocumentChange.h"
#include "ConflictDialog.h"
#include "DocumentManager.h"
#include "DocumentSession.h"
#include "FileOpenRouter.h"
#include "Id.h"

class AppSettings;
class DocumentLibrary;
class QLabel;
class AccessDialog;
class UpdateChecker;
class WardNotices;
class QProgressBar;
class EmergencyAssetView;
class FamilyEditPanel;
class MapWidget;
class WardListView;
class MinisteringView;
class NeedsSubView;
class SidebarWidget;
class TaskListView;
class TeamsView;
class EmergencyBanner;
class GeoLocation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(AppSettings& settings, DocumentLibrary& library, UpdateChecker& updates, QWidget* parent);
    ~MainWindow();

    /// This launch's arguments, or those a later launch handed to this one: each
    /// .wardplanning file and invite link among them is followed by FileOpenRouter's
    /// route.
    void handleArguments(const QStringList& arguments);

    /// Opens a ward by id and tells the person how it went.
    void openWard(const QString& documentId);

signals:
    /// The person asked to set this device up afresh and confirmed it; the app
    /// replaces the identity and closes.
    void startOverRequested();

protected:
    /// Saves the open ward and releases its lease before the window closes.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewWard();
    void onOpenWard();
    void onSaveNow();
    void onSaveCopy();
    void onExportJson();
    void onOpenCopy();
    void onShowFolder();
    void onImportPdf();
    void onImportMinisteringPdf();
    void onUndo();
    void onRedo();
    void onDocumentChanged(const DocumentChange& change);
    void updateWindowTitle();
    void updateUndoRedoActions();

    // Geocoding
    void onGeocodingProgress(int completed, int total);
    void onGeocodingFinished();

    // Wards
    void onSaveFailed(const QString& errorMessage);
    void onDocumentOpened();
    void onAccessLost(const QString& wardName, const ServerVersion::LostAccess& lost);
    void onFetchFailed(const QString& documentId, DocumentSession::Download outcome);
    void onStartOver();
    void onShowAccess();
    /// Asks about the open ward's pending conflict until it is settled or set aside.
    void askAboutConflict();
    void onEditRefused(const QString& message);
    void onFetchFinished(const QString& documentId, DocumentManager::Open result, const QString& reason);
    void updateFileActions();

    // GeoLocation
    void onLocationReady(double latitude, double longitude);

    // Emergency lifecycle
    void onStartEmergency();
    void onEndEmergency();
    void onEmergencyStarted();
    void onEmergencyEnded();
    void generateEmergencyReport();
    void updateEmergencyActions();

    // Sidebar tabs
    void onSidebarTabChanged(int index);

    // Family editing
    void onEditFamilyRequested(const FamilyId& familyId);
    void onDeleteFamilyRequested(const FamilyId& familyId);
    void onSaveFamily();
    void onCancelEdit();
    void onCloseEditPanel();

private:
    void updateCopyBanner();
    void refreshAccessDialog(AccessDialog* dialog);

    bool settleConflict(ConflictDialog::Choice choice, QString* errorMessage);
    void setupUi();
    void setupMenus();
    void setupConnections();
    void initializeDefaultLocation();
    void openEditPanel(const FamilyId& familyId);
    void openPath(const QString& path);

    /// Carries out where a file or an invite link goes; path names the file, when
    /// there is one, for the messages.
    void followRoute(const FileOpenRouter::Route& route, const QString& path);

    /// The ward was shared: say so, and offer the first invite.
    void onShared();
    void onShareFailed(ServerContract::ServerError error);

    /// Add a Device and Add a User: an invite, shown as the message to send.
    void onAddDevice();
    void onAddUser();
    void showInvite(const std::optional<DocumentManager::Invitation>& invitation);

    /// Remove, unlink, withdraw, change a role, and leave, from the access dialog,
    /// each confirmed where it cannot be undone.
    void onRemovePerson(const MemberId& person);
    void onUnlinkDevice(const Fingerprint& device);
    void onWithdrawInvite(const Fingerprint& invite);
    void onLeave();

    /// An invite link opened here that did not reach an open ward.
    void onJoinFailed(DocumentSession::Join outcome, const QString& reason);
    void onLeaveFailed(ServerContract::ServerError error);

    void reportOpen(const QString& documentId, DocumentManager::Open result, const QString& reason);
    void openCopy(const QString& sourcePath);
    void closeEditPanelInternal();

    /// Asks where to save the emergency report and writes it. Returns the saved
    /// path, or nullopt when the user cancels or the report cannot be written.
    std::optional<QString> saveEmergencyReport(const std::optional<QDateTime>& endedAt);

    AppSettings& m_settings;
    DocumentLibrary& m_library;
    UpdateChecker& m_updates;

    // Widgets
    QSplitter* m_splitter;
    EmergencyBanner* m_emergencyBanner;
    QLabel* m_copyBanner;
    WardNotices* m_wardNotices;
    SidebarWidget* m_sidebarTabs;
    WardListView* m_wardListView;
    MinisteringView* m_ministeringView;
    NeedsSubView* m_needsView;
    TeamsView* m_teamsView;
    TaskListView* m_taskListView;
    int m_taskListTabIndex = -1;
    EmergencyAssetView* m_medicalView;
    EmergencyAssetView* m_commsView;
    EmergencyAssetView* m_recoveryView;
    FamilyEditPanel* m_editPanel;
    MapWidget* m_mapWidget;

    // Actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_importPdfAction;
    QAction* m_importMinisteringPdfAction;
    QAction* m_startEmergencyAction;
    QAction* m_endEmergencyAction;
    QAction* m_generateReportAction;
    QAction* m_resolveConflictAction;
    QAction* m_startOverAction;
    QAction* m_exportJsonAction = nullptr;     ///< only when developer export is on
    QAction* m_exitAction;
    QAction* m_undoAction;
    QAction* m_redoAction;

    // Geocoding progress widgets (in status bar)
    QLabel* m_geocodingLabel;
    QProgressBar* m_geocodingProgress;
};
