#include "DocumentLibrary.h"

#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "ServerVersion.h"

#include <QDir>
#include <QFile>
#include <QObject>

#include <algorithm>

namespace
{

constexpr int DocumentIdBytes = DocumentHeader::DocumentIdLength / 2;

bool sortsBefore(const DocumentLibrary::Entry& left, const DocumentLibrary::Entry& right)
{
    const int byName = QString::compare(left.wardName, right.wardName, Qt::CaseInsensitive);
    if (byName != 0)
    {
        return byName < 0;
    }
    return left.documentId < right.documentId;
}

/// Whether the file at path is a copy written from this ward.
bool isCopyOf(const QString& path, const QString& documentId)
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(path);
    DocumentFile::Sealed sealed;
    return bytes.has_value()
        && DocumentFile::decode(bytes.value(), sealed) == DocumentFile::Read::Ok
        && sealed.header.originDocumentId == documentId;
}

} // namespace

DocumentLibrary::DocumentLibrary(AppSettings& settings, const QString& directory, const DeviceIdentity& identity)
    : m_settings(settings)
    , m_directory(directory)
    , m_identity(identity)
{
}

QList<DocumentLibrary::Entry> DocumentLibrary::entries() const
{
    QList<Entry> entries;
    for (const QString& documentId : m_settings.wardIds())
    {
        const AppSettings::WardEntry ward = entryOf(documentId);
        entries.append(Entry{documentId,
                             ward.wardName,
                             ward.lastOpened,
                             m_settings.record(documentId).baseVersion.has_value()});
    }
    std::sort(entries.begin(), entries.end(), sortsBefore);
    return entries;
}

DocumentPaths DocumentLibrary::pathsFor(const QString& documentId) const
{
    if (!m_settings.ward(documentId).has_value())
    {
        return DocumentPaths();
    }

    const QDir directory(m_directory);
    return DocumentPaths{directory.filePath(documentId + ".wardplanning"), directory.filePath(documentId + ".base")};
}

QString DocumentLibrary::newDocumentId()
{
    return QString::fromLatin1(DocumentCrypto::randomBytes(DocumentIdBytes).toHex());
}

void DocumentLibrary::add(const QString& documentId)
{
    QDir().mkpath(m_directory);
    AppSettings::WardEntry ward;
    ward.wardName = QObject::tr("Untitled");
    m_settings.setWard(documentId, ward);
}

void DocumentLibrary::noteDocument(const QString& documentId, const Document& document)
{
    const QString suggested = document.suggestedFilename();
    std::optional<AppSettings::WardEntry> ward = m_settings.ward(documentId);
    if (!ward.has_value() || suggested.isEmpty() || ward.value().wardName == suggested)
    {
        return;
    }

    ward.value().wardName = suggested;
    m_settings.setWard(documentId, ward.value());
}

void DocumentLibrary::markOpened(const QString& documentId)
{
    std::optional<AppSettings::WardEntry> ward = m_settings.ward(documentId);
    if (!ward.has_value())
    {
        return;
    }

    ward.value().lastOpened = QDateTime::currentDateTimeUtc();
    m_settings.setWard(documentId, ward.value());
    m_settings.setLastWardId(documentId);
}

void DocumentLibrary::forget(const QString& documentId)
{
    const DocumentPaths paths = pathsFor(documentId);
    ServerVersion::forget(m_settings, documentId, paths);
    m_settings.removeWard(documentId);
}

void DocumentLibrary::forgetRemoved(const QString& documentId)
{
    for (const QString& path : entryOf(documentId).copyPaths)
    {
        if (isCopyOf(path, documentId))
        {
            QFile::remove(path);
        }
    }
    forget(documentId);
}

bool DocumentLibrary::exportCopy(const QString& documentId,
                                 const Document& document,
                                 const QString& targetPath,
                                 QString* errorMessage)
{
    DocumentHeader first;
    first.documentId = newDocumentId();
    first.originDocumentId = documentId;
    first.keyGeneration = 1;
    first.saveCounter = 0;

    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(document, first, DocumentCrypto::generateDataKey(), m_identity);
    if (!sealed.has_value())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("The copy could not be encrypted.");
        }
        return false;
    }

    if (!DocumentFile::write(targetPath, DocumentFile::encode(sealed.value()), errorMessage))
    {
        return false;
    }

    AppSettings::WardEntry ward = entryOf(documentId);
    if (!ward.copyPaths.contains(targetPath))
    {
        ward.copyPaths.append(targetPath);
        m_settings.setWard(documentId, ward);
    }
    return true;
}

std::optional<DocumentLibrary::Copy> DocumentLibrary::openCopy(const QString& sourcePath, QString* errorMessage) const
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(sourcePath);
    DocumentFile::Sealed sealed;
    QString problem;
    if (!bytes.has_value())
    {
        problem = QObject::tr("%1 could not be read.").arg(sourcePath);
    }
    else
    {
        switch (DocumentFile::decode(bytes.value(), sealed))
        {
        case DocumentFile::Read::Ok:
            break;
        case DocumentFile::Read::NeedsNewerVersion:
            problem = QObject::tr("%1 was written by a newer version of Ward Planning.").arg(sourcePath);
            break;
        case DocumentFile::Read::Malformed:
            problem = QObject::tr("%1 is not a Ward Planning file.").arg(sourcePath);
            break;
        }
    }

    if (problem.isEmpty())
    {
        const Reconcile::DownloadCheck check = Reconcile::validateDownload(sealed.header.documentId,
                                                                           std::nullopt,
                                                                           {sealed.header},
                                                                           sealed.ciphertext,
                                                                           m_identity.self(),
                                                                           Reconcile::HighWaterMark());
        if (check.verdict == Reconcile::Verdict::Accept)
        {
            return Copy{check.document.value(), sealed.header.originDocumentId};
        }
        problem = QObject::tr("This device is not among those %1 was written for.").arg(sourcePath);
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = problem;
    }
    return std::nullopt;
}

AppSettings::WardEntry DocumentLibrary::entryOf(const QString& documentId) const
{
    return m_settings.ward(documentId).value_or(AppSettings::WardEntry());
}
