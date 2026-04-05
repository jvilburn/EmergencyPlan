#include "MainWindow.h"
#include "ArchiveBrowserDialog.h"
#include "EmergencyBanner.h"
#include "WardListView.h"
#include "MinisteringView.h"
#include "NeedsSubView.h"
#include "SidebarWidget.h"
#include "EmergencyAssetView.h"
#include "TaskListView.h"
#include "TeamsView.h"
#include "ResponseArea.h"
#include "FamilyEditPanel.h"
#include "MapWidget.h"
#include "FamilyMarkerProvider.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "ReportGenerator.h"
#include "Ward.h"
#include "FamilyCommands.h"
#include "ImportWardDirectoryCommand.h"
#include "ImportEQMinisteringCommand.h"
#include "ImportRSMinisteringCommand.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"
#include "WardDirectoryImportService.h"
#include "MinisteringImportService.h"
#include "GeoLocation.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QCheckBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    setupConnections();

    // Auto-load last document
    bool documentLoaded = false;
    QString lastPath = loadLastDocumentPath();
    if (!lastPath.isEmpty() && QFile::exists(lastPath))
    {
        if (DocumentManager::instance()->openDocument(lastPath, nullptr))
        {
            // Center on chapel immediately; defer fit until widget has size
            m_mapWidget->centerOnChapel();
            m_mapWidget->requestFitAllFamilies();
            documentLoaded = true;
        }
        else
        {
            saveLastDocumentPath(QString());
        }
    }

    if (!documentLoaded)
    {
        initializeDefaultLocation();
    }
    updateWindowTitle();
    updateUndoRedoActions();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    resize(1400, 900);

    // Central widget with banner and splitter
    QWidget* centralContainer = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_emergencyBanner = new EmergencyBanner(centralContainer);
    m_splitter = new QSplitter(Qt::Horizontal, centralContainer);

    centralLayout->addWidget(m_emergencyBanner);
    centralLayout->addWidget(m_splitter, 1);
    setCentralWidget(centralContainer);

    // Sidebar navigation (two-row button bar)
    m_sidebarTabs = new SidebarWidget(5, m_splitter);

    // Row 1
    m_wardListView = new WardListView(this);
    m_sidebarTabs->addPage(m_wardListView, tr("Families"));

    m_ministeringView = new MinisteringView(this);
    m_sidebarTabs->addPage(m_ministeringView, tr("Ministering"));

    m_teamsView = new TeamsView(this);
    m_sidebarTabs->addPage(m_teamsView, tr("Teams"));

    m_needsView = new NeedsSubView(this);
    m_sidebarTabs->addPage(m_needsView, tr("Needs"));

    m_taskListView = new TaskListView(this);
    m_taskListTabIndex = m_sidebarTabs->count();
    m_sidebarTabs->addPage(m_taskListView, tr("Tasks"));
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);

    // Row 2
    m_medicalView = new EmergencyAssetView(ResponseArea::Medical, this);
    m_sidebarTabs->addPage(m_medicalView, tr("Medical"));

    m_commsView = new EmergencyAssetView(ResponseArea::Communications, this);
    m_sidebarTabs->addPage(m_commsView, tr("Communications"));

    m_recoveryView = new EmergencyAssetView(ResponseArea::Recovery, this);
    m_sidebarTabs->addPage(m_recoveryView, tr("Skills && Gear"));

    // Edit panel (initially hidden)
    m_editPanel = new FamilyEditPanel(m_splitter);
    m_editPanel->hide();

    // Map in the center
    m_mapWidget = new MapWidget(m_splitter);

    m_splitter->addWidget(m_sidebarTabs);
    m_splitter->addWidget(m_editPanel);
    m_splitter->addWidget(m_mapWidget);
    m_splitter->setSizes({300, 0, 1100});
    m_splitter->setStretchFactor(0, 0);  // Tabs don't stretch
    m_splitter->setStretchFactor(1, 0);  // Edit panel doesn't stretch
    m_splitter->setStretchFactor(2, 1);  // Map stretches

    // Set initial highlight provider (WardListView)
    m_mapWidget->setMarkerProvider(m_wardListView);

    // Status bar
    statusBar()->showMessage(tr("Ready"));

    // Geocoding progress widgets (hidden by default)
    m_geocodingLabel = new QLabel(this);
    m_geocodingProgress = new QProgressBar(this);
    m_geocodingProgress->setMaximumWidth(150);
    m_geocodingProgress->setTextVisible(false);
    statusBar()->addPermanentWidget(m_geocodingLabel);
    statusBar()->addPermanentWidget(m_geocodingProgress);
    m_geocodingLabel->hide();
    m_geocodingProgress->hide();
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    m_newAction = fileMenu->addAction(tr("&New"), this, &MainWindow::onNewDocument);
    m_newAction->setShortcut(QKeySequence::New);

    m_openAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::onOpenDocument);
    m_openAction->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();

    m_saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveDocument);
    m_saveAction->setShortcut(QKeySequence::Save);

    m_saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onSaveDocumentAs);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();

    QMenu* importMenu = fileMenu->addMenu(tr("&Import"));
    m_importPdfAction = importMenu->addAction(tr("&Ward Directory PDF..."), this, &MainWindow::onImportPdf);
    m_importMinisteringPdfAction = importMenu->addAction(tr("&Ministering PDF..."), this, &MainWindow::onImportMinisteringPdf);

    fileMenu->addSeparator();

    m_startEmergencyAction = fileMenu->addAction(tr("Start &Emergency..."), this, &MainWindow::onStartEmergency);
    m_endEmergencyAction = fileMenu->addAction(tr("&End Emergency..."), this, &MainWindow::onEndEmergency);
    m_endEmergencyAction->setEnabled(false);
    m_generateReportAction = fileMenu->addAction(tr("&Generate Emergency Report..."), this, &MainWindow::generateEmergencyReport);
    m_generateReportAction->setEnabled(false);
    m_openArchiveAction = fileMenu->addAction(tr("Open Emergency &Archive..."), this, &MainWindow::onOpenArchive);
    m_openArchiveAction->setEnabled(false);

    fileMenu->addSeparator();

    m_exitAction = fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    m_undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::onUndo);
    m_undoAction->setShortcut(QKeySequence::Undo);

    m_redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::onRedo);
    m_redoAction->setShortcut(QKeySequence::Redo);
}

