#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileOpenEvent>
#include <QIcon>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include "AppSettings.h"
#include "DeviceLinks.h"
#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "FileOpenRouter.h"
#include "FirstRunDialog.h"
#include "HttpServerApi.h"
#include "IdentityService.h"
#include "MainWindow.h"
#include "IncomingWards.h"
#include "ServerConnection.h"
#include "SingleInstance.h"
#include "TileService.h"
#include "UpdateChecker.h"

#include <QPushButton>

#include <functional>

namespace
{

/// Replaces this device's identity, and drops what the old one kept in settings: the
/// devices it linked, which it seals into its wards, and the wards it asked for.
std::unique_ptr<IdentityService> replaceIdentity(const QString& path, AppSettings& settings, QString* error)
{
    std::unique_ptr<IdentityService> identity = IdentityService::startOver(path, error);
    if (identity)
    {
        settings.setLinkedDevices({});
        settings.setRequestedWards({});
    }
    return identity;
}

/// On macOS a file or a wardplanning: link opened from elsewhere arrives as an
/// event, not as arguments; it goes where the arguments would.
class FileOpenForwarder : public QObject
{
public:
    explicit FileOpenForwarder(MainWindow& window)
        : m_window(window)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::FileOpen)
        {
            return QObject::eventFilter(watched, event);
        }

        const QFileOpenEvent* open = static_cast<QFileOpenEvent*>(event);
        m_window.handleArguments({QString(), open->file().isEmpty() ? open->url().toString() : open->file()});
        return true;
    }

private:
    MainWindow& m_window;
};

/// This device's identity, or nullptr once the person has been told why there is
/// none and chosen to quit. A damaged file is offered a fresh start; one a newer
/// version wrote is left alone, since starting afresh would orphan it for nothing.
std::unique_ptr<IdentityService> resolveIdentity(const QString& path, AppSettings& settings)
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

    identity = replaceIdentity(path, settings, &error);
    if (!identity)
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), error);
    }
    return identity;
}

/// A fresh identity in place of this device's, then the app closes so the next
/// launch asks the first question afresh.
void startOver(const QString& identityPath, AppSettings& settings)
{
    QString error;
    if (!replaceIdentity(identityPath, settings, &error))
    {
        QMessageBox::critical(nullptr, QObject::tr("Ward Planning"), error);
    }
    QApplication::quit();
}

void onReleaseReply(QNetworkReply* reply, const UpdateChecker::Fetched& done)
{
    done(reply->error() == QNetworkReply::NoError, reply->readAll());
    reply->deleteLater();
}

/// One GET for the release check. GitHub answers only a request that names its agent.
void fetchRelease(QNetworkAccessManager& network, const QUrl& url, UpdateChecker::Fetched done)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WardPlanning/") + WARD_PLANNING_VERSION);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, std::bind(&onReleaseReply, reply, done));
}

/// Whether the arguments after the program's own name a Ward Planning file.
bool namesADocument(const QStringList& arguments)
{
    for (qsizetype i = 1; i < arguments.size(); ++i)
    {
        if (arguments.at(i).endsWith(DocumentConstants::FileExtension, Qt::CaseInsensitive)
            || !FileOpenRouter::invitedWard(arguments.at(i)).isEmpty())
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
    std::unique_ptr<IdentityService> identityService = resolveIdentity(identityPath, *settings);
    if (!identityService)
    {
        return 1;
    }

    // The first screen, while no person exists on this device. Linking reaches the
    // server before the rest of the app exists.
    HttpServerApi serverApi(*settings, nullptr);
    if (!identityService->identity().has_value())
    {
        FirstRunDialog firstRun(*identityService, serverApi, nullptr);
        if (firstRun.exec() != QDialog::Accepted)
        {
            return 0;
        }
    }
    const DeviceIdentity identity = identityService->identity().value();

    // Every service that takes the identity is built after it exists, and takes it
    // by value.
    ServerConnection server(serverApi, identity);
    DocumentLibrary library(*settings, deviceData.filePath("wards"), identity);

    // Stack locals, so the window goes first and the manager outlives it.
    DocumentManager documentManager(identity, *settings, server, library, nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    IncomingWards incoming(*settings, library, server, nullptr);
    QObject::connect(&incoming, &IncomingWards::lostAccess, &documentManager, &DocumentManager::onWardLostElsewhere);
    DeviceLinks links(identity,
                      *settings,
                      server,
                      std::bind(&DocumentLibrary::pathsFor, &library, std::placeholders::_1),
                      nullptr);
    QNetworkAccessManager releaseNetwork;
    UpdateChecker updates(*settings,
                          WARD_PLANNING_VERSION,
                          std::bind(&fetchRelease, std::ref(releaseNetwork), std::placeholders::_1, std::placeholders::_2),
                          nullptr);
    QObject::connect(&documentManager, &DocumentManager::requiresUpdateChanged, &updates, std::bind(&UpdateChecker::check, &updates, true));
    MainWindow window(*settings, library, incoming, links, updates, nullptr);
    QObject::connect(&window,
                     &MainWindow::startOverRequested,
                     &app,
                     std::bind(&startOver, identityPath, std::ref(*settings)));

    // Told outside the call that found out, which is still delivering its replies.
    server.setUnlinkedHandler([&window](const ServerContract::LostAccess& lost) {
        QMetaObject::invokeMethod(&window, std::bind(&MainWindow::unlinkedHere, &window, lost), Qt::QueuedConnection);
    });

    FileOpenForwarder fileOpen(window);
    app.installEventFilter(&fileOpen);
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

    QObject::connect(&instance, &SingleInstance::argumentsReceived, &window, &MainWindow::handleArguments);

    // Every ward the server lists for this person and the library lacks: after a
    // reinstall, cleared app data, a grant, or a link. And whether a newer Ward
    // Planning is out.
    incoming.checkNow();
    updates.check(false);

    return app.exec();
}
