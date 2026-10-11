#pragma once

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

/// Builds the next version on previous: its document id and origin, the data key
/// sealed to every device in the document's sealing set, a fresh nonce, and
/// previous's save counter plus one. The caller has already decided the set and the
/// key; see ServerVersion::reseal. nullopt when sealing or encrypting fails, or the
/// save counter is at its cap.
///
/// The only place a save counter advances.
std::optional<Sealed> seal(const Document& document, const DocumentHeader& previous, const DataKey& dataKey);

/// nullopt when the file is missing or unreadable.
std::optional<QByteArray> readAll(const QString& path);

/// The header line and its newline, without the payload, for deciding what a file
/// is; decode reads it as a version with no ciphertext. A file with no newline in
/// its first MaxHeaderBytes gives those bytes, which decode calls malformed.
/// nullopt when the file is missing or unreadable.
std::optional<QByteArray> readHeader(const QString& path);

constexpr qint64 MaxHeaderBytes = 1024 * 1024;

/// Through QSaveFile, so a crash mid-write leaves the previous file intact.
bool write(const QString& path, const QByteArray& bytes, QString* errorMessage);

} // namespace DocumentFile
