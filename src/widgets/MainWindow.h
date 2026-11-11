#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QString>

#include "DocumentChange.h"
#include "Id.h"
#include "WardSession.h"

class QLabel;
class QProgressBar;
class AppSettings;
class EmergencyAssetView;
class FamilyEditPanel;
class MapWidget;
class FamilyListView;
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
    /// settings are the app's own settings, which the window does not own. developer adds the
    /// Debugging menu.
    MainWindow(AppSettings* settings, bool developer, QWidget* parent);
    ~MainWindow();

    /// Opens the ward the app starts on: on the first start, asks this device's name and your name; then
    /// opens the last ward, or, when there is none or it does not open, shows the ward list.
    /// False when the user cancels, which quits the app.
    bool openStartingWard();

public slots:
    /// Takes the update file at path, opening its ward first when another is open, and tells
    /// the user how it went.
    void openUpdateFile(const QString& path);

    /// Another launch handed over path, its update file, or nothing: brings the window
    /// forward and opens the update.
    void onHandedOver(const QString& path);

protected:
    /// Closes only when mayLeaveWard agrees.
    void closeEvent(QCloseEvent* event) override;

    /// Accepts a dragged update file, which a drop opens.
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewWard();
    void onOpenWard();
    void onShowInFolder();
    void onSendUpdate();
    void onOpenUpdate();

    /// Shows Settings, and on OK saves what changed; a changed name renames this device or
    /// its user in every ward this device holds.
    void onSettings();

    void onExportDecryptedJson();
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

    /// Closes the family edit panel, first asking to save its changes when it has any. False
    /// when the user cancels, which leaves the panel open.
    bool closeEditPanel();

private:
    void setupUi();
    void setupMenus(bool developer);
    void setupConnections();
    void initializeMapHome();
    void openEditPanel(const FamilyId& familyId);
    void closeEditPanelInternal();

    /// Commits the edit panel's family edit and the map position typed for its address, each
    /// its own undo entry, and leaves the panel shown.
    void saveEdit();

    /// When the edit panel has changes, asks whether to save them, and saves them on Save.
    /// False when the user cancels, which leaves the panel as it is.
    bool settleUnsavedEdit();

    /// Creates a ward under Your name on this device, which becomes the last ward.
    void createWard();

    /// Opens the ward, which becomes the last ward; when it does not open, tells why. False
    /// when it does not open.
    bool openWard(const DocumentId& document);

    /// Shows the ward list until a ward opens or is created from it, or the user closes it.
    /// Choosing the ward already open leaves it open as it is. False when the user closes it.
    bool chooseWard();

    /// Writes the to-redo file of an abandoned replay and tells the user where it is, with an
    /// offer to open it.
    void showChangesToRedo(const QString& toRedo);

    /// True when nothing is left unsaved after one more try at saving, or when the user
    /// agrees to lose what is.
    bool unsavedChangesMayBeLost();

    /// Whether the window may move to another ward: the family edit panel closes, as its own
    /// close button closes it, and then unsavedChangesMayBeLost. False when the user cancels
    /// either.
    bool mayLeaveWard();

    /// The report's suggested path: the emergency's name and start date in Documents.
    static QString defaultReportPath(const EmergencyResponse& response);

    AppSettings* m_settings;

    // Widgets
    QSplitter* m_splitter;
    EmergencyBanner* m_emergencyBanner;
    SidebarWidget* m_sidebarTabs;
    FamilyListView* m_familyListView;
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
