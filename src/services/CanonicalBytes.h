#pragma once

#include <QByteArray>
#include <QString>

/// The canonical byte encoding of a document header's associated data.
///
/// Every field is written as a four-byte big-endian length followed by its bytes,
/// so no field can be mistaken for part of another, no byte value has to be
/// forbidden inside a value, and two different structures cannot produce the same
/// bytes.
///
/// Counts are written as fields of their own before a repeated group, so a
/// shorter group followed by a longer one cannot be re-read as the reverse.
class CanonicalBytes
{
public:
    /// One name per kind of value, so a string literal or a bool always reaches
    /// the encoding the caller named.
    CanonicalBytes& appendText(const QString& text);        ///< UTF-8
    CanonicalBytes& appendNumber(qint64 number);            ///< decimal ASCII

    QByteArray bytes() const { return m_bytes; }

private:
    CanonicalBytes& appendBytes(const QByteArray& value);

    QByteArray m_bytes;
};
