#include "MainWindow.h"
#include "AppSettings.h"
#include "DocumentLibrary.h"
#include "DeviceLinks.h"
#include "FileOpenRouter.h"
#include "IncomingWards.h"
#include "OpenWardDialog.h"
#include "AccessDialog.h"
#include "WardNotices.h"
#include "ConflictDialog.h"
#include "SyncStatusWidget.h"
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
#include "DocumentConstants.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "ReportGenerator.h"
#include "Ward.h"
#include "FamilyCommands.h"
#include "ImportWardDirectoryCommand.h"
#include "ImportMinisteringCommand.h"
#include "ItemType.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"
#include "WardDirectoryImportService.h"
#include "MinisteringImportService.h"
#include "GeoLocation.h"

#include <QMenuBar>
#include <QClipboard>
#include <QGuiApplication>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QLocale>

MainWindow::MainWindow(AppSettings& settings,
                       DocumentLibrary& library,
                       IncomingWards& incoming,
                       DeviceLinks& links,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_library(library)
    , m_requests(incoming)
    , m_links(links)
{
    setupUi();
    setupMenus();
    setupConnections();

    initializeDefaultLocation();
    updateWindowTitle();
    updateUndoRedoActions();
}

void MainWindow::handleArguments(const QStringList& arguments)
{
    // Bring this window forward, for a later launch that handed its arguments here.
    setWindowState(windowState() & ~Qt::WindowMinimized);
    raise();
    activateWindow();

    // Files of any other extension are other applications'.
    for (qsizetype i = 1; i < arguments.size(); ++i)
    {
        const QString invited = FileOpenRouter::invitedWard(arguments.at(i));
        if (!invited.isEmpty())
        {
            followRoute(FileOpenRouter::routeInvite(invited, m_library), QString());
        }
        else if (arguments.at(i).endsWith(DocumentConstants::FileExtension, Qt::CaseInsensitive))
        {
            openPath(arguments.at(i));
        }
    }
}

void MainWindow::openPath(const QString& path)
{
    followRoute(FileOpenRouter::route(path, m_library, DocumentManager::instance()->identity()), path);
}

void MainWindow::followRoute(const FileOpenRouter::Route& route, const QString& path)
{
    switch (route.action)
    {
    case FileOpenRouter::Action::OpenWard:
        openWard(route.documentId);
        break;
    case FileOpenRouter::Action::OpenCopy:
        openCopy(path);
        break;
    case FileOpenRouter::Action::AddWard:
    {
        QString error;
        if (!m_library.addFromFile(route.documentId, path, &error))
        {
            QMessageBox::warning(this, tr("Ward Planning"), error);
            break;
        }
        openWard(route.documentId);
        break;
    }
    case FileOpenRouter::Action::RequestAccess:
        offerRequest(route.documentId);
        break;
    case FileOpenRouter::Action::NeedsNewerVersion:
        QMessageBox::warning(this, tr("Ward Planning"), tr("%1 was written by a newer version of Ward Planning.").arg(path));
        break;
    case FileOpenRouter::Action::NotAWardPlanningFile:
        QMessageBox::warning(this, tr("Ward Planning"), tr("%1 is not a Ward Planning file.").arg(path));
        break;
    case FileOpenRouter::Action::Unreadable:
        QMessageBox::warning(this, tr("Ward Planning"), tr("%1 could not be read.").arg(path));
        break;
    }
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
    m_copyBanner = new QLabel(centralContainer);
    m_copyBanner->setObjectName("copyBanner");
    m_copyBanner->setWordWrap(true);
    m_copyBanner->setContentsMargins(12, 6, 12, 6);
    m_copyBanner->hide();
    m_splitter = new QSplitter(Qt::Horizontal, centralContainer);

    centralLayout->addWidget(m_emergencyBanner);
    centralLayout->addWidget(m_copyBanner);

    // The ward's standing notices, under the emergency and copy banners.
    m_wardNotices = new WardNotices(centralContainer);
    centralLayout->addWidget(m_wardNotices);
    connect(m_wardNotices, &WardNotices::accessRequested, this, &MainWindow::onShowAccess);
    connect(m_wardNotices, &WardNotices::linkRequested, this, &MainWindow::onLinkDevice);
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
    statusBar()->addPermanentWidget(new SyncStatusWidget(this));
    m_geocodingLabel->hide();
    m_geocodingProgress->hide();
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    m_newAction = fileMenu->addAction(tr("&New Ward"), this, &MainWindow::onNewWard);
    m_newAction->setShortcut(QKeySequence::New);

    m_openAction = fileMenu->addAction(tr("&Open Ward..."), this, &MainWindow::onOpenWard);
    m_openAction->setShortcut(QKeySequence::Open);

    m_saveAction = fileMenu->addAction(tr("&Save Now"), this, &MainWindow::onSaveNow);
    m_saveAction->setShortcut(QKeySequence::Save);

    fileMenu->addSeparator();

    fileMenu->addAction(tr("Save a &Copy..."), this, &MainWindow::onSaveCopy);
    fileMenu->addAction(tr("Open a Cop&y..."), this, &MainWindow::onOpenCopy);
    fileMenu->addAction(tr("Show in &Folder"), this, &MainWindow::onShowFolder);

    fileMenu->addSeparator();

    QMenu* importMenu = fileMenu->addMenu(tr("&Import"));
    m_importPdfAction = importMenu->addAction(tr("&Ward Directory PDF..."), this, &MainWindow::onImportPdf);
    m_importMinisteringPdfAction = importMenu->addAction(tr("&Ministering PDF..."), this, &MainWindow::onImportMinisteringPdf);

    fileMenu->addSeparator();

    // A conflict set aside by closing its dialog waits here.
    m_resolveConflictAction = fileMenu->addAction(tr("Choose Between Two Versions..."), this, &MainWindow::askAboutConflict);

    // Only before this device holds any ward: a device set up as the wrong person.
    m_startOverAction = fileMenu->addAction(tr("Set Up This Device Again..."), this, &MainWindow::onStartOver);
    connect(fileMenu, &QMenu::aboutToShow, this, &MainWindow::updateFileActions);

    fileMenu->addSeparator();

    m_exitAction = fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    m_undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::onUndo);
    m_undoAction->setShortcut(QKeySequence::Undo);

    m_redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::onRedo);
    m_redoAction->setShortcut(QKeySequence::Redo);

    // Ward menu: who has access, and the actions on it
    QMenu* wardMenu = menuBar()->addMenu(tr("&Ward"));
    wardMenu->addAction(tr("&Who Has Access..."), this, &MainWindow::onShowAccess);

    // Emergency menu
    QMenu* emergencyMenu = menuBar()->addMenu(tr("E&mergency"));

    m_startEmergencyAction = emergencyMenu->addAction(tr("&Start Emergency..."), this, &MainWindow::onStartEmergency);
    m_endEmergencyAction = emergencyMenu->addAction(tr("&End Emergency..."), this, &MainWindow::onEndEmergency);
    m_endEmergencyAction->setEnabled(false);

    emergencyMenu->addSeparator();

    m_generateReportAction = emergencyMenu->addAction(tr("&Generate Report..."), this, &MainWindow::generateEmergencyReport);
    m_generateReportAction->setEnabled(false);
}

