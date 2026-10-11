#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include "AppSettings.h"
#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FirstRunDialog.h"
#include "HttpServerApi.h"
#include "IdentityService.h"
#include "MainWindow.h"
#include "RosterChangeQueue.h"
#include "ServerConnection.h"
#include "SessionTimers.h"
#include "SingleInstance.h"
#include "TileService.h"

#include <QPushButton>

#include <functional>

namespace
{

/// This device's identity, or nullptr once the person has been told why there is
/// none and chosen to quit. A damaged file is offered a fresh start; one a newer
/// version wrote is left alone, since starting afresh would orphan it for nothing.
std::unique_ptr<IdentityService> resolveIdentity(const QString& path)
{
    IdentityFile::Load outcome = IdentityFile::Load::NotFound;
    QString error;
    std::unique_ptr<IdentityService> identity = IdentityService::open(path, outcome, &error);
    if (identity)
    {
        return identity;
    }

    if (outcome != IdentityFile::Load::Failed)
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), error);
        return nullptr;
    }

    QMessageBox damaged(QMessageBox::Critical,
                        QObject::tr("Ward Planning"),
                        error,
                        QMessageBox::NoButton,
                        nullptr);
    damaged.setInformativeText(
        QObject::tr("You can start this device afresh. The damaged file is kept beside it, so it can "
                    "still be restored."));
    QPushButton* quit = damaged.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
    QPushButton* afresh = damaged.addButton(QObject::tr("Start This Device Afresh..."), QMessageBox::AcceptRole);
    damaged.setDefaultButton(quit);
    damaged.exec();
    if (damaged.clickedButton() != afresh)
    {
        return nullptr;
    }

    QMessageBox confirm(QMessageBox::Warning,
                        QObject::tr("Start This Device Afresh"),
                        QObject::tr("This device will get a new identity."),
                        QMessageBox::NoButton,
                        nullptr);
    confirm.setInformativeText(
        QObject::tr("Wards shared with others come back once you link this device to one of your other "
                    "devices, or someone grants it access again.\n\n"
                    "Wards that were never shared from this device will not open, unless the damaged "
                    "file is restored."));
    QPushButton* cancel = confirm.addButton(QMessageBox::Cancel);
    QPushButton* start = confirm.addButton(QObject::tr("Start Afresh"), QMessageBox::DestructiveRole);
    confirm.setDefaultButton(cancel);
    confirm.exec();
    if (confirm.clickedButton() != start)
    {
        return nullptr;
    }

    identity = IdentityService::startOver(path, &error);
    if (!identity)
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), error);
    }
    return identity;
}

/// A fresh identity in place of this device's, then the app closes so the next
/// launch asks the first question afresh.
void startOver(const QString& identityPath)
{
    QString error;
    if (!IdentityService::startOver(identityPath, &error))
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), error);
    }
    QApplication::quit();
}

/// Whether the arguments after the program's own name a Ward Planning file.
bool namesADocument(const QStringList& arguments)
{
    for (qsizetype i = 1; i < arguments.size(); ++i)
    {
        if (arguments.at(i).endsWith(DocumentConstants::FileExtension, Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Ward Planning");
    app.setOrganizationName("Church");
    app.setApplicationVersion(WARD_PLANNING_VERSION);
    app.setWindowIcon(QIcon(":/markers/marker_chapel.svg"));

    // One instance per user, settled before settings or identity load. A later
    // launch hands its arguments to the running one and exits.
    SingleInstance instance(nullptr);
    QString instanceError;
    switch (instance.claim(app.arguments(), &instanceError))
    {
    case SingleInstance::Claim::Claimed:
        break;
    case SingleInstance::Claim::HandedOver:
        return 0;
    case SingleInstance::Claim::Failed:
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), instanceError);
        return 1;
    }

    // Before anything that might touch a document: libsodium must be initialised
    // before its first use, and without it no shared document can be opened.
    if (!DocumentCrypto::initialise())
    {
        QMessageBox::critical(nullptr,
                              QObject::tr("Ward Planning"),
                              QObject::tr("Ward Planning could not start its encryption library."));
        return 1;
    }

    // Load Windows 7 Aero stylesheet
    QFile styleFile(":/styles/windows7.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text))
    {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // This device's state, in local app data, which does not roam.
    const QDir deviceData(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

    QString settingsError;
    std::unique_ptr<AppSettings> settings = AppSettings::open(deviceData.filePath("settings.json"), &settingsError);
    if (!settings)
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), settingsError);
        return 1;
    }

    // Resolved before any document exists: there is no document without an identity.
    const QString identityPath = deviceData.filePath("identity.json");
    std::unique_ptr<IdentityService> identityService = resolveIdentity(identityPath);
    if (!identityService)
    {
        return 1;
    }

    // The first screen, while no person exists on this device.
    if (!identityService->identity().has_value())
    {
        FirstRunDialog firstRun(*identityService, nullptr);
        if (firstRun.exec() != QDialog::Accepted)
        {
            return 0;
        }
    }
    const DeviceIdentity identity = identityService->identity().value();

    // Every service that takes the identity is built after it exists, and takes it
    // by value.
    HttpServerApi serverApi(*settings, nullptr);
    ServerConnection server(serverApi, identity);
    DocumentLibrary library(*settings, deviceData.filePath("wards"), identity);
    RosterChangeQueue queue(identity,
                            *settings,
                            server,
                            std::make_unique<QtSessionTimers>(),
                            [&library](const QString& documentId) { return library.pathsFor(documentId); },
                            nullptr);

    // Stack locals, so the window goes first and the manager outlives it.
    DocumentManager documentManager(identity, *settings, server, library, queue, nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window(*settings, library, nullptr);
    QObject::connect(&window, &MainWindow::startOverRequested, &app, std::bind(&startOver, identityPath));
    window.show();

    // A file passed on the command line (a file association) opens in place of the
    // last ward; so do those a later launch hands over.
    if (namesADocument(app.arguments()))
    {
        window.handleArguments(app.arguments());
    }
    else if (!library.lastOpenedId().isEmpty())
    {
        window.openWard(library.lastOpenedId());
    }

    // Every ward the server lists for this device and the library lacks: after a
    // reinstall, cleared app data, or a grant.
    library.restore(server,
                    &window,
                    std::bind(&MainWindow::onWardRestored, &window, std::placeholders::_1, std::placeholders::_2));
    QObject::connect(&instance, &SingleInstance::argumentsReceived, &window, &MainWindow::handleArguments);

    return app.exec();
}
