#pragma once

#include <QMainWindow>
#include <QSplitter>

#include "DocumentChange.h"

class QLabel;
class QProgressBar;
class QTabWidget;
class DocumentManager;
class MapWidget;
class WardListView;
class MinisteringView;
class GeoLocation;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

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

    // GeoLocation
    void onLocationReady(double latitude, double longitude);

    // Sidebar tabs
    void onSidebarTabChanged(int index);

private:
    void setupUi();
    void setupMenus();
    void setupConnections();
    void initializeDefaultLocation();
    bool maybeSave();

    // Settings helpers
    static QString settingsFilePath();
    static bool loadDefaultLocation(double& lat, double& lng);
    static void saveDefaultLocation(double lat, double lng);

    // Core services
    DocumentManager* m_documentManager;

    // Widgets
    QSplitter* m_splitter;
    QTabWidget* m_sidebarTabs;
    WardListView* m_wardListView;
    MinisteringView* m_ministeringView;
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
