#include "MainWindow.h"
#include "EmergencyBanner.h"
#include "FamilyListView.h"
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
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "ReportGenerator.h"
#include "AddressCoordinatesCommands.h"
#include "FamilyCommands.h"
#include "ImportWardDirectoryCommand.h"
#include "ImportMinisteringCommand.h"
#include "Document.h"
#include "ItemType.h"
#include "Family.h"
#include "WardDirectoryImportService.h"
#include "MinisteringImportService.h"
#include "GeoLocation.h"
#include "OpenWardDialog.h"
#include "ShowInFolder.h"
#include "UpdateFile.h"
#include "UserFolder.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QUrl>
#include <QMenuBar>
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
#include <QSysInfo>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

MainWindow::MainWindow(const QString& settingsFilePath, bool developer, QWidget* parent)
    : QMainWindow(parent)
    , m_settingsFilePath(settingsFilePath)
{
    setupUi();
    setupMenus(developer);
    setupConnections();
    setAcceptDrops(true);

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
    m_familyListView = new FamilyListView(this);
    m_sidebarTabs->addPage(m_familyListView, tr("Families"));

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

    // Set initial highlight provider (FamilyListView)
    m_mapWidget->setMarkerProvider(m_familyListView);

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

void MainWindow::setupMenus(bool developer)
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    m_newWardAction = fileMenu->addAction(tr("&New Ward..."), this, &MainWindow::onNewWard);
    m_newWardAction->setShortcut(QKeySequence::New);

    QAction* openWardAction = fileMenu->addAction(tr("&Open Ward..."), this, &MainWindow::onOpenWard);
    openWardAction->setShortcut(QKeySequence::Open);

    fileMenu->addAction(tr("Show in &Folder"), this, &MainWindow::onShowInFolder);

    fileMenu->addSeparator();

    fileMenu->addAction(tr("&Send Update"), this, &MainWindow::onSendUpdate);
    fileMenu->addAction(tr("Open &Update..."), this, &MainWindow::onOpenUpdate);

    fileMenu->addSeparator();

    QMenu* importMenu = fileMenu->addMenu(tr("&Import"));
    m_importPdfAction = importMenu->addAction(tr("&Ward Directory PDF..."), this, &MainWindow::onImportPdf);
    m_importMinisteringPdfAction = importMenu->addAction(tr("&Ministering PDF..."), this, &MainWindow::onImportMinisteringPdf);

    fileMenu->addSeparator();

    m_exitAction = fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close);
    m_exitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    m_undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::onUndo);
    m_undoAction->setShortcut(QKeySequence::Undo);

    m_redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::onRedo);
    m_redoAction->setShortcut(QKeySequence::Redo);

    // Emergency menu
    QMenu* emergencyMenu = menuBar()->addMenu(tr("E&mergency"));

    m_startEmergencyAction = emergencyMenu->addAction(tr("&Start Emergency..."), this, &MainWindow::onStartEmergency);
    m_endEmergencyAction = emergencyMenu->addAction(tr("&End Emergency..."), this, &MainWindow::onEndEmergency);
    m_endEmergencyAction->setEnabled(false);

    emergencyMenu->addSeparator();

    m_generateReportAction = emergencyMenu->addAction(tr("&Generate Report..."), this, &MainWindow::generateEmergencyReport);
    m_generateReportAction->setEnabled(false);

    if (developer)
    {
        QMenu* debuggingMenu = menuBar()->addMenu(tr("&Debugging"));
        debuggingMenu->addAction(tr("&Export Decrypted JSON..."), this, &MainWindow::onExportDecryptedJson);
    }
}

