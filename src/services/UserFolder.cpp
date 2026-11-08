#include "UserFolder.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>

UserFolder::UserFolder(const QString& root)
    : m_root(root)
{
}

UserFolder UserFolder::standard()
{
    return UserFolder(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Ward Planning");
}

QString UserFolder::write(const QString& name, const QByteArray& bytes, QString* problem) const
{
    if (!QDir().mkpath(m_root))
    {
        *problem = QObject::tr("the folder %1 could not be made").arg(QDir::toNativeSeparators(m_root));
        return QString();
    }
    QString path = m_root + "/" + name;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit())
    {
        *problem = file.errorString();
        return QString();
    }
    return path;
}

QString UserFolder::writeNew(const QString& baseName, const QString& extension, const QByteArray& bytes,
                             QString* problem) const
{
    QString name = baseName + extension;
    for (int copy = 2; QFileInfo::exists(m_root + "/" + name); ++copy)
    {
        name = QString("%1 (%2)%3").arg(baseName).arg(copy).arg(extension);
    }
    return write(name, bytes, problem);
}
