#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
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
#include "NetworkTransport.h"
#include "InstanceLock.h"
#include "KeyFile.h"
#include "MainWindow.h"
#include "TileService.h"

/// The invitation code waiting in the file at path; empty when there is none.
static QString waitingInvitation(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }
    return QString::fromLatin1(file.readAll()).trimmed();
}

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

    // App data is named after the application, so it is found after the name is set. This
    // launch opens the file it was started with, or else the invitation waiting in app data,
    // whose code travels where a file's path does, marked as a code. The waiting invitation
    // stays until a window has it and the user's details, so a start that ends before then
    // leaves it for the next.
    AppData appData = AppData::standard();
    QString file;
    bool invitationWaited = false;
    if (!parser.positionalArguments().isEmpty())
    {
        file = QFileInfo(parser.positionalArguments().first()).absoluteFilePath();
    }
    else
    {
        QString code = waitingInvitation(appData.invitationFilePath());
        if (!code.isEmpty())
        {
            file = MainWindow::JoinPrefix + code;
            invitationWaited = true;
        }
    }

    // A launch that cannot take the instance lock hands what it would open, or nothing, to the
    // Ward Planning holding it and exits; the lock is taken before the key file is read, so two
    // first starts never both make keys.
    InstanceLock lock(appData.lockFilePath(), nullptr);
    if (!lock.take())
    {
        if (lock.handOver(file))
        {
            if (invitationWaited)
            {
                QFile::remove(appData.invitationFilePath());
            }
        }
        else if (invitationWaited)
        {
            QMessageBox::warning(nullptr, QObject::tr("Ward Planning"),
                                 QObject::tr("Ward Planning is already open but did not answer, so your invitation "
                                             "was not opened. Close Ward Planning, then open it again."));
        }
        else if (!file.isEmpty())
        {
            QMessageBox::warning(nullptr, QObject::tr("Ward Planning"),
                                 QObject::tr("Ward Planning is already running but did not answer, so the file was "
                                             "not opened. Open it there with File > Open Update or File > Open "
                                             "Invitation."));
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
    NetworkTransport transport(NetworkTransport::BuiltInAddress, nullptr);
    DocumentManager documentManager(&*keys.identity, &store, &transport, nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    AppSettings settings(appData.settingsFilePath());
    MainWindow window(&settings, parser.isSet(developerOption), nullptr);
    if (!window.askYourDetails())
    {
        return 0;
    }
    if (invitationWaited)
    {
        QFile::remove(appData.invitationFilePath());
    }
    if (!window.openStartingWard(file))
    {
        return 0;
    }
    documentManager.setUseServer(settings.useServer());
    window.show();
    QObject::connect(&lock, &InstanceLock::handedOver, &window, &MainWindow::onHandedOver);
    lock.listen();
    window.checkForUpdate(WARD_PLANNING_RELEASE_NUMBER);

    return app.exec();
}
