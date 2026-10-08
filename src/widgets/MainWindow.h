#pragma once

#include <QDateTime>
#include <QMainWindow>
#include <QSplitter>
#include <QStringList>

#include <optional>

#include "DocumentChange.h"
#include "DocumentManager.h"
#include "DocumentSession.h"
#include "Id.h"

class AppSettings;
class DocumentLibrary;
class QLabel;
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
    MainWindow(AppSettings& settings, DocumentLibrary& library, QWidget* parent);
    ~MainWindow();

    /// This launch's arguments, or those a later launch handed to this one: each
    /// .wardplanning file among them is opened by FileOpenRouter's route.
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
    void onAccessRemoved(const QString& wardName, const QString& byWhom, bool left);
    void onFetchFailed(const QString& documentId, DocumentSession::Download outcome);
    void onStartOver();
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
    void setupUi();
    void setupMenus();
    void setupConnections();
    void initializeDefaultLocation();
    void openEditPanel(const FamilyId& familyId);
    void openPath(const QString& path);
    void reportOpen(DocumentManager::Open result, const QString& reason);
    void openCopy(const QString& sourcePath);
    void closeEditPanelInternal();

    /// Asks where to save the emergency report and writes it. Returns the saved
    /// path, or nullopt when the user cancels or the report cannot be written.
    std::optional<QString> saveEmergencyReport(const std::optional<QDateTime>& endedAt);

    AppSettings& m_settings;
    DocumentLibrary& m_library;

    // Widgets
    QSplitter* m_splitter;
    EmergencyBanner* m_emergencyBanner;
    QLabel* m_copyBanner;
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
    QAction* m_startOverAction;
    QAction* m_exitAction;
    QAction* m_undoAction;
    QAction* m_redoAction;

    // Geocoding progress widgets (in status bar)
    QLabel* m_geocodingLabel;
    QProgressBar* m_geocodingProgress;
};
