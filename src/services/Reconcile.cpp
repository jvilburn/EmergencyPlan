#include "Reconcile.h"

#include <QJsonDocument>
#include <QObject>
#include <QSet>

namespace
{

Reconcile::DownloadCheck fault(const QString& reason)
{
    Reconcile::DownloadCheck check;
    check.verdict = Reconcile::Verdict::ServerFault;
    check.reason = reason;
    return check;
}

} // namespace

namespace Reconcile
{

Fingerprint Self::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, verifyKey);
}

DownloadCheck validate(const QString& documentId,
                       const DocumentHeader& header,
                       const QByteArray& ciphertext,
                       const Self& self)
{
    // Another ward that also lists this device would pass every check below.
    if (header.documentId != documentId)
    {
        return fault(QObject::tr("The version is of a different document."));
    }

    const std::optional<RecipientEntry> entry = header.entryFor(self.fingerprint());
    if (!entry.has_value())
    {
        return fault(QObject::tr("The version is not sealed to this device."));
    }

    const std::optional<DataKey> dataKey = DocumentCrypto::unsealDataKey(entry.value().sealedKey, self.sealing);
    if (!dataKey.has_value())
    {
        return fault(QObject::tr("This device's key in the version cannot be opened."));
    }

    const std::optional<QByteArray> plaintext = DocumentCrypto::decryptPayload(ciphertext, dataKey.value(), header);
    if (!plaintext.has_value())
    {
        return fault(QObject::tr("The document could not be decrypted."));
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(plaintext.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        return fault(QObject::tr("The document is not readable."));
    }

    const std::optional<Document> document = Document::parse(json.object());
    if (!document.has_value())
    {
        return fault(QObject::tr("The document is not readable."));
    }

    if (!headerMatchesSealingSet(header, document.value().sealingSet()))
    {
        return fault(QObject::tr("The version disagrees with its own list of devices."));
    }

    DownloadCheck check;
    check.verdict = Verdict::Accept;
    check.header = header;
    check.dataKey = dataKey;
    check.document = document;
    return check;
}

DownloadCheck validateDownload(const QString& documentId,
                               const QJsonObject& header,
                               const QByteArray& ciphertext,
                               const Self& self)
{
    DocumentHeader parsed;
    switch (DocumentHeader::parse(header, parsed))
    {
    case HeaderParse::Ok:
        return validate(documentId, parsed, ciphertext, self);
    case HeaderParse::NeedsNewerVersion:
    {
        DownloadCheck check;
        check.verdict = Verdict::NeedsNewerVersion;
        return check;
    }
    case HeaderParse::Malformed:
        break;
    }

    return fault(QObject::tr("The server sent a header this build cannot read."));
}

bool headerMatchesSealingSet(const DocumentHeader& header, const SealingSet& set)
{
    if (header.recipients.size() != set.size())
    {
        return false;
    }

    QSet<QString> entries;
    for (const RecipientEntry& entry : header.recipients)
    {
        entries.insert(entry.fingerprint.toHex());
    }

    for (const DeviceKeys& device : set.devices())
    {
        if (!entries.contains(DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey).toHex()))
        {
            return false;
        }
    }

    // Equal sizes, every device found, and entries distinct by parse: one each.
    return entries.size() == header.recipients.size();
}

} // namespace Reconcile
