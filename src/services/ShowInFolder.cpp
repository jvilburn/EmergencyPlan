#include "ShowInFolder.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QUrl>

void ShowInFolder::reveal(const QString& path)
{
#if defined(Q_OS_WIN)
    // Explorer takes /select and the quoted path as one argument, and the path can hold
    // spaces, so the command line is given as it is rather than quoted by QProcess.
    QProcess explorer;
    explorer.setProgram("explorer.exe");
    explorer.setNativeArguments("/select,\"" + QDir::toNativeSeparators(path) + "\"");
    explorer.startDetached();
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("open", QStringList({"-R", path}));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}
