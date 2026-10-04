#pragma once

#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentFile.h"
#include "DocumentRecords.h"
#include "ServerConnection.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

/// Taking in a server version, shared by DocumentSession and RosterChangeQueue:
/// the base copy on disk, and fetching the server's version and validating it
/// before anything local changes.
namespace ServerVersion
{

/// The base copy: the exact bytes of the server version this device last took in
/// or published, decrypted.
struct BaseCopy
{
    DocumentFile::Sealed sealed;
    Document document;
    DataKey dataKey;
};

/// nullopt when the file is missing, its hash is not the record's, or it does not
/// open. Read without the high-water mark: it is older than the document by design.
std::optional<BaseCopy> readBase(const QString& documentId,
                                 const DocumentPaths& paths,
                                 const DocumentRecords& records,
                                 const Reconcile::Self& self);

/// Writes bytes as the base copy of this server version, then records the version
/// and hash and raises the high-water mark to its header.
bool writeBase(DocumentRecords& records,
               const QString& documentId,
               const DocumentPaths& paths,
               const DocumentFile::Sealed& sealed,
               qint64 version,
               QString* errorMessage);

/// Deletes the local file, the base copy, and the record.
void forget(DocumentRecords& records, const QString& documentId, const DocumentPaths& paths);

/// What fetching the server's version found.
struct Fetched
{
    enum class Outcome
    {
        Accepted,
        Removed,
        NeedsNewerVersion,
        CallFailed,         ///< no answer to judge: unreachable, refused, or rate limited
        ServerFault
    };

    Outcome outcome = Outcome::ServerFault;
    QString reason;                         ///< ServerFault: why, for the sync indicator

    // Accepted
    qint64 version = 0;
    DocumentFile::Sealed sealed;
    DataKey dataKey;
    Document document;

    /// Accepted and Removed: the headers fetched after the base.
    QList<DocumentHeader> laterHeaders;

    // Removed
    Fingerprint remover;                    ///< who wrote the header that dropped this device
    bool left = false;                      ///< that header was this person's own leave

    QByteArray bytes() const { return DocumentFile::encode(sealed); }
};

using FetchDone = std::function<void(const Fetched&)>;

/// Downloads the snapshot after the record's base version and validates it
/// against the base copy, or directly when none passes its hash. When the server
/// refuses this device, fetches the header chain instead and looks for proof of
/// removal. Nothing is written. done is called only while receiver is alive.
void fetch(const DeviceIdentity& identity,
           DocumentRecords& records,
           ServerConnection& server,
           const QString& documentId,
           const DocumentPaths& paths,
           QObject* receiver,
           FetchDone done);

/// The removal check alone, for a device the server has just refused: the header
/// chain after the base version, proved from the base copy's header. Removed or
/// ServerFault, or CallFailed.
void proveRemoval(const DeviceIdentity& identity,
                  DocumentRecords& records,
                  ServerConnection& server,
                  const QString& documentId,
                  const DocumentPaths& paths,
                  QObject* receiver,
                  FetchDone done);

} // namespace ServerVersion