void MainWindow::setupConnections()
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &MainWindow::onDocumentChanged);
    connect(DocumentManager::instance(), &DocumentManager::filePathChanged,
            this, &MainWindow::updateWindowTitle);
    connect(DocumentManager::instance(), &DocumentManager::filePathChanged,
            this, &MainWindow::onFilePathChanged);
    connect(DocumentManager::instance(), &DocumentManager::canUndoChanged,
            this, &MainWindow::updateUndoRedoActions);
    connect(DocumentManager::instance(), &DocumentManager::canRedoChanged,
            this, &MainWindow::updateUndoRedoActions);

    // Emergency lifecycle
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &MainWindow::onEmergencyStarted);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &MainWindow::onEmergencyEnded);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewOpened,
            this, &MainWindow::onArchiveViewOpened);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewClosed,
            this, &MainWindow::onArchiveViewClosed);
    connect(EmergencyManager::instance(), &EmergencyManager::responseDataChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_emergencyBanner, &EmergencyBanner::closeArchiveRequested,
            EmergencyManager::instance(), &EmergencyManager::closeArchive);

    // Sidebar tab changes
    connect(m_sidebarTabs, &SidebarWidget::currentChanged,
            this, &MainWindow::onSidebarTabChanged);

    // Highlight and visibility changes from sidebar views
    // highlightChanged connections are handled automatically by MapWidget::setMarkerProvider
    connect(m_wardListView, &WardListView::visibleFamiliesChanged,
            m_mapWidget, &MapWidget::updateHighlights);

    // Family editing
    connect(m_wardListView, &WardListView::editFamilyRequested,
            this, &MainWindow::onEditFamilyRequested);
    connect(m_wardListView, &WardListView::deleteFamilyRequested,
            this, &MainWindow::onDeleteFamilyRequested);
    connect(m_editPanel, &FamilyEditPanel::saveRequested,
            this, &MainWindow::onSaveFamily);
    connect(m_editPanel, &FamilyEditPanel::cancelRequested,
            this, &MainWindow::onCancelEdit);
    connect(m_editPanel, &FamilyEditPanel::closeRequested,
            this, &MainWindow::onCloseEditPanel);

    // Geocoding progress
    connect(DocumentManager::instance(), &DocumentManager::geocodingProgressChanged,
            this, &MainWindow::onGeocodingProgress);
    connect(DocumentManager::instance(), &DocumentManager::geocodingFinished,
            this, &MainWindow::onGeocodingFinished);

    // Auto-save
    connect(DocumentManager::instance(), &DocumentManager::autoSaveFailed,
            this, &MainWindow::onAutoSaveFailed);
}