void MainWindow::setupConnections()
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &MainWindow::onDocumentChanged);
    connect(DocumentManager::instance(), &DocumentManager::undoStackChanged,
            this, &MainWindow::updateUndoRedoActions);
    connect(DocumentManager::instance(), &DocumentManager::commandNotApplied,
            this, &MainWindow::onCommandNotApplied);
    connect(DocumentManager::instance(), &DocumentManager::saveFailed,
            this, &MainWindow::onSaveFailed);
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, &MainWindow::onApplicationStateChanged);

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
    connect(m_familyListView, &FamilyListView::visibleFamiliesChanged,
            m_mapWidget, &MapWidget::updateHighlights);

    // Family editing
    connect(m_familyListView, &FamilyListView::editFamilyRequested,
            this, &MainWindow::onEditFamilyRequested);
    connect(m_familyListView, &FamilyListView::deleteFamilyRequested,
            this, &MainWindow::onDeleteFamilyRequested);
    connect(m_editPanel, &FamilyEditPanel::saveRequested,
            this, &MainWindow::onSaveFamily);
    connect(m_editPanel, &FamilyEditPanel::cancelRequested,
            this, &MainWindow::onCancelEdit);
    connect(m_editPanel, &FamilyEditPanel::closeRequested,
            this, &MainWindow::closeEditPanel);

    // Geocoding progress
    connect(DocumentManager::instance(), &DocumentManager::geocodingProgressChanged,
            this, &MainWindow::onGeocodingProgress);
    connect(DocumentManager::instance(), &DocumentManager::geocodingFinished,
            this, &MainWindow::onGeocodingFinished);
}

bool MainWindow::openStartingWard()
{
    if (loadSetting("deviceName").isEmpty())
    {
        bool ok = false;
        QString deviceName = QInputDialog::getText(this, tr("Ward Planning"), tr("Name this device"),
                                                   QLineEdit::Normal, QSysInfo::machineHostName(), &ok)
                                 .trimmed();
        if (!ok || deviceName.isEmpty())
        {
            return false;
        }
        saveSetting("deviceName", deviceName);
    }

    QString lastWardId = loadSetting("lastWardId");
    if (!lastWardId.isEmpty() && openWard(DocumentId::fromString(lastWardId)))
    {
        return true;
    }
    return chooseWard();
}

bool MainWindow::createWard()
{
    bool ok = false;
    QString userName = QInputDialog::getText(this, tr("New Ward"), tr("Your name"), QLineEdit::Normal,
                                             loadSetting("userName"), &ok)
                           .trimmed();
    if (!ok || userName.isEmpty())
    {
        return false;
    }
    saveSetting("userName", userName);
    DocumentManager::instance()->createWard(userName, loadSetting("deviceName"));
    saveSetting("lastWardId", DocumentManager::instance()->documentId().toString());
    return true;
}

bool MainWindow::openWard(const DocumentId& document)
{
    OpenResult result = DocumentManager::instance()->openWard(document);
    if (result.status != OpenStatus::Opened)
    {
        qWarning("A ward did not open: %s", qPrintable(result.problem));
        QMessageBox::warning(this, tr("Ward Not Opened"), OpenWardDialog::openFailureMessage(result.status));
        return false;
    }
    saveSetting("lastWardId", document.toString());
    return true;
}

bool MainWindow::chooseWard()
{
    while (true)
    {
        OpenWardDialog dialog(DocumentManager::instance()->wards(), this);
        switch (dialog.choose())
        {
        case WardListChoice::Open:
            if (dialog.chosenWard() == DocumentManager::instance()->documentId()
                || openWard(dialog.chosenWard()))
            {
                return true;
            }
            break;
        case WardListChoice::NewWard:
            if (createWard())
            {
                return true;
            }
            break;
        case WardListChoice::Close:
            return false;
        }
    }
}

void MainWindow::onNewWard()
{
    if (mayLeaveWard())
    {
        createWard();
    }
}

void MainWindow::onOpenWard()
{
    if (mayLeaveWard())
    {
        chooseWard();
    }
}

void MainWindow::onShowInFolder()
{
    ShowInFolder::reveal(DocumentManager::instance()->filePath());
}

void MainWindow::onSendUpdate()
{
    if (!settleUnsavedEdit())
    {
        return;
    }
    QString wardName = DocumentManager::instance()->document().wardName();
    QString name = UpdateFile::fileName(wardName, QDate::currentDate());
    QByteArray bytes = DocumentManager::instance()->writeUpdate();
    QString problem;
    QString path = UserFolder::standard().write(name, bytes, &problem);
    if (path.isEmpty())
    {
        QMessageBox::warning(this, tr("Send Update"), tr("The update could not be saved: %1.").arg(problem));
        return;
    }
    ShowInFolder::reveal(path);
    statusBar()->showMessage(tr("Send %1 to every other member of %2.").arg(name, wardName));
}

