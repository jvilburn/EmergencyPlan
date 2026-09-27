#include "Reconcile.h"

#include "AccessControl.h"

#include <QJsonDocument>
#include <QObject>

namespace
{

Reconcile::DownloadCheck fault(const QString& reason)
{
    Reconcile::DownloadCheck check;
    check.verdict = Reconcile::Verdict::ServerFault;
    check.reason = reason;
    return check;
}

Reconcile::DownloadCheck verdictOnly(Reconcile::Verdict verdict)
{
    Reconcile::DownloadCheck check;
    check.verdict = verdict;
    return check;
}

} // namespace

namespace Reconcile
{

Fingerprint Self::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, verifyKey);
}

Verdict readHeaders(const QList<QJsonObject>& json, QList<DocumentHeader>& out)
{
    QList<DocumentHeader> headers;
    bool malformed = false;
    bool newer = false;

    for (const QJsonObject& object : json)
    {
        DocumentHeader header;
        switch (DocumentHeader::parse(object, header))
        {
        case HeaderParse::Ok:
            headers.append(header);
            break;
        case HeaderParse::NeedsNewerVersion:
            newer = true;
            break;
        case HeaderParse::Malformed:
            malformed = true;
            break;
        }
    }

    if (newer)
    {
        return Verdict::NeedsNewerVersion;
    }

    if (malformed)
    {
        return Verdict::ServerFault;
    }

    out = headers;
    return Verdict::Accept;
}

Verdict checkRemoval(const std::optional<DocumentHeader>& baseHeader,
                     const QList<DocumentHeader>& laterHeaders,
                     const Fingerprint& self)
{
    // With no trusted anchor, a header without this device proves nothing: the
    // server could have written it.
    if (!baseHeader.has_value())
    {
        return Verdict::ServerFault;
    }

    if (DocumentCrypto::verifyChain(baseHeader.value(), laterHeaders, self) == ChainResult::Removed)
    {
        return Verdict::Removed;
    }

    return Verdict::ServerFault;
}

DownloadCheck validateDownload(const std::optional<DocumentHeader>& baseHeader,
                               const QList<DocumentHeader>& laterHeaders,
                               const QByteArray& ciphertext,
                               const Self& self,
                               int keyGenerationHighWaterMark)
{
    if (laterHeaders.isEmpty())
    {
        return fault(QObject::tr("The server sent no header for the version it offered."));
    }

    const Fingerprint fingerprint = self.fingerprint();
    const DocumentHeader& header = laterHeaders.last();

    // The only place removal is concluded.
    const std::optional<RecipientEntry> entry = header.entryFor(fingerprint);
    if (!entry.has_value())
    {
        const Verdict removal = checkRemoval(baseHeader, laterHeaders, fingerprint);
        if (removal == Verdict::Removed)
        {
            return verdictOnly(Verdict::Removed);
        }
        return fault(QObject::tr("The server's version does not list this device, "
                                 "and nothing proves it was removed."));
    }

    // With a base header the chain must hold from it. Without one, this device can
    // decrypt, so the checks below judge the version directly.
    if (baseHeader.has_value()
        && DocumentCrypto::verifyChain(baseHeader.value(), laterHeaders, fingerprint)
               != ChainResult::StillMember)
    {
        return fault(QObject::tr("The server's history of who may open this document "
                                 "does not hold."));
    }

    if (!DocumentCrypto::isCoherent(header) || !DocumentCrypto::verifyHeader(header))
    {
        return fault(QObject::tr("The server's version is not signed by its writer."));
    }

    // An entry naming this device's fingerprint with someone else's keys would seal
    // the key to a stranger while looking like this device.
    if (entry.value().publicKey != self.sealing.publicKey
        || entry.value().verifyKey != self.verifyKey)
    {
        return fault(QObject::tr("The server's version lists this device with the wrong keys."));
    }

    const std::optional<DataKey> dataKey =
        DocumentCrypto::unsealDataKey(entry.value().sealedKey, self.sealing);
    if (!dataKey.has_value())
    {
        return fault(QObject::tr("This device's key in the server's version cannot be opened."));
    }

    if (DocumentCrypto::payloadHash(ciphertext) != header.payloadHash)
    {
        return fault(QObject::tr("The server's document does not match its header."));
    }

    const std::optional<QByteArray> plaintext =
        DocumentCrypto::decryptPayload(ciphertext, dataKey.value(), header);
    if (!plaintext.has_value())
    {
        return fault(QObject::tr("The server's document could not be decrypted."));
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(plaintext.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        return fault(QObject::tr("The server's document is not readable."));
    }

    Document document;
    switch (Document::parse(json.object(), document))
    {
    case DocumentParse::Ok:
        break;
    case DocumentParse::NeedsNewerVersion:
        return verdictOnly(Verdict::NeedsNewerVersion);
    case DocumentParse::Malformed:
        return fault(QObject::tr("The server's document is not readable."));
    }

    if (!AccessControl::headerMatchesRoster(header, document.accessRoster()))
    {
        return fault(QObject::tr("The server's version disagrees with its own access list."));
    }

    // A writer in the roster with the keys the header carries, or one who has just
    // left - whose version is accepted like any other.
    if (!DocumentCrypto::writerIsAccountedFor(header, document.accessRoster()))
    {
        return fault(QObject::tr("The server's version was written by someone "
                                 "not on its access list."));
    }

    // A generation below one this device has already seen is an old key being
    // replayed.
    if (header.keyGeneration < keyGenerationHighWaterMark)
    {
        return fault(QObject::tr("The server's version uses an older key than this "
                                 "device has already seen."));
    }

    DownloadCheck check;
    check.verdict = Verdict::Accept;
    check.header = header;
    check.dataKey = dataKey;
    check.document = document;
    return check;
}

} // namespace Reconcile