void MainWindow::onNewDocument()
{
    DocumentManager::instance()->newDocument();
    statusBar()->showMessage(tr("New document created"), 3000);
}

void MainWindow::onOpenDocument()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Document"),
        QString(),
        tr("Emergency Plan Files (*.emergencyplan);;All Files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (DocumentManager::instance()->openDocument(filePath, &errorMessage))
    {
        statusBar()->showMessage(tr("Document opened"), 3000);
        m_mapWidget->fitAllFamilies();
    }
    else
    {
        QMessageBox::warning(this, tr("Error"), errorMessage);
    }
}

void MainWindow::onSaveDocument()
{
    if (DocumentManager::instance()->filePath().isEmpty())
    {
        onSaveDocumentAs();
    }
}

void MainWindow::onSaveDocumentAs()
{
    // Build suggested path: use existing path, or suggested filename
    QString suggestedPath;
    if (!DocumentManager::instance()->filePath().isEmpty())
    {
        suggestedPath = DocumentManager::instance()->filePath();
    }
    else
    {
        QString suggested = DocumentManager::instance()->document().suggestedFilename();
        if (!suggested.isEmpty())
        {
            suggestedPath = suggested + ".emergencyplan";
        }
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Document As"),
        suggestedPath,
        tr("Emergency Plan Files (*.emergencyplan);;All Files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (DocumentManager::instance()->saveDocumentAs(filePath, &errorMessage))
    {
        statusBar()->showMessage(tr("Document saved"), 3000);
    }
    else
    {
        QMessageBox::warning(this, tr("Error"), errorMessage);
    }
}

void MainWindow::onImportPdf()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import from PDF"),
        QString(),
        tr("PDF Files (*.pdf);;All Files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    statusBar()->showMessage(tr("Importing from PDF..."));

    // Parse PDF with ID preservation
    WardDirectoryImportService importService(nullptr);
    WardDirectoryImportResult result = importService.importFromPdf(
        filePath,
        DocumentManager::instance()->document().families(),
        DocumentManager::instance()->document().ministeringPdfDate());

    if (!result.success)
    {
        QString errorMsg = tr("Failed to import PDF");
        if (!result.errors.isEmpty())
        {
            errorMsg += ":\n" + result.errors.join("\n");
        }
        QMessageBox::warning(this, tr("Import Error"), errorMsg);
        statusBar()->showMessage(tr("Import failed"), 3000);
        return;
    }

    // Execute command
    QString description = result.wardName.isEmpty()
        ? QObject::tr("Import %1 families").arg(result.families.size())
        : QObject::tr("Import %1: %2 families")
            .arg(result.wardName)
            .arg(result.families.size());

    DocumentManager::instance()->executeCommand(std::make_unique<ImportWardDirectoryCommand>(
        result.families,
        result.removedFamilyIds,
        result.wardUnitNumber,
        result.wardName,
        result.pdfDate,
        description));

    // Update UI
    QString message = tr("Imported %1 families").arg(result.families.size());

    if (!result.wardUnitNumber.isEmpty())
    {
        updateWindowTitle();
    }

    statusBar()->showMessage(message, 5000);
    m_mapWidget->fitAllFamilies();

    if (!result.errors.isEmpty())
    {
        QMessageBox::information(this, tr("Import Warnings"),
            tr("Some items had warnings:\n") + result.errors.join("\n"));
    }

    // Start background geocoding for imported families
    DocumentManager::instance()->startBatchGeocoding();
}

void MainWindow::onImportMinisteringPdf()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Ministering PDF"),
        QString(),
        tr("PDF Files (*.pdf);;All Files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    statusBar()->showMessage(tr("Importing ministering assignments..."));

    MinisteringImportService importService(nullptr);
    MinisteringImportResult result = importService.importFromPdf(
        filePath,
        DocumentManager::instance()->document().families(),
        DocumentManager::instance()->document().wardDirectoryPdfDate());

    if (!result.success)
    {
        QString errorMsg = tr("Failed to import Ministering PDF");
        if (!result.errors.isEmpty())
        {
            errorMsg += ":\n" + result.errors.join("\n");
        }
        QMessageBox::warning(this, tr("Import Error"), errorMsg);
        statusBar()->showMessage(tr("Import failed"), 3000);
        return;
    }

    statusBar()->clearMessage();

    QString orgType = result.isRSFormat ? tr("RS") : tr("EQ");
    int groupCount = result.groups.size();

    // Build description for undo
    QString description = tr("Import %1 ministering: %2 districts, %3 groups")
        .arg(orgType)
        .arg(result.districts.size())
        .arg(groupCount);

    if (result.isRSFormat)
    {
        DocumentManager::instance()->executeCommand(std::make_unique<ImportRSMinisteringCommand>(
            result.districts,
            result.groups,
            result.families,
            result.pdfDate,
            description));
    }
    else
    {
        DocumentManager::instance()->executeCommand(std::make_unique<ImportEQMinisteringCommand>(
            result.districts,
            result.groups,
            result.families,
            result.pdfDate,
            description));
    }

    QString message = tr("Imported %1 ministering: %2 districts, %3 groups")
        .arg(orgType)
        .arg(result.districts.size())
        .arg(groupCount);

    statusBar()->showMessage(message, 5000);

    if (!result.errors.isEmpty())
    {
        QMessageBox::information(this, tr("Import Warnings"),
            tr("Some items had warnings:\n") + result.errors.join("\n"));
    }

    // Start background geocoding for any new families
    DocumentManager::instance()->startBatchGeocoding();
}

void MainWindow::onUndo()
{
    DocumentManager::instance()->undo();
}

void MainWindow::onRedo()
{
    DocumentManager::instance()->redo();
}

void MainWindow::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    int count = DocumentManager::instance()->document().families().size();
    statusBar()->showMessage(tr("%1 families").arg(count));
}

void MainWindow::updateWindowTitle()
{
    QString title = "Emergency Plan";

    QString filePath = DocumentManager::instance()->filePath();
    if (!filePath.isEmpty())
    {
        QFileInfo fileInfo(filePath);
        title = fileInfo.completeBaseName() + " - " + title;
    }
    else
    {
        QString suggested = DocumentManager::instance()->document().suggestedFilename();
        if (!suggested.isEmpty())
        {
            title = suggested + " - " + title;
        }
        else
        {
            title = "Untitled - " + title;
        }
    }

    setWindowTitle(title);
}

void MainWindow::updateUndoRedoActions()
{
    m_undoAction->setEnabled(DocumentManager::instance()->canUndo());
    m_redoAction->setEnabled(DocumentManager::instance()->canRedo());

    QString undoText = tr("&Undo");
    if (DocumentManager::instance()->canUndo())
    {
        undoText += " " + DocumentManager::instance()->undoDescription();
    }
    m_undoAction->setText(undoText);

    QString redoText = tr("&Redo");
    if (DocumentManager::instance()->canRedo())
    {
        redoText += " " + DocumentManager::instance()->redoDescription();
    }
    m_redoAction->setText(redoText);
}

void MainWindow::onGeocodingProgress(int completed, int total)
{
    if (total > 0)
    {
        m_geocodingLabel->setText(tr("Geocoding %1/%2").arg(completed).arg(total));
        m_geocodingProgress->setMaximum(total);
        m_geocodingProgress->setValue(completed);
        m_geocodingLabel->show();
        m_geocodingProgress->show();
    }
}

void MainWindow::onGeocodingFinished()
{
    m_geocodingLabel->hide();
    m_geocodingProgress->hide();
}

void MainWindow::onAutoSaveFailed(const QString& errorMessage)
{
    statusBar()->showMessage(tr("Auto-save failed: %1 — use Save As to save manually").arg(errorMessage));
}

void MainWindow::onFilePathChanged()
{
    saveLastDocumentPath(DocumentManager::instance()->filePath());
    updateEmergencyActions();
}

// ============================================================================
// Default Location / Settings
// ============================================================================

static QJsonObject loadSettingsJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

static void saveSettingsJson(const QString& path, const QJsonObject& json)
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);

    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(json).toJson());
    }
}

