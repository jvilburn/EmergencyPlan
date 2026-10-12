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

/// Taking in and writing server versions: the base copy on disk, the sealing-set rule
/// applied to a version about to be written, and fetching the server's version and
/// validating it before anything local changes.
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

/// Why this device no longer holds a ward, read off the roster it last held and who
/// published the version that left it out.
struct LostAccess
{
    enum class How
    {
        Removed,        ///< another person wrote a version without this person
        Left,           ///< this person's own device wrote one without this person
        Unlinked        ///< this person's own device wrote one without this device
    };

    How how = How::Removed;
    QString byWhom;     ///< the publisher's name for Removed; the device's name for Unlinked; empty for Left

    bool operator==(const LostAccess& other) const { return how == other.how && byWhom == other.byWhom; }
    bool operator!=(const LostAccess& other) const { return !(*this == other); }
};

/// The reason a header that leaves out self gives, from the roster this device last
/// read and the device that published the version.
LostAccess whyLost(const SealingSet& previousRoster, const Fingerprint& self, const Fingerprint& publisher);

/// The sealing-set rule applied to a document about to be written on top of the
/// version whose header is previous: the data key is fresh when that version sealed
/// to anyone the document's roster now lacks.
struct Resealed
{
    Document document;
    DataKey dataKey;
};
Resealed reseal(const Document& document, const DataKey& dataKey, const DocumentHeader& previous);

/// What fetching the server's version found.
struct Fetched
{
    enum class Outcome
    {
        Accepted,
        AccessLost,         ///< the version's header does not seal to self
        NeedsNewerVersion,
        CallFailed,         ///< no answer to judge: unreachable, refused, or rate limited
        ServerFault
    };

    Outcome outcome = Outcome::ServerFault;
    QString reason;                         ///< ServerFault: why, for the sync indicator
    ServerContract::ServerError error = ServerContract::ServerError::None;   ///< CallFailed: the server's answer

    // Accepted
    qint64 version = 0;
    ServerContract::Publication published;
    DocumentFile::Sealed sealed;
    DataKey dataKey;
    Document document;

    QByteArray bytes() const { return DocumentFile::encode(sealed); }
};

using FetchDone = std::function<void(const Fetched&)>;

/// Fetches the server's current version of documentId and validates it for self
/// against trusted, as Reconcile does for a local file. Replies come only while
/// receiver lives.
void fetch(const Reconcile::Self& self,
           const QList<DataKey>& trusted,
           ServerConnection& server,
           const QString& documentId,
           QObject* receiver,
           FetchDone done);

} // namespace ServerVersion
