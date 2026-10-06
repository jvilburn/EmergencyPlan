#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "IdentityService.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "TileService.h"

#include <QPushButton>

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
    std::unique_ptr<IdentityService> identity = resolveIdentity(deviceData.filePath("identity.json"));
    if (!identity)
    {
        return 1;
    }

    // Initialize core services (singletons)
    DocumentManager documentManager(nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window(*settings, nullptr);
    window.show();

    // A file passed on the command line (a file association), and those a later
    // launch hands over.
    window.handleArguments(app.arguments());
    QObject::connect(&instance, &SingleInstance::argumentsReceived, &window, &MainWindow::handleArguments);

    return app.exec();
}
