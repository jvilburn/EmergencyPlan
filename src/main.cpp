#include <QApplication>
#include <QFile>
#include "MainWindow.h"
#include "TileService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Emergency Plan");
    app.setOrganizationName("Church");

    // Load Windows 7 Aero stylesheet
    QFile styleFile(":/styles/windows7.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text))
    {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // Initialize tile service for map rendering
    TileService tileService;
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window;
    window.show();

    return app.exec();
}
