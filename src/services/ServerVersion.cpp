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
    // A call that got no answer to judge is worth trying again, whatever the
    // reason: only a version that fails validation is the server misbehaving.
    if (!snapshot.ok())
    {
        Fetched failed = outcomeOnly(Fetched::Outcome::CallFailed);
        failed.error = snapshot.error;
        done(failed);
        return;
    }

    // A version not sealed to this device is one this device no longer holds.
    DocumentHeader header;
    if (DocumentHeader::parse(snapshot.value->header, header) == HeaderParse::Ok
        && !header.entryFor(self.fingerprint()).has_value())
    {
        Fetched lost = outcomeOnly(Fetched::Outcome::AccessLost);
        lost.published = snapshot.value->published;
        done(lost);
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

LostAccess whyLost(const SealingSet& previousRoster, const Fingerprint& self, const Fingerprint& publisher)
{
    const std::optional<Holder> me = previousRoster.personOf(self);
    const std::optional<Holder> writer = previousRoster.personOf(publisher);
    if (!me.has_value() || !writer.has_value() || writer->id != me->id)
    {
        return LostAccess{LostAccess::How::Removed, writer.has_value() ? writer->name : QString()};
    }

    // My own person wrote it: a leave from another device, or an unlink of this one.
    if (publisher == self || me->devices.size() == 1)
    {
        return LostAccess{LostAccess::How::Left, QString()};
    }
    const std::optional<NamedDevice> device = previousRoster.device(publisher);
    return LostAccess{LostAccess::How::Unlinked, device.has_value() ? device->name : QString()};
}

Resealed reseal(const Document& document, const DataKey& dataKey, const DocumentHeader& previous)
{
    QSet<QString> previousRecipients;
    for (const RecipientEntry& entry : previous.recipients)
    {
        previousRecipients.insert(entry.fingerprint.toHex());
    }
    const SealingSet::Next next = document.sealingSet().next(previousRecipients);

    Resealed resealed{document, dataKey};
    SealingSet sealingSet = next.set;
    if (next.rotate)
    {
        sealingSet.addFormerKey(dataKey);
        resealed.dataKey = DocumentCrypto::generateDataKey();
    }
    resealed.document.setSealingSet(sealingSet);
    return resealed;
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

} // namespace ServerVersion