void MainWindow::setupConnections()
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &MainWindow::onDocumentChanged);
    connect(DocumentManager::instance(), &DocumentManager::documentOpened,
            this, &MainWindow::updateWindowTitle);
    connect(DocumentManager::instance(), &DocumentManager::documentOpened,
            this, &MainWindow::onDocumentOpened);
    connect(DocumentManager::instance(), &DocumentManager::editRefused,
            this, &MainWindow::onEditRefused);
    connect(DocumentManager::instance(), &DocumentManager::fetchFinished,
            this, &MainWindow::onFetchFinished);
    connect(DocumentManager::instance(), &DocumentManager::accessLost,
            this, &MainWindow::onAccessLost);
    connect(DocumentManager::instance(), &DocumentManager::shared,
            this, &MainWindow::showInviteLink, Qt::QueuedConnection);
    connect(DocumentManager::instance(), &DocumentManager::shareFailed,
            this, &MainWindow::onShareFailed);
    connect(DocumentManager::instance(), &DocumentManager::accessCallFailed,
            this, &MainWindow::onAccessCallFailed);
    connect(DocumentManager::instance(), &DocumentManager::grantBlocked, this, &MainWindow::onGrantBlocked);
    connect(&m_requests, &IncomingWards::requested, this, &MainWindow::onRequested);
    connect(&m_requests, &IncomingWards::requestFailed, this, &MainWindow::onRequestFailed);
    connect(&m_requests, &IncomingWards::fetched, this, &MainWindow::onWardRestored);
    connect(&m_links, &DeviceLinks::linkRead, this, &MainWindow::onLinkRead);
    connect(&m_links, &DeviceLinks::approved, this, &MainWindow::onLinkApproved);
    connect(&m_links, &DeviceLinks::unlinked, this, &MainWindow::onUnlinked);
    connect(&m_links, &DeviceLinks::failed, this, &MainWindow::onLinkFailed);
    connect(&m_requests, &IncomingWards::declined, this, &MainWindow::onRequestDeclined);
    connect(DocumentManager::instance(), &DocumentManager::fetchFailed,
            this, &MainWindow::onFetchFailed);
    connect(DocumentManager::instance(), &DocumentManager::conflictRaised,
            this, &MainWindow::askAboutConflict, Qt::QueuedConnection);
    connect(DocumentManager::instance(), &DocumentManager::canUndoChanged,
            this, &MainWindow::updateUndoRedoActions);
    connect(DocumentManager::instance(), &DocumentManager::canRedoChanged,
            this, &MainWindow::updateUndoRedoActions);

    // Emergency lifecycle
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &MainWindow::onEmergencyStarted);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &MainWindow::onEmergencyEnded);
    connect(EmergencyManager::instance(), &EmergencyManager::responseDataChanged,
            m_mapWidget, &MapWidget::updateHighlights);
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

    // A local write that failed is the one modal failure.
    connect(DocumentManager::instance(), &DocumentManager::saveFailed,
            this, &MainWindow::onSaveFailed);
}

