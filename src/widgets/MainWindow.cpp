#include "MainWindow.h"
#include "WardListView.h"
#include "MapWidget.h"
#include "DocumentManager.h"
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

    // Ward list on the left
    m_wardListView = new WardListView(m_splitter);
    m_wardListView->setup(m_documentManager);
    m_wardListView->setMinimumWidth(250);
    m_wardListView->setMaximumWidth(400);

    // Map in the center
    m_mapWidget = new MapWidget(m_documentManager, m_splitter);
    m_mapWidget->setMinimumWidth(400);

    m_splitter->addWidget(m_wardListView);
    m_splitter->addWidget(m_mapWidget);
    m_splitter->setSizes({300, 1100});
    m_splitter->setStretchFactor(0, 0);  // List doesn't stretch
    m_splitter->setStretchFactor(1, 1);  // Map stretches

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

    m_importPdfAction = fileMenu->addAction(tr("Import &Ward Directory PDF..."), this, &MainWindow::onImportPdf);
    m_importMinisteringPdfAction = fileMenu->addAction(tr("Import &Ministering PDF..."), this, &MainWindow::onImportMinisteringPdf);

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

    // Map <-> WardListView selection sync
    connect(m_wardListView, &WardListView::familySelected,
            m_mapWidget, &MapWidget::centerOnFamily);
    connect(m_mapWidget, &MapWidget::familyClicked,
            m_wardListView, &WardListView::setSelectedFamilyId);
    connect(m_wardListView, &WardListView::visibleFamiliesChanged,
            m_mapWidget, &MapWidget::setVisibleFamilyIds);

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

    // Parse PDF
    WardDirectoryImportService importService;
    WardDirectoryImportResult result = importService.importFromPdf(filePath);

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
        result.wardUnitNumber,
        result.wardName,
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
        filePath, m_documentManager->document().families());

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
            description));
    }
    else
    {
        m_documentManager->executeCommand(std::make_unique<ImportEQMinisteringCommand>(
            result.districts,
            result.groups,
            result.families,
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

void MainWindow::onDocumentChanged()
{
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