QString MainWindow::settingsFilePath()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cacheDir + "/settings.json";
}

bool MainWindow::loadDefaultLocation(double& lat, double& lng)
{
    QJsonObject json = loadSettingsJson(settingsFilePath());
    if (!json.contains("defaultLat") || !json.contains("defaultLng"))
    {
        return false;
    }

    lat = json["defaultLat"].toDouble();
    lng = json["defaultLng"].toDouble();
    return true;
}

void MainWindow::saveDefaultLocation(double lat, double lng)
{
    QJsonObject json = loadSettingsJson(settingsFilePath());
    json["defaultLat"] = lat;
    json["defaultLng"] = lng;
    saveSettingsJson(settingsFilePath(), json);
}

void MainWindow::saveLastDocumentPath(const QString& filePath)
{
    QJsonObject json = loadSettingsJson(settingsFilePath());
    json["lastDocumentPath"] = filePath;
    saveSettingsJson(settingsFilePath(), json);
}

QString MainWindow::loadLastDocumentPath()
{
    QJsonObject json = loadSettingsJson(settingsFilePath());
    return json["lastDocumentPath"].toString();
}

void MainWindow::initializeDefaultLocation()
{
    double lat, lng;
    if (loadDefaultLocation(lat, lng))
    {
        // Use saved location
        m_mapWidget->setCenter(lat, lng);
    }
    else
    {
        // No saved location - request via geolocation
        GeoLocation* geoLocation = new GeoLocation(this);
        connect(geoLocation, &GeoLocation::locationReady,
                this, &MainWindow::onLocationReady);
        connect(geoLocation, &GeoLocation::locationError,
                geoLocation, &QObject::deleteLater);  // Clean up on error
        geoLocation->requestLocation();
    }
}

