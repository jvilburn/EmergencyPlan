#include "ServerVersion.h"

#include <QFile>
#include <QObject>

using namespace ServerContract;
using namespace std::placeholders;

namespace
{

using ServerVersion::Fetched;

Fetched outcomeOnly(Fetched::Outcome outcome)
{
    Fetched fetched;
    fetched.outcome = outcome;
    return fetched;
}

void onSnapshot(const Reconcile::Self& self,
                const QList<DataKey>& trusted,
                const QString& documentId,
                const ServerVersion::FetchDone& done,
                const Result<Snapshot>& snapshot)
{
    if (snapshot.error == ServerError::AccessLost)
    {
        Fetched fetched = outcomeOnly(Fetched::Outcome::AccessLost);
        fetched.lost = snapshot.lost;
        done(fetched);
        return;
    }

    // A call that got no answer to judge is worth trying again, whatever the
    // reason: only a version that fails validation is the server misbehaving.
    if (!snapshot.ok())
    {
        Fetched failed = outcomeOnly(Fetched::Outcome::CallFailed);
        failed.error = snapshot.error;
        done(failed);
        return;
    }

    const Reconcile::DownloadCheck check =
        Reconcile::validateDownload(documentId, snapshot.value->header, snapshot.value->blob, self, trusted);
    switch (check.verdict)
    {
    case Reconcile::Verdict::Accept:
        break;
    case Reconcile::Verdict::NeedsNewerVersion:
        done(outcomeOnly(Fetched::Outcome::NeedsNewerVersion));
        return;
    case Reconcile::Verdict::ServerFault:
    {
        Fetched fetched = outcomeOnly(Fetched::Outcome::ServerFault);
        fetched.reason = check.reason;
        done(fetched);
        return;
    }
    }

    Fetched fetched = outcomeOnly(Fetched::Outcome::Accepted);
    fetched.version = snapshot.value->version;
    fetched.published = snapshot.value->published;
    fetched.sealed = DocumentFile::Sealed{check.header.value(), snapshot.value->blob};
    fetched.dataKey = check.dataKey.value();
    fetched.document = check.document.value();
    done(fetched);
}

} // namespace

