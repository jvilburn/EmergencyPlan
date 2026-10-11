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

void onSnapshot(const DeviceIdentity& identity,
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
        done(outcomeOnly(Fetched::Outcome::CallFailed));
        return;
    }

    const Reconcile::DownloadCheck check =
        Reconcile::validateDownload(documentId, snapshot.value->header, snapshot.value->blob, identity.self());
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

    const Reconcile::DownloadCheck check = Reconcile::validate(documentId, sealed.header, sealed.ciphertext, self);
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

Access accessFor(const Head& head, const DocumentRecord& record, const QList<DeviceKeys>& linked)
{
    Access access;
    for (const Member& member : head.members)
    {
        for (const Device& device : member.person.devices)
        {
            access.allowList.insert(device.fingerprint.toHex());
        }
    }
    access.approved = record.granted + linked;
    return access;
}

Resealed reseal(const Document& document, const DataKey& dataKey, const Access& access)
{
    const SealingSet::Next next = document.sealingSet().next(access.allowList, access.approved);

    Resealed resealed{document, dataKey};
    resealed.document.setSealingSet(next.devices);
    if (next.rotate)
    {
        resealed.dataKey = DocumentCrypto::generateDataKey();
    }
    return resealed;
}

bool isCurrent(const Document& document, const Access& access)
{
    const SealingSet::Next next = document.sealingSet().next(access.allowList, access.approved);
    return !next.rotate && next.devices == document.sealingSet();
}

void settleGranted(DocumentRecords& records,
                   const QString& documentId,
                   const DocumentHeader& published,
                   const Access& access)
{
    DocumentRecord record = records.record(documentId);
    QList<DeviceKeys> waiting;
    for (const DeviceKeys& device : record.granted)
    {
        const Fingerprint fingerprint = DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey);
        if (!published.entryFor(fingerprint).has_value() && access.allowList.contains(fingerprint.toHex()))
        {
            waiting.append(device);
        }
    }

    if (waiting != record.granted)
    {
        record.granted = waiting;
        records.setRecord(documentId, record);
    }
}

void fetch(const DeviceIdentity& identity,
           ServerConnection& server,
           const QString& documentId,
           QObject* receiver,
           FetchDone done)
{
    server.download(documentId, receiver, std::bind(&onSnapshot, identity, documentId, done, _1));
}

} // namespace ServerVersion
