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
/// server now refuses. Removed only with a baseHeader and a chain that holds and
/// ends without this device; otherwise ServerFault.
Verdict checkRemoval(const std::optional<DocumentHeader>& baseHeader,
                     const QList<DocumentHeader>& laterHeaders,
                     const Fingerprint& self);

/// Decides whether a server version may replace the local file.
///
/// baseHeader is the base copy's header - the last version this machine and the
/// server agreed on - never the local file's, whose offline saves the server never
/// saw and so are not in its chain. nullopt when no base copy passes its recorded
/// hash.
///
/// laterHeaders ends with the header of the downloaded version, fetched in the
/// same snapshot as ciphertext; empty is ServerFault.
DownloadCheck validateDownload(const std::optional<DocumentHeader>& baseHeader,
                               const QList<DocumentHeader>& laterHeaders,
                               const QByteArray& ciphertext,
                               const Self& self,
                               int keyGenerationHighWaterMark);

} // namespace Reconcile