namespace ServerVersion
{

std::optional<BaseCopy> readBase(const QString& documentId,
                                 const DocumentPaths& paths,
                                 const DocumentRecords& records,
                                 const Reconcile::Self& self)
{
    const DocumentRecord record = records.record(documentId);
    const std::optional<QByteArray> bytes = DocumentFile::readAll(paths.base);
    if (!record.baseVersion.has_value() || !bytes.has_value())
    {
        return std::nullopt;
    }

    DocumentFile::Sealed sealed;
    if (DocumentFile::decode(bytes.value(), sealed) != DocumentFile::Read::Ok
        || DocumentCrypto::payloadHash(sealed.ciphertext) != record.baseHash)
    {
        return std::nullopt;
    }

    const Reconcile::DownloadCheck check = Reconcile::validate(documentId, sealed.header, sealed.ciphertext, self, {});
    if (check.verdict != Reconcile::Verdict::Accept)
    {
        return std::nullopt;
    }

    return BaseCopy{sealed, check.document.value(), check.dataKey.value()};
}

bool writeBase(DocumentRecords& records,
               const QString& documentId,
               const DocumentPaths& paths,
               const DocumentFile::Sealed& sealed,
               qint64 version,
               QString* errorMessage)
{
    if (!DocumentFile::write(paths.base, DocumentFile::encode(sealed), errorMessage))
    {
        return false;
    }

    DocumentRecord record = records.record(documentId);
    record.baseVersion = version;
    record.baseHash = DocumentCrypto::payloadHash(sealed.ciphertext);
    record.baseSaveCounter = sealed.header.saveCounter;
    records.setRecord(documentId, record);
    return true;
}

void forget(DocumentRecords& records, const QString& documentId, const DocumentPaths& paths)
{
    QFile::remove(paths.file);
    QFile::remove(paths.base);
    records.removeRecord(documentId);
}

QList<DataKey> trustedKeys(const QString& documentId,
                           const DocumentPaths& paths,
                           const DocumentRecords& records,
                           const Reconcile::Self& self)
{
    QList<DataKey> keys;
    const std::optional<BaseCopy> base = readBase(documentId, paths, records, self);
    if (base.has_value())
    {
        keys.append(base.value().dataKey);
    }

    DocumentFile::Sealed local;
    const std::optional<QByteArray> bytes = DocumentFile::readAll(paths.file);
    if (bytes.has_value() && DocumentFile::decode(bytes.value(), local) == DocumentFile::Read::Ok)
    {
        const Reconcile::DownloadCheck check = Reconcile::validate(documentId, local.header, local.ciphertext, self, {});
        if (check.verdict == Reconcile::Verdict::Accept)
        {
            keys.append(check.dataKey.value());
        }
    }
    return keys;
}

bool listsDevice(const Head& head, const Fingerprint& device)
{
    for (const Member& member : head.members)
    {
        for (const Device& listed : member.person.devices)
        {
            if (listed.fingerprint == device)
            {
                return true;
            }
        }
    }
    return false;
}

Access accessFor(const Head& head, const QList<DeviceKeys>& approved)
{
    Access access;
    for (const Member& member : head.members)
    {
        for (const Device& device : member.person.devices)
        {
            access.allowList.insert(device.fingerprint.toHex());
        }
    }
    access.approved = approved;
    return access;
}

Resealed reseal(const Document& document, const DataKey& dataKey, const Access& access)
{
    const SealingSet::Next next = document.sealingSet().next(access.allowList, access.approved);

    Resealed resealed{document, dataKey};
    SealingSet sealingSet = next.devices;
    if (next.rotate)
    {
        sealingSet.addFormerKey(dataKey);
        resealed.dataKey = DocumentCrypto::generateDataKey();
    }
    resealed.document.setSealingSet(sealingSet);
    return resealed;
}

bool isCurrent(const Document& document, const Access& access)
{
    const SealingSet::Next next = document.sealingSet().next(access.allowList, access.approved);
    return !next.rotate && next.devices == document.sealingSet();
}

void fetch(const Reconcile::Self& self,
           const QList<DataKey>& trusted,
           ServerConnection& server,
           const QString& documentId,
           QObject* receiver,
           FetchDone done)
{
    server.download(documentId, receiver, std::bind(&onSnapshot, self, trusted, documentId, done, _1));
}

namespace
{

void onResealingFetched(const QString& documentId,
                        const Access& access,
                        const ResealingBuilt& done,
                        const Fetched& theirs)
{
    if (theirs.outcome != Fetched::Outcome::Accepted)
    {
        done(theirs, std::nullopt);
        return;
    }

    const Resealed resealed = reseal(theirs.document, theirs.dataKey, access);
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(resealed.document, theirs.sealed.header, resealed.dataKey);
    if (!sealed.has_value())
    {
        Fetched failed = outcomeOnly(Fetched::Outcome::ServerFault);
        failed.reason = QObject::tr("The version could not be sealed on this device.");
        done(failed, std::nullopt);
        return;
    }

    done(theirs, Resealing{documentId, theirs.version, Upload{sealed->header, sealed->ciphertext}});
}

void onResealingHead(const Reconcile::Self& self,
                     const QList<DataKey>& trusted,
                     const QList<DeviceKeys>& linked,
                     const DeviceKeys& admitted,
                     ServerConnection& server,
                     const QString& documentId,
                     QObject* receiver,
                     const ResealingBuilt& done,
                     const Result<Head>& head)
{
    if (head.error == ServerError::AccessLost)
    {
        Fetched lost = outcomeOnly(Fetched::Outcome::AccessLost);
        lost.lost = head.lost;
        done(lost, std::nullopt);
        return;
    }
    if (!head.ok())
    {
        Fetched failed = outcomeOnly(Fetched::Outcome::CallFailed);
        failed.error = head.error;
        done(failed, std::nullopt);
        return;
    }
    if (!listsDevice(head.value.value(), self.fingerprint()))
    {
        Fetched fault = outcomeOnly(Fetched::Outcome::ServerFault);
        fault.reason = QObject::tr("The server's list of who has access leaves out this device.");
        done(fault, std::nullopt);
        return;
    }

    // The head does not list admitted yet; the grant or approval this goes up with
    // adds it as it stores the version.
    Access access = accessFor(head.value.value(), linked + QList<DeviceKeys>{admitted});
    access.allowList.insert(DocumentCrypto::fingerprintOf(admitted.publicKey, admitted.verifyKey).toHex());
    fetch(self, trusted, server, documentId, receiver, std::bind(&onResealingFetched, documentId, access, done, _1));
}

} // namespace

void buildResealing(const Reconcile::Self& self,
                    const QList<DataKey>& trusted,
                    const QList<DeviceKeys>& linked,
                    const DeviceKeys& admitted,
                    ServerConnection& server,
                    const QString& documentId,
                    QObject* receiver,
                    ResealingBuilt done)
{
    server.head(documentId,
                receiver,
                std::bind(&onResealingHead, self, trusted, linked, admitted, std::ref(server), documentId, receiver, done, _1));
}

} // namespace ServerVersion