void MainWindow::onLocationReady(double latitude, double longitude)
{
    m_mapWidget->setCenter(latitude, longitude);
    saveDefaultLocation(latitude, longitude);

    // Clean up the GeoLocation object
    sender()->deleteLater();
}

void MainWindow::onSidebarTabChanged(int index)
{
    QWidget* currentTab = m_sidebarTabs->widget(index);

    if (auto* provider = dynamic_cast<FamilyMarkerProvider*>(currentTab))
    {
        m_mapWidget->setMarkerProvider(provider);
    }
    else
    {
        m_mapWidget->setMarkerProvider(nullptr);
    }
}

// ============================================================================
// Family Editing
// ============================================================================

void MainWindow::onEditFamilyRequested(const FamilyId& familyId)
{
    // If editing a different family, check for unsaved changes
    if (m_editPanel->isVisible() && m_editPanel->familyId() != familyId)
    {
        if (m_editPanel->isDirty())
        {
            QMessageBox::StandardButton result = QMessageBox::question(
                this,
                tr("Unsaved Changes"),
                tr("Save changes to %1?").arg(m_editPanel->family().displayName()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (result == QMessageBox::Save)
            {
                onSaveFamily();
            }
            else if (result == QMessageBox::Cancel)
            {
                return;
            }
        }
    }

    openEditPanel(familyId);
}

void MainWindow::onDeleteFamilyRequested(const FamilyId& familyId)
{
    const auto& families = DocumentManager::instance()->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    const Family& family = it.value();
    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Delete Family"),
        tr("Delete %1?").arg(family.displayName()),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        // Close edit panel if editing this family
        if (m_editPanel->isVisible() && m_editPanel->familyId() == familyId)
        {
            closeEditPanelInternal();
        }

        // TODO: Implement DeleteFamilyCommand
        statusBar()->showMessage(tr("Delete not yet implemented"), 3000);
    }
}

void MainWindow::onSaveFamily()
{
    if (!m_editPanel->isVisible())
    {
        return;
    }

    Family editedFamily = m_editPanel->family();
    FamilyId familyId = m_editPanel->familyId();

    // Get original family for command
    const auto& families = DocumentManager::instance()->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    Family originalFamily = it.value();

    // Execute update command
    DocumentManager::instance()->executeCommand(std::make_unique<UpdateFamilyCommand>(
        originalFamily,
        editedFamily));

    closeEditPanelInternal();
}

void MainWindow::onCancelEdit()
{
    if (!m_editPanel->isVisible())
    {
        return;
    }

    if (m_editPanel->isDirty())
    {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("Discard Changes"),
            tr("Discard changes to %1?").arg(m_editPanel->family().displayName()),
            QMessageBox::Yes | QMessageBox::No);

        if (result != QMessageBox::Yes)
        {
            return;
        }
    }

    closeEditPanelInternal();
}

void MainWindow::onCloseEditPanel()
{
    if (!m_editPanel->isVisible())
    {
        return;
    }

    if (m_editPanel->isDirty())
    {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Save changes to %1?").arg(m_editPanel->family().displayName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (result == QMessageBox::Save)
        {
            onSaveFamily();
            return;
        }
        else if (result == QMessageBox::Cancel)
        {
            return;
        }
    }

    closeEditPanelInternal();
}

