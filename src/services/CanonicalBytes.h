#pragma once

#include <QByteArray>
#include <QString>

/// The one canonical byte encoding used wherever this format has to hash or sign
/// a structure: a document header's associated data.
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
    CanonicalBytes& append(const QByteArray& value);
    CanonicalBytes& append(const QString& text);
    CanonicalBytes& append(qint64 number);

    QByteArray bytes() const { return m_bytes; }

private:
    QByteArray m_bytes;
};