void MainWindow::onOpenUpdate()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Update"), UserFolder::standard().path(),
                                                tr("Ward Planning updates (*.wardupdate)"));
    if (!path.isEmpty())
    {
        openUpdateFile(path);
    }
}

void MainWindow::openUpdateFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("Open Update"),
                             tr("The file %1 could not be read.").arg(QDir::toNativeSeparators(path)));
        return;
    }
    QByteArray bytes = file.readAll();
    UpdateRead update = UpdateFile::read(bytes);
    switch (update.status)
    {
    case UpdateReadStatus::Read:
        break;
    case UpdateReadStatus::Newer:
        QMessageBox::warning(this, tr("Open Update"),
                             tr("This ward was saved by a newer version of Ward Planning. Update to see the latest "
                                "changes and share your work."));
        return;
    case UpdateReadStatus::Unreadable:
        qWarning("An update did not read: %s", qPrintable(update.problem));
        QMessageBox::warning(this, tr("Open Update"), tr("This file is not a Ward Planning update."));
        return;
    }

    DocumentId document = update.header->document();
    if (document != DocumentManager::instance()->documentId())
    {
        if (!DocumentManager::instance()->holdsWard(document))
        {
            QMessageBox::warning(this, tr("Open Update"), tr("This update is for a ward this device does not have."));
            return;
        }
        if (!mayLeaveWard() || !openWard(document))
        {
            return;
        }
    }

    TakeResult result = DocumentManager::instance()->takeUpdate(bytes);
    QString wardName = DocumentManager::instance()->document().wardName();
    switch (result.status)
    {
    case TakeStatus::Taken:
        break;
    case TakeStatus::NotNewer:
        QMessageBox::information(this, tr("Open Update"), tr("You already have a newer update."));
        return;
    case TakeStatus::OtherWard:
        QMessageBox::warning(this, tr("Open Update"), tr("This update is for a ward this device does not have."));
        return;
    case TakeStatus::UnknownWriter:
        QMessageBox::warning(this, tr("Open Update"),
                             tr("This update is from a device this copy of the ward does not list yet. Open the "
                                "update from whoever added it first."));
        return;
    case TakeStatus::NotNamed:
        QMessageBox::warning(this, tr("Open Update"), tr("This ward is no longer shared with this device."));
        return;
    case TakeStatus::Newer:
        QMessageBox::warning(this, tr("Open Update"),
                             tr("This ward was saved by a newer version of Ward Planning. Update to see the latest "
                                "changes and share your work."));
        return;
    case TakeStatus::Unreadable:
        qWarning("An update was not taken: %s", qPrintable(result.problem));
        QMessageBox::warning(this, tr("Open Update"), tr("This update could not be read. Nothing was changed."));
        return;
    }

    switch (result.replay.status)
    {
    case ReplayStatus::FastForwarded:
        QMessageBox::information(this, tr("Open Update"), tr("The update has been added to %1.").arg(wardName));
        break;
    case ReplayStatus::Replayed:
    {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Open Update"),
            tr("The update has been added to %1, with your own changes on top. Send an update so the others get "
               "your changes?")
                .arg(wardName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes)
        {
            onSendUpdate();
        }
        break;
    }
    case ReplayStatus::Abandoned:
        showChangesToRedo(result.replay.toRedo);
        break;
    }
}

void MainWindow::showChangesToRedo(const QString& toRedo)
{
    QString wardName = DocumentManager::instance()->document().wardName();
    QString baseName = tr("To redo - %1 - %2")
                           .arg(wardName, QDateTime::currentDateTime().toString("yyyy-MM-dd HH.mm"));
    QString problem;
    QString path = UserFolder::standard().writeNew(baseName, ".txt", toRedo.toUtf8(), &problem);
    QString stopped = toRedo.section('\n', 0, 0);

    QMessageBox message(QMessageBox::Warning, tr("Changes Not Applied"), QString(), QMessageBox::Ok, this);
    if (path.isEmpty())
    {
        message.setText(tr("%1 The list of what needs doing again could not be saved: %2. It is shown below.")
                            .arg(stopped, problem));
        message.setDetailedText(toRedo);
        message.exec();
        return;
    }
    message.setText(tr("%1 What needs doing again is in %2, in the Ward Planning folder in Documents.")
                        .arg(stopped, QFileInfo(path).fileName()));
    QPushButton* open = message.addButton(tr("Open"), QMessageBox::ActionRole);
    message.exec();
    if (message.clickedButton() == open)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile() && urls.first().toLocalFile().endsWith(".wardupdate"))
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    // The update opens once the drop has finished, so the window it was dragged from is not
    // held while the user answers what opening it asks.
    QString path = event->mimeData()->urls().first().toLocalFile();
    event->acceptProposedAction();
    QMetaObject::invokeMethod(this, "openUpdateFile", Qt::QueuedConnection, Q_ARG(QString, path));
}

