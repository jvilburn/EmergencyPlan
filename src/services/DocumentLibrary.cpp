#include "DocumentLibrary.h"

#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "ServerConnection.h"
#include "ServerVersion.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <algorithm>

using namespace std::placeholders;

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

/// Whether the file at the copy's path is still that copy.
bool isStillThere(const AppSettings::ExportedCopy& copy)
{
    const std::optional<QByteArray> bytes = DocumentFile::readHeader(copy.path);
    DocumentFile::Sealed sealed;
    return bytes.has_value()
        && DocumentFile::decode(bytes.value(), sealed) == DocumentFile::Read::Ok
        && sealed.header.documentId == copy.documentId;
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
        const AppSettings::WardEntry ward = m_settings.ward(documentId).value();
        entries.append(Entry{documentId,
                             ward.wardName.value_or(QObject::tr("Untitled")),
                             ward.lastOpened,
                             m_settings.record(documentId).baseVersion.has_value(),
                             QFile::exists(pathsFor(documentId).file)});
    }
    std::sort(entries.begin(), entries.end(), sortsBefore);
    return entries;
}

DocumentPaths DocumentLibrary::pathsFor(const QString& documentId) const
{
    if (!contains(documentId))
    {
        return DocumentPaths();
    }

    const QDir directory(m_directory);
    return DocumentPaths{directory.filePath(documentId + DocumentConstants::FileExtension),
                         directory.filePath(documentId + ".base")};
}

QString DocumentLibrary::newDocumentId()
{
    return QString::fromLatin1(DocumentCrypto::randomBytes(DocumentIdBytes).toHex());
}

void DocumentLibrary::add(const QString& documentId)
{
    if (m_settings.ward(documentId).has_value())
    {
        return;
    }

    QDir().mkpath(m_directory);
    m_settings.setWard(documentId, AppSettings::WardEntry());
}

bool DocumentLibrary::addFromFile(const QString& documentId, const QString& sourcePath, QString* errorMessage)
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(sourcePath);
    if (!bytes.has_value())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("%1 could not be read.").arg(sourcePath);
        }
        return false;
    }

    add(documentId);
    if (!DocumentFile::write(pathsFor(documentId).file, bytes.value(), errorMessage))
    {
        m_settings.removeWard(documentId);
        return false;
    }
    return true;
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

QString DocumentLibrary::lastOpenedId() const
{
    const QString documentId = m_settings.lastWardId();
    return m_settings.ward(documentId).has_value() ? documentId : QString();
}

void DocumentLibrary::forget(const QString& documentId)
{
    const DocumentPaths paths = pathsFor(documentId);
    ServerVersion::forget(m_settings, documentId, paths);
    m_settings.removeWard(documentId);
}

void DocumentLibrary::forgetRemoved(const QString& documentId)
{
    const std::optional<AppSettings::WardEntry> ward = m_settings.ward(documentId);
    if (ward.has_value())
    {
        for (const AppSettings::ExportedCopy& copy : ward.value().copies)
        {
            if (isStillThere(copy))
            {
                QFile::remove(copy.path);
            }
        }
    }
    forget(documentId);
}

bool DocumentLibrary::exportCopy(const QString& documentId,
                                 const Document& document,
                                 const QString& targetPath,
                                 QString* errorMessage)
{
    std::optional<AppSettings::WardEntry> ward = m_settings.ward(documentId);
    QString problem;
    if (!ward.has_value())
    {
        problem = QObject::tr("Only a ward on this device can be copied.");
    }
    else if (QFileInfo(QFileInfo(targetPath).absolutePath()) == QFileInfo(m_directory))
    {
        problem = QObject::tr("A copy cannot be saved among the wards' own files. Choose another folder.");
    }
    if (!problem.isEmpty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = problem;
        }
        return false;
    }

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

    // A copy written over an earlier one at the same path replaces it.
    QList<AppSettings::ExportedCopy>& copies = ward.value().copies;
    copies.removeIf([&](const AppSettings::ExportedCopy& copy) { return copy.path == targetPath; });
    copies.append(AppSettings::ExportedCopy{targetPath, sealed.value().header.documentId});
    m_settings.setWard(documentId, ward.value());
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

    if (problem.isEmpty() && sealed.header.originDocumentId.isEmpty())
    {
        problem = QObject::tr("%1 is a ward's own file, not a copy.").arg(sourcePath);
    }

    if (problem.isEmpty() && !sealed.header.entryFor(m_identity.fingerprint()).has_value())
    {
        problem = QObject::tr("This device is not among those %1 was written for.").arg(sourcePath);
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
        problem = QObject::tr("%1 is damaged, or was changed after it was written.").arg(sourcePath);
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = problem;
    }
    return std::nullopt;
}

