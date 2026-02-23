#include "MainWindow.h"
#include "WardListView.h"
#include "MinisteringView.h"
#include "NeedsSubView.h"
#include "SidebarWidget.h"
#include "EmergencyAssetView.h"
#include "ResponseArea.h"
#include "FamilyEditPanel.h"
#include "MapWidget.h"
#include "FamilyMarkerProvider.h"
#include "DocumentManager.h"
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
#include <QMessageBox>
#include <QCloseEvent>
#include <QLabel>
#include <QProgressBar>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

/// Placeholder widget that implements FamilyMarkerProvider with no-op defaults.
/// Used for tabs that don't yet have full implementation (e.g., Teams).
class PlaceholderView : public QWidget, public FamilyMarkerProvider
{
public:
    explicit PlaceholderView(const QString& message, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        QLabel* label = new QLabel(message);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
    }

    HighlightInfo highlightInfo() const override { return {}; }
    QSet<FamilyId> visibleFamilyIds() const override { return {}; }
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_documentManager(new DocumentManager(this))
{
    setupUi();
    setupMenus();
    setupConnections();
    initializeDefaultLocation();
    updateWindowTitle();
    updateUndoRedoActions();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    resize(1400, 900);

    // Central widget with splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    // Sidebar navigation (two-row button bar)
    m_sidebarTabs = new SidebarWidget(4, m_splitter);

    // Row 1
    m_wardListView = new WardListView(m_documentManager, this);
    m_sidebarTabs->addPage(m_wardListView, tr("Families"));

    m_ministeringView = new MinisteringView(m_documentManager, this);
    m_sidebarTabs->addPage(m_ministeringView, tr("Ministering"));

    PlaceholderView* teamsPlaceholder = new PlaceholderView(tr("Teams functionality coming soon"));
    m_sidebarTabs->addPage(teamsPlaceholder, tr("Teams"));

    m_needsView = new NeedsSubView(m_documentManager, this);
    m_sidebarTabs->addPage(m_needsView, tr("Needs"));

    // Row 2
    m_medicalView = new EmergencyAssetView(m_documentManager, ResponseArea::Medical, this);
    m_sidebarTabs->addPage(m_medicalView, tr("Medical"));

    m_commsView = new EmergencyAssetView(m_documentManager, ResponseArea::Communications, this);
    m_sidebarTabs->addPage(m_commsView, tr("Communications"));

    m_recoveryView = new EmergencyAssetView(m_documentManager, ResponseArea::Recovery, this);
    m_sidebarTabs->addPage(m_recoveryView, tr("Skills && Gear"));

    // Edit panel (initially hidden)
    m_editPanel = new FamilyEditPanel(m_documentManager, m_splitter);
    m_editPanel->hide();

    // Map in the center
    m_mapWidget = new MapWidget(m_documentManager, m_splitter);

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
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &MainWindow::onDocumentChanged);
    connect(m_documentManager, &DocumentManager::dirtyChanged,
            this, &MainWindow::updateWindowTitle);
    connect(m_documentManager, &DocumentManager::filePathChanged,
            this, &MainWindow::updateWindowTitle);
    connect(m_documentManager, &DocumentManager::canUndoChanged,
            this, &MainWindow::updateUndoRedoActions);
    connect(m_documentManager, &DocumentManager::canRedoChanged,
            this, &MainWindow::updateUndoRedoActions);

    // Sidebar tab changes
    connect(m_sidebarTabs, &SidebarWidget::currentChanged,
            this, &MainWindow::onSidebarTabChanged);

    // Map <-> WardListView selection sync
    connect(m_mapWidget, &MapWidget::familyClicked,
            m_wardListView, &WardListView::setSelectedFamilyId);
    connect(m_mapWidget, &MapWidget::mapDeselected,
            m_wardListView, &WardListView::clearSelection);

