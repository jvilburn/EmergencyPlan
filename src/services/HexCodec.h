#pragma once

#include <QByteArray>
#include <QString>

/// Hex encoding for the parts of the document format that carry raw bytes.
///
/// Qt already encodes and decodes hex; what it does not offer is a decode that
/// fails. QByteArray::fromHex skips characters it does not recognise and carries
/// on, so a malformed string decodes to a short array rather than being refused.
/// Everything here parses bytes that arrived from a server, so refusal is the
/// behaviour that matters.
namespace HexCodec
{

/// Lowercase, which is the only spelling decode() accepts.
QString encode(const QByteArray& bytes);

/// Returns an empty array unless the text is exactly expectedSize bytes of
/// lowercase hex.
QByteArray decode(const QString& text, int expectedSize);

/// True for exactly `characters` lowercase hex digits.
bool isCanonical(const QString& text, int characters);

} // namespace HexCodec
