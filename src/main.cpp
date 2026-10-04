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
#include "MainWindow.h"
#include "SingleInstance.h"
#include "TileService.h"

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

    QString settingsError;
    const QString settingsPath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("settings.json");
    std::unique_ptr<AppSettings> settings = AppSettings::open(settingsPath, &settingsError);
    if (!settings)
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), settingsError);
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
