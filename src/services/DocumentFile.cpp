#include "DocumentFile.h"

#include "DocumentCrypto.h"

#include <QFile>
#include <QJsonDocument>
#include <QObject>
#include <QSaveFile>

// Every file here is opened in binary mode, so the raw XChaCha20-Poly1305 output
// reaches the disk byte for byte: every 0x0A and 0x0D stays exactly as written,
// which is what lets it authenticate when read back.

namespace DocumentFile
{

QByteArray encode(const Sealed& sealed)
{
    // Compact JSON is one line, so the first newline ends the header.
    return QJsonDocument(sealed.header.toJson()).toJson(QJsonDocument::Compact) + '\n' + sealed.ciphertext;
}

Read decode(const QByteArray& bytes, Sealed& out)
{
    const qsizetype newline = bytes.indexOf('\n');
    if (newline < 0)
    {
        return Read::Malformed;
    }

    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(bytes.left(newline), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject())
    {
        return Read::Malformed;
    }

    DocumentHeader header;
    switch (DocumentHeader::parse(json.object(), header))
    {
    case HeaderParse::Ok:
        break;
    case HeaderParse::NeedsNewerVersion:
        return Read::NeedsNewerVersion;
    case HeaderParse::Malformed:
        return Read::Malformed;
    }

    out.header = header;
    out.ciphertext = bytes.mid(newline + 1);
    return Read::Ok;
}

std::optional<Sealed> seal(const Document& document, const DocumentHeader& previous, const DataKey& dataKey)
{
    if (previous.saveCounter >= DocumentHeader::MaxSaveCounter)
    {
        return std::nullopt;
    }

    DocumentHeader header;
    header.documentId = previous.documentId;
    header.originDocumentId = previous.originDocumentId;
    header.saveCounter = previous.saveCounter + 1;

    // Rebuilt from the sealing set on every seal rather than carried forward, so a
    // mis-sealed entry cannot lock its owner out of every later version.
    for (const DeviceKeys& device : document.sealingSet().devices())
    {
        const QByteArray sealedKey = DocumentCrypto::sealDataKey(dataKey, device.publicKey);
        if (sealedKey.isEmpty())
        {
            return std::nullopt;
        }
        header.recipients.append(
            RecipientEntry{DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey), sealedKey});
    }
    if (header.recipients.isEmpty())
    {
        return std::nullopt;
    }

    // Fresh on every seal, never derived from the counter or carried over.
    header.nonce = DocumentCrypto::generateNonce();

    const QByteArray plaintext = QJsonDocument(document.toJson()).toJson(QJsonDocument::Compact);
    const QByteArray ciphertext = DocumentCrypto::encryptPayload(plaintext, dataKey, header);
    if (ciphertext.isEmpty())
    {
        return std::nullopt;
    }

    return Sealed{header, ciphertext};
}

std::optional<QByteArray> readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    return file.readAll();
}

std::optional<QByteArray> readHeader(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    return file.readLine(MaxHeaderBytes);
}

bool write(const QString& path, const QByteArray& bytes, QString* errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("Could not write %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace DocumentFile