void MainWindow::onExportDecryptedJson()
{
    QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Export Decrypted JSON"), tr("This writes the ward's member data unencrypted to a file. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
    {
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Export Decrypted JSON"), QString(),
                                                tr("JSON Files (*.json)"));
    if (path.isEmpty())
    {
        return;
    }

    QByteArray bytes = QJsonDocument(DocumentManager::instance()->payload().toJson()).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit())
    {
        QMessageBox::warning(this, tr("Export Decrypted JSON"),
                             tr("The file %1 could not be written.").arg(QDir::toNativeSeparators(path)));
    }
}

bool MainWindow::mayLeaveWard()
{
    return closeEditPanel() && unsavedChangesMayBeLost();
}

bool MainWindow::unsavedChangesMayBeLost()
{
    if (DocumentManager::instance()->retryPendingSave())
    {
        return true;
    }
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Changes Not Saved"), tr("Your latest changes could not be saved and will be lost. Continue anyway?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!mayLeaveWard())
    {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::onSaveFailed(const QString& problem)
{
    QMessageBox::warning(this, tr("Change Not Saved"),
                         tr("Your change could not be saved: %1. It is kept while the app stays open and saved as "
                            "soon as saving works again; closing asks first.")
                             .arg(problem));
}

void MainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive)
    {
        DocumentManager::instance()->retryPendingSave();
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
    WardDirectoryImportResult result = WardDirectoryImportService::importFromPdf(
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

    statusBar()->clearMessage();

    CommitResult commit = DocumentManager::instance()->executeCommand(ImportWardDirectoryCommand::fromImport(
        DocumentManager::instance()->document(), result, QFileInfo(filePath).fileName()));

    switch (commit.status)
    {
    case CommitStatus::Applied:
        statusBar()->showMessage(tr("Imported the ward directory"), 5000);
        m_mapWidget->fitAllFamilies();
        break;
    case CommitStatus::NothingToDo:
        statusBar()->showMessage(tr("Nothing changed"), 5000);
        break;
    case CommitStatus::Conflicted:
    case CommitStatus::Refused:
        break;
    }
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

    MinisteringImportResult result = MinisteringImportService::importFromPdf(
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

    CommitResult commit = DocumentManager::instance()->executeCommand(ImportMinisteringCommand::fromImport(
        DocumentManager::instance()->document(), result, QFileInfo(filePath).fileName()));

    QString orgType = result.isRSFormat ? tr("RS") : tr("EQ");
    switch (commit.status)
    {
    case CommitStatus::Applied:
        statusBar()->showMessage(tr("Imported the %1 ministering assignments").arg(orgType), 5000);
        break;
    case CommitStatus::NothingToDo:
        statusBar()->showMessage(tr("Nothing changed"), 5000);
        break;
    case CommitStatus::Conflicted:
    case CommitStatus::Refused:
        break;
    }
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
    updateWindowTitle();
    if (m_editPanel->isVisible())
    {
        m_editPanel->refreshCoordinates(DocumentManager::instance()->document());
    }

    // A full change can put a different emergency in place of the one in progress
    if (change.action == ChangeAction::Full)
    {
        updateEmergencyBanner();
    }
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(tr("%1 - Ward Planning").arg(DocumentManager::instance()->document().wardName()));
}

void MainWindow::updateUndoRedoActions()
{
    m_undoAction->setEnabled(DocumentManager::instance()->canUndo());
    m_redoAction->setEnabled(DocumentManager::instance()->canRedo());

    QString undoText = tr("&Undo");
    if (DocumentManager::instance()->canUndo())
    {
        undoText += " " + DocumentManager::instance()->undoHeading();
    }
    m_undoAction->setText(undoText);

    QString redoText = tr("&Redo");
    if (DocumentManager::instance()->canRedo())
    {
        redoText += " " + DocumentManager::instance()->redoHeading();
    }
    m_redoAction->setText(redoText);
}

void MainWindow::onCommandNotApplied(const CommitResult& result)
{
    if (result.status == CommitStatus::Refused)
    {
        QMessageBox::warning(this, tr("Change Not Applied"), result.refusal);
        return;
    }

    QStringList text;
    text.append(tr("Your change could not be applied, because the ward changed first. This needs doing again:"));
    text.append(QString());
    text.append(result.description.heading());
    for (const QString& line : result.description.lines())
    {
        text.append(QString("- %1").arg(line));
    }
    QMessageBox::warning(this, tr("Change Not Applied"), text.join("\n"));
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

QJsonObject MainWindow::loadSettings() const
{
    QFile file(m_settingsFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QJsonObject();
    }
    QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

void MainWindow::saveSettings(const QJsonObject& json) const
{
    QSaveFile file(m_settingsFilePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(json).toJson());
        file.commit();
    }
}

QString MainWindow::loadSetting(const QString& key) const
{
    return loadSettings()[key].toString();
}

void MainWindow::saveSetting(const QString& key, const QString& value) const
{
    QJsonObject json = loadSettings();
    json[key] = value;
    saveSettings(json);
}

bool MainWindow::loadDefaultLocation(double& lat, double& lng) const
{
    QJsonObject json = loadSettings();
    if (!json.contains("defaultLat") || !json.contains("defaultLng"))
    {
        return false;
    }

    lat = json["defaultLat"].toDouble();
    lng = json["defaultLng"].toDouble();
    return true;
}

void MainWindow::saveDefaultLocation(double lat, double lng) const
{
    QJsonObject json = loadSettings();
    json["defaultLat"] = lat;
    json["defaultLng"] = lng;
    saveSettings(json);
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

    if (FamilyMarkerProvider* provider = dynamic_cast<FamilyMarkerProvider*>(currentTab))
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
    if (m_editPanel->isVisible()
        && m_editPanel->familyId() != familyId
        && !settleUnsavedEdit())
    {
        return;
    }
    openEditPanel(familyId);
}

void MainWindow::onDeleteFamilyRequested(const FamilyId& familyId)
{
    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    // The family as the user sees it when asked, which is what the deletion removes.
    Family family = it.value();
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

        DocumentManager::instance()->executeCommand(
            DeleteFamilyCommand::fromSeen(family, DocumentManager::instance()->document()));
    }
}

void MainWindow::onSaveFamily()
{
    if (!m_editPanel->isVisible())
    {
        return;
    }
    saveEdit();
    closeEditPanelInternal();
}

void MainWindow::saveEdit()
{
    // The position belongs to the address, so it is committed whatever happens to the
    // family edit.
    Family saved = m_editPanel->family();
    DocumentManager::instance()->executeCommand(
        UpdateFamilyCommand::fromEdit(m_editPanel->openedFamily(), saved, DocumentManager::instance()->document()));
    std::optional<Coordinates> typed = m_editPanel->typedCoordinates();
    if (typed.has_value())
    {
        DocumentManager::instance()->executeCommand(SetAddressCoordinatesCommand::fromTyped(
            DocumentManager::instance()->document(), saved.address().full(), *typed));
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

bool MainWindow::closeEditPanel()
{
    if (!m_editPanel->isVisible())
    {
        return true;
    }
    if (!settleUnsavedEdit())
    {
        return false;
    }
    closeEditPanelInternal();
    return true;
}

bool MainWindow::settleUnsavedEdit()
{
    if (!m_editPanel->isDirty())
    {
        return true;
    }
    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("Save changes to %1?").arg(m_editPanel->family().displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (result == QMessageBox::Cancel)
    {
        return false;
    }
    if (result == QMessageBox::Save)
    {
        saveEdit();
    }
    return true;
}

void MainWindow::openEditPanel(const FamilyId& familyId)
{
    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    auto it = families.find(familyId);
    if (it == families.end())
    {
        return;
    }

    m_editPanel->setFamily(it.value(), DocumentManager::instance()->document());

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

    DocumentManager::instance()->executeCommand(StartEmergencyCommand::fromNew(EmergencyResponse::create(name.trimmed())));
    statusBar()->showMessage(tr("Emergency \"%1\" started").arg(name.trimmed()), 5000);
}

void MainWindow::onEndEmergency()
{
    if (!EmergencyManager::instance()->isActive())
    {
        return;
    }
    const EmergencyResponse& inProgress = EmergencyManager::instance()->response();
    QString emergencyName = inProgress.name();
    QString defaultPath = defaultReportPath(inProgress);

    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("End Emergency"));
    dialog.setText(tr("End \"%1\"?").arg(emergencyName));
    dialog.setInformativeText(tr("Ending saves the summary report, which is the record of the emergency, and then removes the emergency."));
    dialog.setIcon(QMessageBox::Question);

    QPushButton* endButton = dialog.addButton(tr("End"), QMessageBox::DestructiveRole);
    dialog.addButton(QMessageBox::Cancel);

    dialog.exec();

    if (dialog.clickedButton() != endButton)
    {
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Emergency Report"), defaultPath,
                                                    tr("PDF Files (*.pdf)"));
    if (filePath.isEmpty())
    {
        return;
    }

    switch (DocumentManager::instance()->endEmergency(filePath))
    {
    case EndResult::Ended:
        statusBar()->showMessage(tr("Emergency \"%1\" ended; report saved to %2").arg(emergencyName, filePath), 5000);
        break;
    case EndResult::AlreadyEnded:
        QMessageBox::information(this, tr("End Emergency"),
                                 tr("\"%1\" has already been ended by someone else.").arg(emergencyName));
        break;
    case EndResult::ReportFailed:
        QMessageBox::warning(this, tr("End Emergency"),
                             tr("The report could not be written to %1, so \"%2\" was not ended.").arg(filePath, emergencyName));
        break;
    }
}

void MainWindow::generateEmergencyReport()
{
    if (!EmergencyManager::instance()->isActive())
    {
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Emergency Report"), defaultReportPath(EmergencyManager::instance()->response()),
        tr("PDF Files (*.pdf)"));
    if (filePath.isEmpty())
    {
        return;
    }

    // The emergency as it stands after the dialog, which may have changed or ended meanwhile
    const Document& document = DocumentManager::instance()->document();
    const std::optional<EmergencyResponse>& response = document.emergencyResponse();
    if (!response)
    {
        QMessageBox::information(this, tr("Report"), tr("The emergency has been ended by someone else."));
        return;
    }

    if (ReportGenerator::generateReport(*response, document, QDateTime::currentDateTimeUtc(), filePath))
    {
        statusBar()->showMessage(tr("Report saved to %1").arg(filePath), 5000);
        return;
    }

    QMessageBox::warning(this, tr("Report Error"),
                         tr("Failed to generate the report."));
}

QString MainWindow::defaultReportPath(const EmergencyResponse& response)
{
    QString name = response.name().simplified().replace(' ', '_')
        + "_" + response.startedAt().toLocalTime().toString("yyyy-MM-dd")
        + ".pdf";
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + name;
}

void MainWindow::onEmergencyStarted()
{
    updateEmergencyBanner();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
    m_splitter->setSizes({600, 0, 1100});
    updateEmergencyActions();
}

void MainWindow::onEmergencyEnded()
{
    updateEmergencyBanner();
    m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);
    m_splitter->setSizes({300, 0, 1100});
    updateEmergencyActions();
}

void MainWindow::updateEmergencyBanner()
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        m_emergencyBanner->clearBanner();
        return;
    }
    m_emergencyBanner->showEmergency(manager->response().name());
}

void MainWindow::updateEmergencyActions()
{
    bool active = EmergencyManager::instance()->isActive();
    m_startEmergencyAction->setEnabled(!active);
    m_endEmergencyAction->setEnabled(active);
    m_generateReportAction->setEnabled(active);
}
