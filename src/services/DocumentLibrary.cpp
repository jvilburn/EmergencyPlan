#include "DocumentLibrary.h"

#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "HexCodec.h"
#include "ServerConnection.h"
#include "ServerVersion.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
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
    return contains(documentId) ? pathsIn(documentId) : DocumentPaths();
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

void DocumentLibrary::forgetLost(const QString& documentId)
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

    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(document, first, DocumentCrypto::generateDataKey());
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

bool DocumentLibrary::writeRecoveryCopy(const QString& documentId,
                                        const Document& document,
                                        const QString& directory,
                                        QString* path,
                                        QString* errorMessage)
{
    // A name the file system takes on any platform: no reserved or control
    // characters, and no trailing dot or space, which Windows drops.
    QString wardName = m_settings.ward(documentId).value_or(AppSettings::WardEntry()).wardName.value_or(
        QObject::tr("Untitled"));
    wardName.replace(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]"), "-");
    while (wardName.endsWith('.') || wardName.endsWith(' '))
    {
        wardName.chop(1);
    }
    if (wardName.isEmpty())
    {
        wardName = QObject::tr("Untitled");
    }

    // Never over an earlier copy or anyone's file: a copy that replaced one would
    // destroy what it exists to keep.
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm");
    const QString target = unusedPath(directory, QObject::tr("%1 (unsaved changes %2)").arg(wardName, stamp));
    if (!QDir().mkpath(directory) || !exportCopy(documentId, document, target, errorMessage))
    {
        if (errorMessage != nullptr && errorMessage->isEmpty())
        {
            *errorMessage = QObject::tr("%1 could not be written.").arg(target);
        }
        return false;
    }

    if (path != nullptr)
    {
        *path = target;
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
        const Reconcile::DownloadCheck check =
            Reconcile::validate(sealed.header.documentId, sealed.header, sealed.ciphertext, m_identity.self(), {});
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

void DocumentLibrary::fetchWard(const QString& documentId,
                                ServerConnection& server,
                                QObject* receiver,
                                const WardFetched& fetched)
{
    // The folder the download writes into, which the first ward on a device makes.
    QDir().mkpath(m_directory);
    DocumentSession::download(DocumentSession::Context{m_identity, m_settings, server},
                              documentId,
                              pathsIn(documentId),
                              receiver,
                              std::bind(&DocumentLibrary::onWardFetched, this, documentId, fetched, _1, _2));
}

void DocumentLibrary::onWardFetched(const QString& documentId,
                                    const WardFetched& fetched,
                                    DocumentSession::Download outcome,
                                    const std::optional<Document>& document)
{
    if (outcome == DocumentSession::Download::Done)
    {
        takeIn(documentId, document.value());
    }
    fetched(documentId, outcome);
}

void DocumentLibrary::join(const InviteLink& link,
                           ServerConnection& server,
                           QObject* receiver,
                           const DocumentSession::Joined& joined)
{
    QDir().mkpath(m_directory);
    DocumentSession::join(DocumentSession::Context{m_identity, m_settings, server},
                          link,
                          pathsIn(link.documentId),
                          receiver,
                          std::bind(&DocumentLibrary::onJoined, this, link.documentId, joined, _1, _2, _3));
}

void DocumentLibrary::onJoined(const QString& documentId,
                               const DocumentSession::Joined& joined,
                               DocumentSession::Join outcome,
                               const QString& reason,
                               const std::optional<Document>& document)
{
    if (outcome == DocumentSession::Join::Done)
    {
        takeIn(documentId, document.value());
    }
    joined(outcome, reason, document);
}

void DocumentLibrary::takeIn(const QString& documentId, const Document& document)
{
    add(documentId);
    noteDocument(documentId, document);
}

DocumentPaths DocumentLibrary::pathsIn(const QString& documentId) const
{
    const QDir directory(m_directory);
    return DocumentPaths{directory.filePath(documentId + DocumentConstants::FileExtension),
                         directory.filePath(documentId + ".base")};
}

QString DocumentLibrary::unusedPath(const QString& directory, const QString& baseName)
{
    const QDir folder(directory);
    QString candidate = folder.filePath(baseName + DocumentConstants::FileExtension);
    for (int count = 2; QFileInfo::exists(candidate); ++count)
    {
        candidate = folder.filePath(QString("%1 (%2)").arg(baseName).arg(count) + DocumentConstants::FileExtension);
    }
    return candidate;
}