// --- From the server -------------------------------------------------------------

/// One restore in flight: the wards still being fetched, and those that arrived.
struct DocumentLibrary::Restore
{
    QStringList pending;
    QStringList arrived;
    Restored done;
};

void DocumentLibrary::restore(ServerConnection& server, QObject* receiver, const Restored& done)
{
    std::shared_ptr<Restore> restore = std::make_shared<Restore>();
    restore->done = done;
    server.myDocuments(receiver, std::bind(&DocumentLibrary::onListed, this, std::ref(server), receiver, restore, _1));
}

void DocumentLibrary::onListed(ServerConnection& server,
                               QObject* receiver,
                               const std::shared_ptr<Restore>& restore,
                               const ServerContract::Result<QStringList>& listed)
{
    if (listed.ok())
    {
        for (const QString& documentId : listed.value.value())
        {
            if (!contains(documentId))
            {
                restore->pending.append(documentId);
            }
        }
    }

    if (restore->pending.isEmpty())
    {
        restore->done(QStringList());
        return;
    }

    const QStringList toFetch = restore->pending;
    for (const QString& documentId : toFetch)
    {
        fetchWard(documentId, server, receiver, std::bind(&DocumentLibrary::onRestored, this, restore, documentId, _1));
    }
}

void DocumentLibrary::onRestored(const std::shared_ptr<Restore>& restore,
                                 const QString& documentId,
                                 DocumentSession::Download outcome)
{
    restore->pending.removeOne(documentId);
    if (outcome == DocumentSession::Download::Done)
    {
        restore->arrived.append(documentId);
    }
    else
    {
        // Not listed until it arrives, so the next restore tries it afresh.
        forget(documentId);
    }

    if (restore->pending.isEmpty())
    {
        restore->done(restore->arrived);
    }
}

void DocumentLibrary::fetchWard(const QString& documentId,
                                ServerConnection& server,
                                QObject* receiver,
                                const Fetched& done)
{
    add(documentId);
    DocumentSession::download(DocumentSession::Context{m_identity, m_settings, server},
                              documentId,
                              pathsFor(documentId),
                              receiver,
                              std::bind(&DocumentLibrary::onWardFetched, this, documentId, done, _1));
}

void DocumentLibrary::onWardFetched(const QString& documentId, const Fetched& done, DocumentSession::Download outcome)
{
    switch (outcome)
    {
    case DocumentSession::Download::Done:
        noteFromFile(documentId);
        break;
    case DocumentSession::Download::Removed:
        // The download has already deleted its files and record.
        m_settings.removeWard(documentId);
        break;
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::NeedsNewerVersion:
    case DocumentSession::Download::ServerFault:
        break;
    }
    done(outcome);
}

void DocumentLibrary::noteFromFile(const QString& documentId)
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(pathsFor(documentId).file);
    DocumentFile::Sealed sealed;
    if (!bytes.has_value() || DocumentFile::decode(bytes.value(), sealed) != DocumentFile::Read::Ok)
    {
        return;
    }

    const Reconcile::DownloadCheck check = Reconcile::validateDownload(documentId,
                                                                       std::nullopt,
                                                                       {sealed.header},
                                                                       sealed.ciphertext,
                                                                       m_identity.self(),
                                                                       m_settings.record(documentId).highWaterMark);
    if (check.verdict == Reconcile::Verdict::Accept)
    {
        noteDocument(documentId, check.document.value());
    }
}
