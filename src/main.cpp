#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include "AppData.h"
#include "Crypto.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FileWardStore.h"
#include "InstanceLock.h"
#include "KeyFile.h"
#include "MainWindow.h"
#include "TileService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Ward Planning");
    app.setApplicationVersion(WARD_PLANNING_VERSION);
    app.setWindowIcon(QIcon(":/markers/marker_chapel.svg"));

    QCommandLineParser parser;
    QCommandLineOption developerOption("developer", QObject::tr("Adds the Debugging menu."));
    parser.addOption(developerOption);
    parser.addPositionalArgument("update", QObject::tr("An update file to open."));
    parser.process(app);

    // App data is named after the application, so it is found after the name is set. A launch
    // that cannot take the instance lock exits, and the lock is taken before the key file is
    // read, so two first starts never both make keys.
    AppData appData = AppData::standard();
    InstanceLock lock(appData.lockFilePath());
    if (!lock.take())
    {
        return 0;
    }

    // Load Windows 7 Aero stylesheet
    QFile styleFile(":/styles/windows7.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text))
    {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    if (!Crypto::initialize())
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"),
                              QObject::tr("Ward Planning could not start its encryption library."));
        return 1;
    }

    KeyFileResult keys = KeyFile::loadOrCreate(appData.keyFilePath());
    switch (keys.status)
    {
    case KeyFileStatus::Loaded:
    case KeyFileStatus::Created:
        break;
    case KeyFileStatus::Unreadable:
    case KeyFileStatus::Fault:
        qWarning("This device's key file could not be read: %s", qPrintable(keys.problem));
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"),
                              QObject::tr("This device's keys in %1 could not be read. Nothing has been changed.")
                                  .arg(QDir::toNativeSeparators(appData.keyFilePath())));
        return 1;
    }

    // Initialize core services (singletons)
    FileWardStore store(appData.wardsDirectory());
    DocumentManager documentManager(&*keys.identity, &store, nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window(appData.settingsFilePath(), parser.isSet(developerOption), nullptr);
    if (!window.openStartingWard())
    {
        return 0;
    }
    window.show();
    if (!parser.positionalArguments().isEmpty())
    {
        window.openUpdateFile(parser.positionalArguments().first());
    }

    return app.exec();
}
