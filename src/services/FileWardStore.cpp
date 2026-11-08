#include "FileWardStore.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace
{

/// The system's error text as a clause, with no final period.
QString asClause(const QString& error)
{
    QString clause = error.trimmed();
    if (clause.endsWith('.'))
    {
        clause.chop(1);
    }
    return clause;
}

}  // namespace

FileWardStore::FileWardStore(const QString& directory)
    : m_directory(directory)
{
}

bool FileWardStore::write(const DocumentId& document, const QByteArray& bytes, QString* problem)
{
    if (!QDir().mkpath(m_directory))
    {
        *problem = tr("the folder %1 could not be made").arg(QDir::toNativeSeparators(m_directory));
        return false;
    }
    // Binary, with no newline translation: the payload is authenticated against these bytes.
    QSaveFile file(pathOf(document));
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit())
    {
        *problem = asClause(file.errorString());
        return false;
    }
    return true;
}

StoreRead FileWardStore::read(const DocumentId& document) const
{
    QFile file(pathOf(document));
    if (!file.exists())
    {
        return StoreRead{StoreReadStatus::Missing, QByteArray(), QString()};
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        return StoreRead{StoreReadStatus::Fault, QByteArray(), asClause(file.errorString())};
    }
    QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return StoreRead{StoreReadStatus::Fault, QByteArray(), asClause(file.errorString())};
    }
    return StoreRead{StoreReadStatus::Read, bytes, QString()};
}

QString FileWardStore::pathOf(const DocumentId& document) const
{
    return m_directory + "/" + document.toString() + ".wardplanning";
}