void MainWindow::onNewWard()
{
    QString error;
    if (!DocumentManager::instance()->newDocument(&error))
    {
        QMessageBox::warning(this, tr("New Ward"), error);
        return;
    }
    statusBar()->showMessage(tr("New ward created"), 3000);
}

void MainWindow::onOpenWard()
{
    OpenWardDialog dialog(m_library.entries(), m_requests.pending(), m_requests.waiting(), this);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedDocumentId().isEmpty())
    {
        openWard(dialog.selectedDocumentId());
    }
}

void MainWindow::openWard(const QString& documentId)
{
    QString reason;
    reportOpen(DocumentManager::instance()->openDocument(documentId, &reason), reason);
}

void MainWindow::onWardRestored(const QString& documentId, DocumentSession::Download outcome)
{
    switch (outcome)
    {
    case DocumentSession::Download::Done:
    {
        const QString name = DocumentManager::instance()->wardNameOf(documentId);
        statusBar()->showMessage(tr("%1 restored from the server").arg(name), 5000);

        // The first to arrive opens, unless something is open or on its way.
        const DocumentManager* manager = DocumentManager::instance();
        if (manager->documentId().isEmpty() && !manager->copy().has_value() && !manager->isFetching())
        {
            openWard(documentId);
        }
        break;
    }
    case DocumentSession::Download::NeedsNewerVersion:
        statusBar()->showMessage(tr("A ward shared with you needs a newer version of Ward Planning."), 10000);
        break;
    case DocumentSession::Download::WriteFailed:
        statusBar()->showMessage(tr("A ward shared with you could not be saved on this device."), 10000);
        break;
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::AccessLost:
    case DocumentSession::Download::ServerFault:
    case DocumentSession::Download::NotYetSealed:
        // IncomingWards keeps these to itself and tries again.
        break;
    }
}

void MainWindow::onFetchFinished(const QString& documentId, DocumentManager::Open result, const QString& reason)
{
    Q_UNUSED(documentId);
    statusBar()->clearMessage();
    reportOpen(result, reason);
}

void MainWindow::reportOpen(DocumentManager::Open result, const QString& reason)
{
    switch (result)
    {
    case DocumentManager::Open::Opened:
        statusBar()->showMessage(tr("Ward opened"), 3000);
        break;
    case DocumentManager::Open::Fetching:
        statusBar()->showMessage(tr("Fetching the ward from the server..."));
        break;
    case DocumentManager::Open::NotFound:
        QMessageBox::warning(this, tr("Open Ward"), tr("This ward's file is missing from this device."));
        break;
    case DocumentManager::Open::NeedsNewerVersion:
        QMessageBox::warning(this, tr("Open Ward"), tr("This ward was saved by a newer version of Ward Planning."));
        break;
    case DocumentManager::Open::Refused:
        QMessageBox::warning(this, tr("Open Ward"), tr("This ward's file on this device cannot be opened. %1").arg(reason));
        break;
    case DocumentManager::Open::NotClosed:
        QMessageBox::warning(this, tr("Open Ward"), tr("The open ward could not be saved, so it stays open. %1").arg(reason));
        break;
    }
}

void MainWindow::onSaveNow()
{
    // Only a local failure is reported here; the sync indicator says the rest.
    QString error;
    if (!DocumentManager::instance()->publishNow(&error))
    {
        QMessageBox::warning(this, tr("Save"), error);
    }
}

void MainWindow::onSaveCopy()
{
    const QString suggested = DocumentManager::instance()->document().suggestedFilename();
    const QString targetPath = QFileDialog::getSaveFileName(
        this,
        tr("Save a Copy"),
        suggested.isEmpty() ? QString() : suggested + DocumentConstants::FileExtension,
        tr("Ward Planning Files (%1)").arg(DocumentConstants::FileGlob));
    if (targetPath.isEmpty())
    {
        return;
    }

    QString error;
    if (!m_library.exportCopy(DocumentManager::instance()->documentId(),
                              DocumentManager::instance()->document(),
                              targetPath,
                              &error))
    {
        QMessageBox::warning(this, tr("Save a Copy"), error);
        return;
    }
    statusBar()->showMessage(tr("Copy saved to %1").arg(targetPath), 5000);
}

void MainWindow::onOpenCopy()
{
    const QString sourcePath = QFileDialog::getOpenFileName(
        this,
        tr("Open a Copy"),
        QString(),
        tr("Ward Planning Files (%1)").arg(DocumentConstants::FileGlob));
    if (!sourcePath.isEmpty())
    {
        openCopy(sourcePath);
    }
}

void MainWindow::openCopy(const QString& sourcePath)
{
    QString error;
    const std::optional<DocumentLibrary::Copy> copy = m_library.openCopy(sourcePath, &error);
    if (!copy.has_value())
    {
        QMessageBox::warning(this, tr("Open a Copy"), error);
        return;
    }
    if (!DocumentManager::instance()->showCopy(copy.value(), &error))
    {
        QMessageBox::warning(this, tr("Open a Copy"), tr("The open ward could not be saved, so it stays open. %1").arg(error));
    }
}

