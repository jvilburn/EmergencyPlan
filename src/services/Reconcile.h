#pragma once

#include "CryptoTypes.h"
#include "Document.h"
#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/// The decisions that keep a local file safe when the server moves, as pure
/// functions over headers, ciphertext, keys, and rosters. The sync service and
/// DocumentManager supply the inputs and carry out the result.
///
/// Only Removed ever leads to deleting anything, and it is concluded only from a
/// trusted anchor and a chain that holds.
namespace Reconcile
{

enum class Verdict
{
    Accept,
    Removed,
    NeedsNewerVersion,  ///< a later format somewhere in what was fetched
    ServerFault         ///< anything that does not check out; conclude nothing
};

struct DownloadCheck
{
    Verdict verdict = Verdict::ServerFault;
    QString reason;                         ///< for the sync indicator on ServerFault
    std::optional<DocumentHeader> header;   ///< set on Accept
    std::optional<DataKey> dataKey;         ///< set on Accept
    std::optional<Document> document;       ///< set on Accept
};

/// The highest version this device has seen of one document, kept by the device.
/// A version below it is an older one being replayed.
struct HighWaterMark
{
    int keyGeneration = 0;
    qint64 saveCounter = 0;

    /// Lower on generation, or the same generation and lower on the save counter.
    bool isAbove(const DocumentHeader& header) const;
};

/// This device, as a download is checked against it.
struct Self
{
    Keypair sealing;
    VerifyKey verifyKey;

    Fingerprint fingerprint() const;
};

/// Parses every fetched header strictly. Accept with every header in out; a later
/// format anywhere is NeedsNewerVersion, whatever else is wrong, so a newer header
/// never leads to deleting anything; otherwise any malformed header is ServerFault.
Verdict readHeaders(const QList<QJsonObject>& json, QList<DocumentHeader>& out);

/// Proves removal from headers alone - no payload, so it also serves a key the
/// server now refuses. Removed only with a baseHeader of documentId and a chain
/// that holds and ends without this device; otherwise ServerFault.
Verdict checkRemoval(const QString& documentId,
                     const std::optional<DocumentHeader>& baseHeader,
                     const QList<DocumentHeader>& laterHeaders,
                     const Fingerprint& self);

/// Decides whether a server version may replace the local file.
///
/// baseHeader is the base copy's header - the last version this machine and the
/// server agreed on - never the local file's, whose offline saves the server never
/// saw and so are not in its chain. nullopt when no base copy passes its recorded
/// hash.
///
/// documentId is the document being reconciled, as the local file names it; a
/// version of any other document is ServerFault, with or without a base header.
///
/// laterHeaders ends with the header of the downloaded version, fetched in the
/// same snapshot as ciphertext; empty is ServerFault.
DownloadCheck validateDownload(const QString& documentId,
                               const std::optional<DocumentHeader>& baseHeader,
                               const QList<DocumentHeader>& laterHeaders,
                               const QByteArray& ciphertext,
                               const Self& self,
                               const HighWaterMark& highWaterMark);

} // namespace Reconcile
