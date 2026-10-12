#include "ServerContract.h"

#include "DocumentCrypto.h"

#include <QJsonDocument>

namespace ServerContract
{

Fingerprint Signer::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey);
}

QByteArray headerText(const DocumentHeader& header)
{
    return QJsonDocument(header.toJson()).toJson(QJsonDocument::Compact);
}

QByteArray signedBytes(QLatin1String call, const QString& documentId, qint64 baseVersion, const Upload& upload)
{
    // Each part on its own line, so no two inputs can run together into the same bytes.
    QByteArray bytes = signedBytes(call, documentId);
    bytes += QByteArray::number(baseVersion) + '\n';
    bytes += DocumentCrypto::hash(HashDomain::SignedWrite, headerText(upload.header), DocumentCrypto::PayloadHashSize).toHex()
           + '\n';
    bytes += DocumentCrypto::hash(HashDomain::SignedWrite, upload.blob, DocumentCrypto::PayloadHashSize).toHex() + '\n';
    return bytes;
}

QByteArray signedBytes(QLatin1String call, const QString& documentId)
{
    return QByteArray(call.data(), call.size()) + '\n' + documentId.toUtf8() + '\n';
}

} // namespace ServerContract