void MainWindow::openEditPanel(const FamilyId& familyId)
{
    const auto& families = DocumentManager::instance()->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    m_editPanel->setFamily(it.value());

    if (!m_editPanel->isVisible())
    {
        // Get current sizes and edit panel preferred width
        QList<int> sizes = m_splitter->sizes();
        int editWidth = m_editPanel->sizeHint().width();
        if (editWidth < 300)
        {
            editWidth = 300;  // Minimum reasonable width
        }

        // Expand: take space from the map
        sizes[1] = editWidth;
        sizes[2] = sizes[2] - editWidth;
        if (sizes[2] < 400)
        {
            sizes[2] = 400;  // Keep minimum map width
        }

        m_editPanel->show();
        m_splitter->setSizes(sizes);
    }
}

void MainWindow::closeEditPanelInternal()
{
    if (!m_editPanel->isVisible())
    {
        return;
    }

    // Get current sizes before hiding
    QList<int> sizes = m_splitter->sizes();
    int editWidth = sizes[1];

    // Hide panel and give space back to map
    m_editPanel->hide();
    sizes[1] = 0;
    sizes[2] = sizes[2] + editWidth;
    m_splitter->setSizes(sizes);
}

// ============================================================================
// Emergency Lifecycle
// ============================================================================

void MainWindow::onStartEmergency()
{
    bool ok;
    QString name = QInputDialog::getText(
        this,
        tr("Start Emergency"),
        tr("Emergency name:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty())
    {
        return;
    }

    EmergencyManager::instance()->startEmergency(name.trimmed());
    statusBar()->showMessage(tr("Emergency \"%1\" started").arg(name.trimmed()), 5000);
}

void MainWindow::onEndEmergency()
{
    QString emergencyName = EmergencyManager::instance()->response().name();

    // Custom dialog with Archive & End, Discard & End, Cancel
    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("End Emergency"));
    dialog.setText(tr("End \"%1\"?").arg(emergencyName));
    dialog.setIcon(QMessageBox::Question);

    QCheckBox* pdfCheckBox = new QCheckBox(tr("Generate summary report (PDF)"));
    pdfCheckBox->setChecked(true);
    dialog.setCheckBox(pdfCheckBox);

    QPushButton* archiveButton = dialog.addButton(tr("Archive && End"), QMessageBox::AcceptRole);
    QPushButton* discardButton = dialog.addButton(tr("Discard && End"), QMessageBox::DestructiveRole);
    dialog.addButton(QMessageBox::Cancel);

    dialog.exec();

    QAbstractButton* clicked = dialog.clickedButton();
    bool shouldEnd = false;
    bool archive = false;

    if (clicked == archiveButton)
    {
        shouldEnd = true;
        archive = true;
    }
    else if (clicked == discardButton)
    {
        QMessageBox::StandardButton confirm = QMessageBox::warning(
            this,
            tr("Discard Response Data"),
            tr("This will permanently delete all response data for this emergency. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (confirm == QMessageBox::Yes)
        {
            shouldEnd = true;
            archive = false;
        }
    }

    if (!shouldEnd)
    {
        return;
    }

    // Generate PDF report before ending (data is cleared on end)
    if (pdfCheckBox->isChecked())
    {
        if (!generateEmergencyReportWithConfirm())
        {
            return;
        }
    }

    EmergencyManager::instance()->endEmergency(archive);
    if (archive)
    {
        statusBar()->showMessage(tr("Emergency \"%1\" archived").arg(emergencyName), 5000);
    }
    else
    {
        statusBar()->showMessage(tr("Emergency \"%1\" ended").arg(emergencyName), 5000);
    }
}

void MainWindow::generateEmergencyReport()
{
    generateEmergencyReportWithConfirm();
}

bool MainWindow::generateEmergencyReportWithConfirm()
{
    if (!EmergencyManager::instance()->isActive())
    {
        return false;
    }

    const EmergencyResponse& response = EmergencyManager::instance()->response();

    // Default filename based on emergency name and date
    QString defaultName = response.name().simplified().replace(' ', '_')
        + "_" + response.startedAt().toString("yyyy-MM-dd")
        + ".pdf";

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Emergency Report"), defaultDir + "/" + defaultName,
        tr("PDF Files (*.pdf)"));

    if (filePath.isEmpty())
    {
        // User cancelled — ask if they want to skip the report
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Skip Report"),
            tr("No report will be generated. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        return answer == QMessageBox::Yes;
    }

    // Get ward name from document metadata
    QString wardName;
    const QHash<QString, Ward>& wards = DocumentManager::instance()->document().wards();
    if (!wards.isEmpty())
    {
        wardName = wards.constBegin().value().name();
        if (!wardName.isEmpty())
        {
            wardName += tr(" Ward");
        }
    }

    if (ReportGenerator::generateReport(response, wardName, filePath))
    {
        statusBar()->showMessage(tr("Report saved to %1").arg(filePath), 5000);
        return true;
    }

    QMessageBox::warning(this, tr("Report Error"),
                         tr("Failed to generate the report."));
    return false;
}

void MainWindow::onEmergencyStarted()
{
    m_emergencyBanner->setEmergencyName(EmergencyManager::instance()->response().name());
    m_taskListView->rebuild();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
    updateEmergencyActions();
}

void MainWindow::onEmergencyEnded()
{
    m_emergencyBanner->clearBanner();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);
    updateEmergencyActions();
}

void MainWindow::onArchiveViewOpened()
{
    m_emergencyBanner->setArchiveName(EmergencyManager::instance()->response().name());
    m_taskListView->rebuild();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
    updateEmergencyActions();
}

void MainWindow::onArchiveViewClosed()
{
    if (EmergencyManager::instance()->isActive())
    {
        m_emergencyBanner->setEmergencyName(EmergencyManager::instance()->response().name());
    }
    else
    {
        m_emergencyBanner->clearBanner();
    }
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, EmergencyManager::instance()->isActive());
    if (EmergencyManager::instance()->isActive())
    {
        m_taskListView->rebuild();
    }
    updateEmergencyActions();
}

void MainWindow::onOpenArchive()
{
    QString docPath = DocumentManager::instance()->filePath();
    if (docPath.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("No Document"),
            tr("Please save your document first to view archives."));
        return;
    }

    QFileInfo docInfo(docPath);
    QString archiveDir = docInfo.absolutePath() + "/" + docInfo.completeBaseName() + "_archives";

    ArchiveBrowserDialog dialog(archiveDir, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString selectedPath = dialog.selectedFilePath();
    if (selectedPath.isEmpty())
    {
        return;
    }

    switch (dialog.selectedAction())
    {
    case ArchiveBrowserDialog::Action::View:
    {
        QString errorMessage;
        if (!EmergencyManager::instance()->loadArchive(selectedPath, &errorMessage))
        {
            QMessageBox::critical(this, tr("Open Archive Failed"), errorMessage);
        }
        break;
    }

    case ArchiveBrowserDialog::Action::Reopen:
    {
        QMessageBox::StandardButton confirm = QMessageBox::question(
            this,
            tr("Reopen Emergency"),
            tr("This will restore this emergency as active.\n\n"
               "Any current emergency will be ended and archived first.\n\n"
               "Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (confirm != QMessageBox::Yes)
        {
            break;
        }

        // End current emergency first (always archive to preserve data)
        if (EmergencyManager::instance()->isActive())
        {
            EmergencyManager::instance()->endEmergency(true);

            // If still active, archive save failed — abort reopen
            if (EmergencyManager::instance()->isActive())
            {
                break;
            }
        }

        QString errorMessage;
        if (!EmergencyManager::instance()->reopenArchive(selectedPath, &errorMessage))
        {
            QMessageBox::critical(this, tr("Reopen Failed"), errorMessage);
        }
        else
        {
            statusBar()->showMessage(
                tr("Emergency \"%1\" reopened").arg(EmergencyManager::instance()->response().name()),
                5000);
        }
        break;
    }

    case ArchiveBrowserDialog::Action::None:
        break;
    }
}

void MainWindow::updateEmergencyActions()
{
    bool active = EmergencyManager::instance()->isActive();
    bool viewing = EmergencyManager::instance()->isViewingArchive();
    bool hasDocument = !DocumentManager::instance()->filePath().isEmpty();
    m_startEmergencyAction->setEnabled(!active && !viewing);
    m_endEmergencyAction->setEnabled(active && !viewing);
    m_generateReportAction->setEnabled(active && !viewing);
    m_openArchiveAction->setEnabled(hasDocument && !viewing);
}
