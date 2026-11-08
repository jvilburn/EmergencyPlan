#pragma once

#include <QMainWindow>
#include <QSplitter>

#include "DocumentChange.h"
#include "Id.h"
#include "WardSession.h"

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
class EmergencyResponse;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent);
    ~MainWindow();

    /// Opens the ward the app starts on: on the first start, asks this device's name; then
    /// opens the last ward, or, when there is none or it does not open, tells why and creates
    /// a ward. False when the user cancels, which quits the app.
    bool openStartingWard();

protected:
    /// Tries a pending save once more, and when it still fails asks before closing.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewWard();
    void onSaveFailed(const QString& problem);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void onImportPdf();
    void onImportMinisteringPdf();
    void onUndo();
    void onRedo();
    void onDocumentChanged(const DocumentChange& change);
    void updateWindowTitle();
    void updateUndoRedoActions();
    void onCommandNotApplied(const CommitResult& result);

    // Geocoding
    void onGeocodingProgress(int completed, int total);
    void onGeocodingFinished();

    // GeoLocation
    void onLocationReady(double latitude, double longitude);

    // Emergency lifecycle
    void onStartEmergency();
    void onEndEmergency();
    void onEmergencyStarted();
    void onEmergencyEnded();
    void generateEmergencyReport();
    void updateEmergencyBanner();
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
    void setupUi();
    void setupMenus();
    void setupConnections();
    void initializeDefaultLocation();
    void openEditPanel(const FamilyId& familyId);
    void closeEditPanelInternal();

    /// Asks the user's name, kept for the next new ward, and creates a ward under it on this
    /// device, which becomes the last ward. False when the user cancels.
    bool createWard();

    /// True when nothing is left unsaved after one more try at saving, or when the user
    /// agrees to lose what is.
    bool unsavedChangesMayBeLost();

    /// What the user is told when a ward does not open, for each way it can fail.
    static QString openFailureMessage(OpenStatus status);

    // Settings helpers
    /// The report's suggested path: the emergency's name and start date in Documents.
    static QString defaultReportPath(const EmergencyResponse& response);
    static QString settingsFilePath();

    /// The text setting under key in the settings file, empty when it has none.
    static QString loadSetting(const QString& key);
    static void saveSetting(const QString& key, const QString& value);
    static bool loadDefaultLocation(double& lat, double& lng);
    static void saveDefaultLocation(double lat, double lng);

    // Widgets
    QSplitter* m_splitter;
    EmergencyBanner* m_emergencyBanner;
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
    QAction* m_newWardAction;
    QAction* m_importPdfAction;
    QAction* m_importMinisteringPdfAction;
    QAction* m_startEmergencyAction;
    QAction* m_endEmergencyAction;
    QAction* m_generateReportAction;
    QAction* m_exitAction;
    QAction* m_undoAction;
    QAction* m_redoAction;

    // Geocoding progress widgets (in status bar)
    QLabel* m_geocodingLabel;
    QProgressBar* m_geocodingProgress;
};
