#pragma once

#include "CryptoTypes.h"
#include "Document.h"
#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

/// Whether a version may become this device's local file, as pure functions over a
/// header, ciphertext, and this device's keys. A server download and a local file
/// opening are judged by the same rules.
namespace Reconcile
{

enum class Verdict
{
    Accept,
    NeedsNewerVersion,  ///< a later format
    ServerFault         ///< anything that does not check out; the local file is untouched
};

struct DownloadCheck
{
    Verdict verdict = Verdict::ServerFault;
    QString reason;                         ///< for the sync indicator on ServerFault
    std::optional<DocumentHeader> header;   ///< set on Accept
    std::optional<DataKey> dataKey;         ///< set on Accept
    std::optional<Document> document;       ///< set on Accept
};

/// This device, as a version is checked against it.
struct Self
{
    Keypair sealing;
    VerifyKey verifyKey;

    Fingerprint fingerprint() const;
};

/// A version this device can take: the header is of documentId and lists this
/// device, its key unseals, the payload decrypts and parses, and the header matches
/// the payload's sealing set.
DownloadCheck validate(const QString& documentId,
                       const DocumentHeader& header,
                       const QByteArray& ciphertext,
                       const Self& self);

/// validate, for a header as the server stored it: one in a later format is
/// NeedsNewerVersion, and one that does not parse is ServerFault.
DownloadCheck validateDownload(const QString& documentId,
                               const QJsonObject& header,
                               const QByteArray& ciphertext,
                               const Self& self);

/// Every recipient entry's fingerprint is one device in the set, and every device
/// in the set has exactly one entry. The agreement the server cannot check, since it
/// never sees the payload.
bool headerMatchesSealingSet(const DocumentHeader& header, const SealingSet& set);

} // namespace Reconcile