    // Highlight and visibility changes from sidebar views
    connect(m_wardListView, &WardListView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_wardListView, &WardListView::visibleFamiliesChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_ministeringView, &MinisteringView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_needsView, &NeedsSubView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_medicalView, &EmergencyAssetView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_commsView, &EmergencyAssetView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
    connect(m_recoveryView, &EmergencyAssetView::highlightChanged,
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
    connect(m_documentManager, &DocumentManager::geocodingProgressChanged,
            this, &MainWindow::onGeocodingProgress);
    connect(m_documentManager, &DocumentManager::geocodingFinished,
            this, &MainWindow::onGeocodingFinished);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_documentManager->isDirty())
    {
        return true;
    }

    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("The document has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (result == QMessageBox::Save)
    {
        onSaveDocument();
        return !m_documentManager->isDirty();
    }
    else if (result == QMessageBox::Discard)
    {
        return true;
    }

    return false;
}

void MainWindow::onNewDocument()
{
    if (!maybeSave())
    {
        return;
    }

    m_documentManager->newDocument();
    statusBar()->showMessage(tr("New document created"), 3000);
}

void MainWindow::onOpenDocument()
{
    if (!maybeSave())
    {
        return;
    }

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
    if (m_documentManager->openDocument(filePath, &errorMessage))
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
    if (m_documentManager->filePath().isEmpty())
    {
        onSaveDocumentAs();
        return;
    }

    QString errorMessage;
    if (m_documentManager->saveDocument(&errorMessage))
    {
        statusBar()->showMessage(tr("Document saved"), 3000);
    }
    else
    {
        QMessageBox::warning(this, tr("Error"), errorMessage);
    }
}

void MainWindow::onSaveDocumentAs()
{
    // Build suggested path: use existing path, or suggested filename
    QString suggestedPath;
    if (!m_documentManager->filePath().isEmpty())
    {
        suggestedPath = m_documentManager->filePath();
    }
    else
    {
        QString suggested = m_documentManager->document().suggestedFilename();
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
    if (m_documentManager->saveDocumentAs(filePath, &errorMessage))
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
    WardDirectoryImportService importService;
    WardDirectoryImportResult result = importService.importFromPdf(
        filePath,
        m_documentManager->document().families(),
        m_documentManager->document().ministeringPdfDate());

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

    m_documentManager->executeCommand(std::make_unique<ImportWardDirectoryCommand>(
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
    m_documentManager->startBatchGeocoding();
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

    MinisteringImportService importService;
    MinisteringImportResult result = importService.importFromPdf(
        filePath,
        m_documentManager->document().families(),
        m_documentManager->document().wardDirectoryPdfDate());

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
        m_documentManager->executeCommand(std::make_unique<ImportRSMinisteringCommand>(
            result.districts,
            result.groups,
            result.families,
            result.pdfDate,
            description));
    }
    else
    {
        m_documentManager->executeCommand(std::make_unique<ImportEQMinisteringCommand>(
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
    m_documentManager->startBatchGeocoding();
}

void MainWindow::onUndo()
{
    m_documentManager->undo();
}

void MainWindow::onRedo()
{
    m_documentManager->redo();
}

void MainWindow::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    int count = m_documentManager->document().families().size();
    statusBar()->showMessage(tr("%1 families").arg(count));
}

void MainWindow::updateWindowTitle()
{
    QString title = "Emergency Plan";

    QString filePath = m_documentManager->filePath();
    if (!filePath.isEmpty())
    {
        QFileInfo fileInfo(filePath);
        title = fileInfo.completeBaseName() + " - " + title;
    }
    else
    {
        QString suggested = m_documentManager->document().suggestedFilename();
        if (!suggested.isEmpty())
        {
            title = suggested + " - " + title;
        }
        else
        {
            title = "Untitled - " + title;
        }
    }

    if (m_documentManager->isDirty())
    {
        title = "* " + title;
    }

    setWindowTitle(title);
}

void MainWindow::updateUndoRedoActions()
{
    m_undoAction->setEnabled(m_documentManager->canUndo());
    m_redoAction->setEnabled(m_documentManager->canRedo());

    QString undoText = tr("&Undo");
    if (m_documentManager->canUndo())
    {
        undoText += " " + m_documentManager->undoDescription();
    }
    m_undoAction->setText(undoText);

    QString redoText = tr("&Redo");
    if (m_documentManager->canRedo())
    {
        redoText += " " + m_documentManager->redoDescription();
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

// ============================================================================
// Default Location / Settings
// ============================================================================

QString MainWindow::settingsFilePath()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cacheDir + "/settings.json";
}

bool MainWindow::loadDefaultLocation(double& lat, double& lng)
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
    {
        return false;
    }

    QJsonObject json = doc.object();
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
    // Ensure cache directory exists
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);

    QJsonObject json;
    json["defaultLat"] = lat;
    json["defaultLng"] = lng;

    QFile file(settingsFilePath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(json).toJson());
    }
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
    const auto& families = m_documentManager->document().families();
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
    const auto& families = m_documentManager->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    Family originalFamily = it.value();

    // Execute update command
    m_documentManager->executeCommand(std::make_unique<UpdateFamilyCommand>(
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
    const auto& families = m_documentManager->document().families();
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