void MainWindow::onShowFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_library.directory()));
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

    if (!DocumentManager::instance()->executeCommand(std::make_unique<ImportWardDirectoryCommand>(
            result.families,
            result.removedFamilyIds,
            result.wardUnitNumber,
            result.wardName,
            result.pdfDate,
            description)))
    {
        return;
    }

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

    MinisteringOrg org = result.isRSFormat
        ? MinisteringOrg::ReliefSociety
        : MinisteringOrg::EldersQuorum;
    if (!DocumentManager::instance()->executeCommand(std::make_unique<ImportMinisteringCommand>(
            org,
            result.districts,
            result.groups,
            result.families,
            result.pdfDate,
            description)))
    {
        return;
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
    int count = DocumentManager::instance()->document().families().size();
    statusBar()->showMessage(tr("%1 families").arg(count));

    // The undo text names the latest command; a replaced document may name its ward.
    updateUndoRedoActions();
    if (change.action == ChangeAction::Full)
    {
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle()
{
    // The ward's own name, as the ward list shows it; its file is named by id.
    const QString suggested = DocumentManager::instance()->document().suggestedFilename();
    setWindowTitle(tr("%1 - Ward Planning").arg(suggested.isEmpty() ? tr("Untitled") : suggested));
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

void MainWindow::onSaveFailed(const QString& errorMessage)
{
    QMessageBox::warning(this,
                         tr("Not Saved"),
                         tr("Your changes could not be saved on this device: %1\n\n"
                            "They are kept while Ward Planning stays open, and saving is tried again "
                            "with your next change.").arg(errorMessage));
}

void MainWindow::onDocumentOpened()
{
    updateEmergencyActions();
    updateCopyBanner();
    m_mapWidget->fitAllFamilies();
}

void MainWindow::updateCopyBanner()
{
    const std::optional<DocumentLibrary::Copy>& copy = DocumentManager::instance()->copy();
    if (!copy.has_value())
    {
        m_copyBanner->hide();
        return;
    }

    m_copyBanner->setText(tr("A read-only copy. Changes go in the ward itself."));
    m_copyBanner->show();
}

void MainWindow::onAccessLost(const QString& wardName, const ServerContract::LostAccess& lost)
{
    const QString ward = wardName.isEmpty() ? tr("a ward") : wardName;
    QString message;
    switch (lost.how)
    {
    case ServerContract::LostAccess::How::Left:
        message = tr("You left %1, so it is no longer on this device.").arg(ward);
        break;
    case ServerContract::LostAccess::How::Unlinked:
        unlinkedHere(lost);
        return;
    case ServerContract::LostAccess::How::Removed:
        message = lost.byWhom.isEmpty()
                      ? tr("Your access to %1 was removed, so it is no longer on this device.").arg(ward)
                      : tr("%1 removed your access to %2, so it is no longer on this device.").arg(lost.byWhom, ward);
        break;
    }
    QMessageBox::information(this, tr("Ward Planning"), message);
}

void MainWindow::onFetchFailed(const QString& documentId, DocumentSession::Download outcome)
{
    Q_UNUSED(documentId);
    statusBar()->clearMessage();
    switch (outcome)
    {
    case DocumentSession::Download::Unreachable:
        QMessageBox::warning(this,
                             tr("Open Ward"),
                             tr("The server could not be reached to fetch this ward. Try again when you are online."));
        break;
    case DocumentSession::Download::NeedsNewerVersion:
        QMessageBox::warning(this, tr("Open Ward"), tr("This ward was saved by a newer version of Ward Planning."));
        break;
    case DocumentSession::Download::WriteFailed:
        QMessageBox::warning(this,
                             tr("Open Ward"),
                             tr("This ward arrived from the server but could not be saved on this device."));
        break;
    case DocumentSession::Download::Done:
    case DocumentSession::Download::AccessLost:
    case DocumentSession::Download::ServerFault:
    case DocumentSession::Download::NotYetSealed:
        QMessageBox::warning(this,
                             tr("Open Ward"),
                             tr("The server's copy of this ward could not be checked, so it was not opened."));
        break;
    }
}

void MainWindow::updateFileActions()
{
    m_resolveConflictAction->setEnabled(DocumentManager::instance()->pendingConflict().has_value());
    m_startOverAction->setEnabled(m_library.entries().isEmpty());
}

void MainWindow::onEditRefused(const QString& message)
{
    statusBar()->showMessage(message, 8000);
}

void MainWindow::askAboutConflict()
{
    // Asked again with the reason until it is settled or set aside; a conflict whose
    // ward has closed since it was raised is gone, and nothing is asked.
    QString error;
    for (;;)
    {
        const std::optional<DocumentSession::Conflict> conflict = DocumentManager::instance()->pendingConflict();
        if (!conflict.has_value())
        {
            return;
        }

        ConflictDialog dialog(conflict.value(), error, this);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }

        error.clear();
        if (settleConflict(dialog.choice(), &error))
        {
            return;
        }
    }
}

bool MainWindow::settleConflict(ConflictDialog::Choice choice, QString* errorMessage)
{
    switch (choice)
    {
    case ConflictDialog::Choice::SaveMineAsCopy:
    {
        QString recoveryPath;
        if (!DocumentManager::instance()->saveRecoveryCopy(&recoveryPath, errorMessage)
            || !DocumentManager::instance()->resolveConflict(DocumentSession::Resolution::KeepTheirs, errorMessage))
        {
            return false;
        }
        QMessageBox::information(this,
                                 tr("Your Version Was Saved"),
                                 tr("Your version is saved as a copy at:\n%1\n\nUse File > Open a Copy to "
                                    "look at it; anything you want from it has to be entered again by hand.")
                                     .arg(recoveryPath));
        return true;
    }
    case ConflictDialog::Choice::KeepTheirs:
        return DocumentManager::instance()->resolveConflict(DocumentSession::Resolution::KeepTheirs, errorMessage);
    case ConflictDialog::Choice::KeepMine:
        return DocumentManager::instance()->resolveConflict(DocumentSession::Resolution::KeepMine, errorMessage);
    }
    return false;
}

void MainWindow::onShowAccess()
{
    DocumentManager* manager = DocumentManager::instance();
    if (manager->copy().has_value() || manager->session() == nullptr)
    {
        QMessageBox::information(this, tr("Who Has Access"), tr("Open a ward to see who has access to it."));
        return;
    }

    AccessDialog dialog(currentMembers(), manager->identity(), manager->session()->isShared(), this);

    connect(&dialog, &AccessDialog::shareRequested, manager, &DocumentManager::share);
    connect(&dialog, &AccessDialog::grantRequested, manager, &DocumentManager::grant);
    connect(&dialog, &AccessDialog::linkRequested, this, &MainWindow::onLinkDevice);
    connect(&dialog, &AccessDialog::unlinkRequested, this, &MainWindow::onUnlinkDevice);
    connect(&dialog, &AccessDialog::removeRequested, this, &MainWindow::onRemoveMember);
    connect(&dialog, &AccessDialog::roleRequested, manager, &DocumentManager::setRole);
    connect(&dialog, &AccessDialog::leaveRequested, &dialog, &QDialog::accept);
    connect(&dialog, &AccessDialog::leaveRequested, this, &MainWindow::onLeave, Qt::QueuedConnection);
    connect(&dialog, &AccessDialog::declineRequested, manager, &DocumentManager::decline);
    connect(manager, &DocumentManager::accessCallDone, &dialog, std::bind(&MainWindow::refreshAccessDialog, this, &dialog));
    manager->fetchRequests(&dialog, std::bind(&AccessDialog::setRequests, &dialog, std::placeholders::_1));
    connect(&dialog, &AccessDialog::inviteLinkRequested, this, std::bind(&MainWindow::showInviteLink, this, manager->documentId()));

    // Current while it is open: a head check's changes show, and another ward closes it.
    connect(manager, &DocumentManager::headChanged, &dialog, std::bind(&MainWindow::refreshAccessDialog, this, &dialog));
    connect(manager, &DocumentManager::documentOpened, &dialog, &QDialog::reject);
    connect(manager, &DocumentManager::shared, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::showInviteLink(const QString& documentId)
{
    const QString link = FileOpenRouter::inviteLink(documentId);

    QMessageBox message(QMessageBox::Information, tr("Invite Link"), tr("Send this link to someone you want to add."),
                        QMessageBox::Close, this);
    message.setInformativeText(tr("%1\n\nWhoever opens it can ask for access. You or another admin then grant it "
                                  "from Who Has Access.").arg(link));
    message.setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* copy = message.addButton(tr("Copy Link"), QMessageBox::ActionRole);
    message.exec();
    if (message.clickedButton() == copy)
    {
        QGuiApplication::clipboard()->setText(link);
        statusBar()->showMessage(tr("Invite link copied"), 5000);
    }
}

void MainWindow::onShareFailed(ServerContract::ServerError error)
{
    switch (error)
    {
    case ServerContract::ServerError::Unreachable:
    case ServerContract::ServerError::RateLimited:
        QMessageBox::warning(this, tr("Share"), tr("The server could not be reached. Connect to the internet and share again."));
        return;
    default:
        QMessageBox::warning(this, tr("Share"), tr("The server did not accept this ward, so it was not shared."));
        return;
    }
}

void MainWindow::refreshAccessDialog(AccessDialog* dialog)
{
    dialog->setMembers(currentMembers());
    DocumentManager::instance()->fetchRequests(dialog,
                                               std::bind(&AccessDialog::setRequests, dialog, std::placeholders::_1));
}

void MainWindow::offerRequest(const QString& documentId)
{
    if (m_requests.pending().contains(documentId))
    {
        QMessageBox::information(this,
                                 tr("Ward Planning"),
                                 tr("You have already asked for this ward. It appears here once an admin grants it."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Request Access"),
        tr("This ward is not on this device yet. Ask its admins for access?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer == QMessageBox::Yes)
    {
        m_requests.request(documentId);
    }
}

void MainWindow::onRequested(const QString& documentId)
{
    Q_UNUSED(documentId);
    QMessageBox::information(this,
                             tr("Request Access"),
                             tr("Your request was sent. The ward appears in your list once an admin grants it, "
                                "which they do from Who Has Access."));
}

void MainWindow::onRequestFailed(const QString& documentId, ServerContract::ServerError error)
{
    Q_UNUSED(documentId);
    if (error == ServerContract::ServerError::UnknownDocument)
    {
        QMessageBox::warning(this, tr("Request Access"), tr("The server has no such ward. It was deleted, or the link is wrong."));
        return;
    }
    QMessageBox::warning(this,
                         tr("Request Access"),
                         tr("The request could not be sent. Connect to the internet and open the link again."));
}

void MainWindow::onRequestDeclined(const QString& documentId)
{
    Q_UNUSED(documentId);
    QMessageBox::information(this, tr("Request Access"), tr("An admin declined your request for access to a ward."));
}

void MainWindow::onLinkDevice()
{
    bool entered = false;
    const QString code = QInputDialog::getText(this,
                                               tr("Link a Device"),
                                               tr("Enter the code your new device shows:"),
                                               QLineEdit::Normal,
                                               QString(),
                                               &entered);
    if (entered && !code.trimmed().isEmpty())
    {
        m_links.read(code);
    }
}

void MainWindow::onLinkRead(const QString& code, const ServerContract::LinkState& state)
{
    if (state.approvedAs.has_value())
    {
        QMessageBox::information(this, tr("Link a Device"), tr("That code has already been used."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Link a Device"),
        tr("Link \"%1\" to your account? It will open every ward you have, and you can unlink it later.")
            .arg(state.deviceName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer == QMessageBox::Yes)
    {
        m_links.approve(code, state.keys);
    }
}

void MainWindow::onLinkApproved(const QString& deviceName)
{
    statusBar()->showMessage(tr("%1 is linked. Your wards are on their way to it.").arg(deviceName), 10000);
    if (DocumentManager::instance()->session() != nullptr)
    {
        DocumentManager::instance()->session()->checkServer();
    }
}

void MainWindow::onUnlinkDevice(const Fingerprint& device)
{
    // This person's devices as the server last said, and which of them the lost one
    // linked: a device planted from a stolen phone should not outlive the phone.
    QList<ServerContract::Device> mine;
    for (const ServerContract::Member& member : currentMembers())
    {
        if (member.person.member == DocumentManager::instance()->identity().member)
        {
            mine = member.person.devices;
        }
    }

    QString name;
    QList<Fingerprint> alsoLinked;
    QStringList alsoNames;
    for (const ServerContract::Device& listed : mine)
    {
        if (listed.fingerprint == device)
        {
            name = listed.name;
        }
        else if (listed.linkedBy == device && listed.fingerprint != DocumentManager::instance()->identity().fingerprint())
        {
            alsoLinked.append(listed.fingerprint);
            alsoNames.append(tr("\"%1\" on %2").arg(listed.name,
                                                    QLocale().toString(listed.linkedAt.toLocalTime().date(),
                                                                       QLocale::ShortFormat)));
        }
    }

    QMessageBox confirm(QMessageBox::Question,
                        tr("Unlink"),
                        tr("Unlink \"%1\"? It loses every ward at once, and deletes them when it next connects.")
                            .arg(name),
                        QMessageBox::Cancel,
                        this);
    QPushButton* both = nullptr;
    if (!alsoLinked.isEmpty())
    {
        confirm.setInformativeText(tr("It also linked %1. Unlink those too?").arg(QLocale().createSeparatedList(alsoNames)));
        both = confirm.addButton(alsoLinked.size() == 1 ? tr("Unlink Both") : tr("Unlink All"), QMessageBox::AcceptRole);
        confirm.setDefaultButton(both);
    }
    QPushButton* justThis = confirm.addButton(alsoLinked.isEmpty() ? tr("Unlink") : tr("Just This One"),
                                              QMessageBox::DestructiveRole);
    confirm.exec();

    if (confirm.clickedButton() == both)
    {
        m_links.unlink(QList<Fingerprint>{device} + alsoLinked);
    }
    else if (confirm.clickedButton() == justThis)
    {
        m_links.unlink({device});
    }
}

void MainWindow::unlinkedHere(const ServerContract::LostAccess& lost)
{
    // Reported once, however many wards and calls hear it.
    if (m_unlinkedHere)
    {
        return;
    }
    m_unlinkedHere = true;

    DocumentManager::instance()->close(nullptr);
    for (const DocumentLibrary::Entry& entry : m_library.entries())
    {
        m_library.forgetLost(entry.documentId);
    }
    QMessageBox::information(this,
                             tr("Ward Planning"),
                             lost.byWhom.isEmpty()
                                 ? tr("This device was unlinked, so its wards have been deleted from it. Ward Planning "
                                      "will close; start it again to set this device up.")
                                 : tr("%1 unlinked this device, so its wards have been deleted from it. Ward Planning "
                                      "will close; start it again to set this device up.")
                                       .arg(lost.byWhom));
    emit startOverRequested();
}

void MainWindow::onRemoveMember(const MemberId& member)
{
    QString name;
    for (const ServerContract::Member& listed : currentMembers())
    {
        if (listed.person.member == member)
        {
            name = listed.person.displayName;
        }
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Remove"),
        tr("Remove %1? They lose access at once, and their app deletes its copy of this ward the next time it "
           "connects. A copy they took out of the app beforehand keeps what it had.")
            .arg(name),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
    {
        DocumentManager::instance()->remove(member);
    }
}

void MainWindow::onLeave()
{
    DocumentManager* manager = DocumentManager::instance();
    const QList<ServerContract::Member> members = currentMembers();
    const MemberId self = manager->identity().member;

    bool selfAdmin = false;
    int admins = 0;
    QList<ServerContract::Member> others;
    for (const ServerContract::Member& member : members)
    {
        admins += member.isAdmin() ? 1 : 0;
        if (member.person.member == self)
        {
            selfAdmin = member.isAdmin();
        }
        else
        {
            others.append(member);
        }
    }

    // The last admin names a successor, so the ward can still add and remove people.
    std::optional<MemberId> successor;
    if (selfAdmin && admins == 1 && !others.isEmpty())
    {
        // Each named with their devices, so two people sharing a name are told apart.
        QStringList labels;
        for (const ServerContract::Member& member : others)
        {
            QStringList devices;
            for (const ServerContract::Device& device : member.person.devices)
            {
                devices.append(device.name);
            }
            labels.append(tr("%1 (%2)").arg(member.person.displayName, QLocale().createSeparatedList(devices)));
        }

        bool chosen = false;
        const QString label = QInputDialog::getItem(this,
                                                    tr("Leave"),
                                                    tr("Before you leave, choose who will be admin:"),
                                                    labels,
                                                    0,
                                                    false,
                                                    &chosen);
        if (!chosen)
        {
            return;
        }
        successor = others.at(labels.indexOf(label)).person.member;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Leave"),
        others.isEmpty()
            ? tr("Leave this ward? You are the only one with access, so nobody will be able to open it again.")
            : tr("Leave this ward? Its copy on this device is deleted at once, and on your other devices when they "
                 "next connect."),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
    {
        manager->leave(successor);
    }
}

void MainWindow::onUnlinked(int count)
{
    statusBar()->showMessage(tr("%n device(s) unlinked", "", count), 10000);
    if (DocumentManager::instance()->session() != nullptr)
    {
        DocumentManager::instance()->session()->checkServer();
    }
}

void MainWindow::onGrantBlocked(const QString& reason)
{
    QMessageBox::warning(this, tr("Who Has Access"), tr("Nobody can be added yet. %1").arg(reason));
}

void MainWindow::onAccessCallFailed(ServerContract::ServerError error)
{
    QString message;
    switch (error)
    {
    case ServerContract::ServerError::NotAdmin:
    {
        QStringList admins;
        for (const ServerContract::Member& member : currentMembers())
        {
            if (member.isAdmin())
            {
                admins.append(member.person.displayName);
            }
        }
        message = admins.isEmpty()
                      ? tr("Only an admin can change who has access.")
                      : tr("Only an admin can change who has access. Ask %1.").arg(QLocale().createSeparatedList(admins));
        break;
    }
    case ServerContract::ServerError::LastAdmin:
        message = tr("Make someone else an admin first, so the ward can still add and remove people.");
        break;
    case ServerContract::ServerError::Unreachable:
    case ServerContract::ServerError::RateLimited:
        message = tr("Connect to the internet to change who has access.");
        break;
    case ServerContract::ServerError::NotFound:
        message = tr("That has already changed. Look again at Who Has Access.");
        break;
    default:
        message = tr("The server did not accept the change.");
        break;
    }
    QMessageBox::warning(this, tr("Who Has Access"), message);
}

void MainWindow::onLinkFailed(ServerContract::ServerError error)
{
    QString message;
    switch (error)
    {
    case ServerContract::ServerError::NotFound:
        message = tr("No device is showing that code. Check it, or start again on the new device.");
        break;
    case ServerContract::ServerError::Expired:
        message = tr("That code has expired. Start again on the new device.");
        break;
    case ServerContract::ServerError::Unreachable:
    case ServerContract::ServerError::RateLimited:
        message = tr("Connect to the internet to link or unlink a device.");
        break;
    default:
        message = tr("The server did not accept the change.");
        break;
    }
    QMessageBox::warning(this, tr("Link a Device"), message);
}

QList<ServerContract::Member> MainWindow::currentMembers() const
{
    const DocumentSession* session = DocumentManager::instance()->session();
    if (session == nullptr || !session->lastHead().has_value())
    {
        return {};
    }
    return session->lastHead().value().members;
}

void MainWindow::onStartOver()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Set Up This Device Again"),
        tr("This device will get a new identity, and Ward Planning will close. Start it again to "
           "answer the first question afresh.\n\nThis is for a device set up as the wrong person "
           "before it held any ward."),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Ok)
    {
        emit startOverRequested();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_editPanel->isVisible() && m_editPanel->isDirty())
    {
        const QMessageBox::StandardButton discard = QMessageBox::question(
            this,
            tr("Discard Changes"),
            tr("Discard changes to %1?").arg(m_editPanel->family().displayName()),
            QMessageBox::Yes | QMessageBox::No);
        if (discard != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
    }

    // Flushes the pending save and releases the lease.
    QString error;
    if (DocumentManager::instance()->close(&error))
    {
        event->accept();
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this,
        tr("Not Saved"),
        tr("Your latest changes could not be saved: %1\n\nClose anyway and lose them?").arg(error),
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Discard)
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

// ============================================================================
// Default Location / Settings
// ============================================================================

void MainWindow::initializeDefaultLocation()
{
    const std::optional<AppSettings::Location> location = m_settings.defaultLocation();
    if (location.has_value())
    {
        m_mapWidget->setCenter(location.value().latitude, location.value().longitude);
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
    m_settings.setDefaultLocation(AppSettings::Location{latitude, longitude});

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

    // A refused update keeps the panel open, with what was typed.
    if (DocumentManager::instance()->executeCommand(std::make_unique<UpdateFamilyCommand>(originalFamily, editedFamily)))
    {
        closeEditPanelInternal();
    }
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

    if (EmergencyManager::instance()->startEmergency(name.trimmed()))
    {
        statusBar()->showMessage(tr("Emergency \"%1\" started").arg(name.trimmed()), 5000);
    }
}

void MainWindow::onEndEmergency()
{
    QString emergencyName = EmergencyManager::instance()->response().name();

    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("End Emergency"));
    dialog.setText(tr("End \"%1\"?").arg(emergencyName));
    dialog.setInformativeText(
        tr("Ending the emergency clears its response data from the document. "
           "The summary report is the record that remains: a PDF, not encrypted, "
           "saved wherever you choose."));
    dialog.setIcon(QMessageBox::Question);

    QPushButton* reportButton = dialog.addButton(tr("Save Report && End"), QMessageBox::AcceptRole);
    QPushButton* endButton = dialog.addButton(tr("End Without Report"), QMessageBox::DestructiveRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(reportButton);

    dialog.exec();

    QAbstractButton* clicked = dialog.clickedButton();
    if (clicked == reportButton)
    {
        const std::optional<QString> reportPath = saveEmergencyReport(QDateTime::currentDateTimeUtc());
        if (!reportPath.has_value())
        {
            return;
        }
        if (EmergencyManager::instance()->endEmergency())
        {
            statusBar()->showMessage(
                tr("Emergency \"%1\" ended; report saved to %2").arg(emergencyName, reportPath.value()),
                5000);
        }
    }
    else if (clicked == endButton)
    {
        if (EmergencyManager::instance()->endEmergency())
        {
            statusBar()->showMessage(tr("Emergency \"%1\" ended").arg(emergencyName), 5000);
        }
    }
}

void MainWindow::generateEmergencyReport()
{
    const std::optional<QString> reportPath = saveEmergencyReport(std::nullopt);
    if (reportPath.has_value())
    {
        statusBar()->showMessage(tr("Report saved to %1").arg(reportPath.value()), 5000);
    }
}

std::optional<QString> MainWindow::saveEmergencyReport(const std::optional<QDateTime>& endedAt)
{
    if (!EmergencyManager::instance()->isActive())
    {
        return std::nullopt;
    }

    const EmergencyResponse& response = EmergencyManager::instance()->response();

    // Default filename based on emergency name and local start date
    QString defaultName = response.name().simplified().replace(' ', '_')
        + "_" + response.startedAt().toLocalTime().toString("yyyy-MM-dd")
        + ".pdf";

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Emergency Report"), defaultDir + "/" + defaultName,
        tr("PDF Files (*.pdf)"));

    if (filePath.isEmpty())
    {
        return std::nullopt;
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

    if (ReportGenerator::generateReport(response, endedAt, wardName, filePath))
    {
        return filePath;
    }

    QMessageBox::warning(this, tr("Report Error"),
                         tr("Failed to generate the report."));
    return std::nullopt;
}

void MainWindow::onEmergencyStarted()
{
    m_emergencyBanner->setEmergencyName(EmergencyManager::instance()->response().name());
    m_taskListView->rebuild();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
    m_splitter->setSizes({600, 0, 1100});
    updateEmergencyActions();
}

void MainWindow::onEmergencyEnded()
{
    m_emergencyBanner->clearBanner();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);
    m_splitter->setSizes({300, 0, 1100});
    updateEmergencyActions();
}

void MainWindow::updateEmergencyActions()
{
    bool active = EmergencyManager::instance()->isActive();
    m_startEmergencyAction->setEnabled(!active);
    m_endEmergencyAction->setEnabled(active);
    m_generateReportAction->setEnabled(active);
}
