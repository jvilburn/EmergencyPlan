#include "AppData.h"

#include <QStandardPaths>

AppData::AppData(const QString& root)
    : m_root(root)
{
}

AppData AppData::standard()
{
    return AppData(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}
