#include <QApplication>
#include <QFile>
#include <QIcon>
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "MainWindow.h"
#include "TileService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Emergency Plan");
    app.setOrganizationName("");
    app.setWindowIcon(QIcon(":/markers/marker_chapel.svg"));

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
    if (args.size() > 1 && args[1].endsWith(".emergencyplan", Qt::CaseInsensitive))
    {
        window.openFile(args[1]);
    }

    return app.exec();
}
