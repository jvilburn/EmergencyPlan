#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include "AppData.h"
#include "AppSettings.h"
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
    parser.addPositionalArgument("file", QObject::tr("An update or invitation file to open."));
    parser.process(app);

    // App data is named after the application, so it is found after the name is set. A launch
    // that cannot take the instance lock hands its update or invitation file, or nothing, to
    // the Ward Planning holding it and exits; the lock is taken before the key file is read, so two
    // first starts never both make keys.
    AppData appData = AppData::standard();
    InstanceLock lock(appData.lockFilePath(), nullptr);
    QString file;
    if (!parser.positionalArguments().isEmpty())
    {
        file = QFileInfo(parser.positionalArguments().first()).absoluteFilePath();
    }
    if (!lock.take())
    {
        if (!lock.handOver(file) && !file.isEmpty())
        {
            QMessageBox::warning(nullptr, QObject::tr("Ward Planning"),
                                 QObject::tr("Ward Planning is already running but did not answer, so the file was "
                                             "not opened. Open it there with File > Open Update or File > Join a "
                                             "Ward."));
        }
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

    AppSettings settings(appData.settingsFilePath());
    MainWindow window(&settings, parser.isSet(developerOption), nullptr);
    if (!window.openStartingWard())
    {
        return 0;
    }
    window.show();
    QObject::connect(&lock, &InstanceLock::handedOver, &window, &MainWindow::onHandedOver);
    lock.listen();
    if (!file.isEmpty())
    {
        window.openFile(file);
    }

    return app.exec();
}
