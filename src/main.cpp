#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "MainWindow.h"
#include "TileService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Ward Planning");
    app.setOrganizationName("Church");
    app.setApplicationVersion(WARD_PLANNING_VERSION);
    app.setWindowIcon(QIcon(":/markers/marker_chapel.svg"));

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

    // Initialize core services (singletons)
    DocumentManager documentManager(nullptr);
    EmergencyManager emergencyManager(nullptr);

    // Initialize tile service for map rendering
    TileService tileService(nullptr);
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window(nullptr);
    window.show();

    // Open file passed as command-line argument (e.g. from file association)
    const QStringList args = app.arguments();
    if (args.size() > 1
        && args[1].endsWith(DocumentConstants::FileExtension, Qt::CaseInsensitive))
    {
        window.openFile(args[1]);
    }

    return app.exec();
}
