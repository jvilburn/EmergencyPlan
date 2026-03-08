#pragma once

#include <QMainWindow>
#include <QSplitter>

#include "DocumentChange.h"
#include "Id.h"

class QLabel;
class QProgressBar;
class DocumentManager;
class EmergencyAssetView;
class FamilyEditPanel;
class MapWidget;
class WardListView;
class MinisteringView;
class NeedsSubView;
class SidebarWidget;
class GeoLocation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
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

    // Auto-save
    void onAutoSaveFailed(const QString& errorMessage);

    // GeoLocation
    void onLocationReady(double latitude, double longitude);

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

    // Settings helpers
    static QString settingsFilePath();
    static bool loadDefaultLocation(double& lat, double& lng);
    static void saveDefaultLocation(double lat, double lng);

    // Core services
    DocumentManager* m_documentManager;

    // Widgets
    QSplitter* m_splitter;
    SidebarWidget* m_sidebarTabs;
    WardListView* m_wardListView;
    MinisteringView* m_ministeringView;
    NeedsSubView* m_needsView;
    EmergencyAssetView* m_medicalView;
    EmergencyAssetView* m_commsView;
    EmergencyAssetView* m_recoveryView;
    FamilyEditPanel* m_editPanel;
    MapWidget* m_mapWidget;

    // Actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_importPdfAction;
    QAction* m_importMinisteringPdfAction;
    QAction* m_exitAction;
    QAction* m_undoAction;
    QAction* m_redoAction;

    // Geocoding progress widgets (in status bar)
    QLabel* m_geocodingLabel;
    QProgressBar* m_geocodingProgress;
};
