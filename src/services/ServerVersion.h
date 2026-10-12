#pragma once

#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentFile.h"
#include "DocumentRecords.h"
#include "ServerConnection.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

/// Taking in and writing server versions, shared by DocumentSession and
/// SealingQueue: the base copy on disk, the sealing-set rule applied to a version
/// about to be written, and fetching the server's version and validating it before
/// anything local changes.
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
/// open.
std::optional<BaseCopy> readBase(const QString& documentId,
                                 const DocumentPaths& paths,
                                 const DocumentRecords& records,
                                 const Reconcile::Self& self);

/// Writes sealed as the base copy of this server version, then records the version,
/// the hash, and the save counter.
bool writeBase(DocumentRecords& records,
               const QString& documentId,
               const DocumentPaths& paths,
               const DocumentFile::Sealed& sealed,
               qint64 version,
               QString* errorMessage);

/// Deletes the local file, the base copy, and the record.
void forget(DocumentRecords& records, const QString& documentId, const DocumentPaths& paths);

/// The data keys of the versions this device holds of a ward - its base copy and
/// its local file - which a server version must descend from.
QList<DataKey> trustedKeys(const QString& documentId,
                           const DocumentPaths& paths,
                           const DocumentRecords& records,
                           const Reconcile::Self& self);

/// What one write may seal to: the allow-list from the latest head, and the devices
/// this client approved for this ward.
struct Access
{
    QSet<QString> allowList;        ///< fingerprint hex
    QList<DeviceKeys> approved;
};

/// Whether a head lists this device. One that does not is never applied: it would
/// seal this device out of its own local file.
bool listsDevice(const ServerContract::Head& head, const Fingerprint& device);

/// The allow-list a head names, and the devices this device approved for the write:
/// the ones it linked, and for a grant or an approval the one being added.
Access accessFor(const ServerContract::Head& head, const QList<DeviceKeys>& approved);

/// The sealing-set rule applied to a document about to be written: its sealing set
/// becomes SealingSet::next, and the data key is fresh when that rotates.
struct Resealed
{
    Document document;
    DataKey dataKey;
};
Resealed reseal(const Document& document, const DataKey& dataKey, const Access& access);

/// Whether a version already seals to exactly what the rule would give, so writing
/// it again would change nothing about who can read it.
bool isCurrent(const Document& document, const Access& access);

/// What fetching the server's version found.
struct Fetched
{
    enum class Outcome
    {
        Accepted,
        AccessLost,
        NeedsNewerVersion,
        CallFailed,         ///< no answer to judge: unreachable, refused, or rate limited
        ServerFault
    };

    Outcome outcome = Outcome::ServerFault;
    QString reason;                         ///< ServerFault: why, for the sync indicator

    // Accepted
    qint64 version = 0;
    ServerContract::Publication published;
    DocumentFile::Sealed sealed;
    DataKey dataKey;
    Document document;

    // AccessLost
    std::optional<ServerContract::LostAccess> lost;

    QByteArray bytes() const { return DocumentFile::encode(sealed); }
};

using FetchDone = std::function<void(const Fetched&)>;

/// How building a resealing ended: Accepted with the resealing, or why not. A fetched
/// version that could not be sealed - which only a crypto failure causes - counts as a
/// ServerFault with its reason.
using ResealingBuilt =
    std::function<void(const Fetched& theirs, const std::optional<ServerContract::Resealing>& built)>;

/// Fetches the server's head and current version of documentId, validates it against
/// trusted as fetch does, applies the sealing-set rule with approved, encrypts, and
/// hands back what grant or approveLink sends. A head that does not list this device
/// is a ServerFault: applying it would seal this device out. Replies come only while
/// receiver lives.
void buildResealing(const Reconcile::Self& self,
                    const QList<DataKey>& trusted,
                    const QList<DeviceKeys>& approved,
                    ServerConnection& server,
                    const QString& documentId,
                    QObject* receiver,
                    ResealingBuilt done);

/// Downloads the server's current version and validates it against the keys this
/// device trusts. Nothing is written. done is called only while receiver is alive.
void fetch(const Reconcile::Self& self,
           const QList<DataKey>& trusted,
           ServerConnection& server,
           const QString& documentId,
           QObject* receiver,
           FetchDone done);

} // namespace ServerVersion
