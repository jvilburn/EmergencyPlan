#include <QApplication>
#include "MainWindow.h"
#include "TileService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Emergency Plan");
    app.setOrganizationName("Church");

    // Initialize tile service for map rendering
    TileService tileService;
    TileService::setInstance(&tileService);
    tileService.initialize();

    MainWindow window;
    window.show();

    return app.exec();
}
