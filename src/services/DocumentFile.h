#pragma once

#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentHeader.h"

#include <QByteArray>
#include <QString>

#include <optional>

/// A document on disk: a single-line JSON header, a '\n' byte, then the raw
/// encrypted payload.
///
/// The same bytes are the local file and the base copy, and split at the first
/// newline they are an upload's header and blob. Reading the header is one
/// readLine(), which keeps every version check cheap.
namespace DocumentFile
{

/// One version of a document as written: its header and its ciphertext.
struct Sealed
{
    DocumentHeader header;
    QByteArray ciphertext;
};

/// The header line, '\n', and the ciphertext. The header is compact JSON, which
/// never contains a newline.
QByteArray encode(const Sealed& sealed);

enum class Read
{
    Ok,
    NeedsNewerVersion,
    Malformed
};

/// Splits at the first newline and parses the header with DocumentHeader::parse.
Read decode(const QByteArray& bytes, Sealed& out);

/// Builds the next version on previous: recipients and admins resealed from the
/// document's roster by AccessControl::resealAll, a fresh nonce, previous's save
/// counter plus one, the writer's keys, the time, the payload hash, and the
/// signature. The key generation is previous's, so a membership change that
/// rotates the key passes the header AccessControl produced. nullopt when
/// resealing or encrypting fails, or the save counter is at its cap.
///
/// The only place a save counter advances.
std::optional<Sealed> seal(const Document& document,
                           const DocumentHeader& previous,
                           const DataKey& dataKey,
                           const DeviceIdentity& writer);

/// nullopt when the file is missing or unreadable.
std::optional<QByteArray> readAll(const QString& path);

/// Through QSaveFile, so a crash mid-write leaves the previous file intact.
bool write(const QString& path, const QByteArray& bytes, QString* errorMessage);

} // namespace DocumentFile
