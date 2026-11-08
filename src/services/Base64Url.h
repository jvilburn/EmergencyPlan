#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

/// Base64url without padding: the encoding of every binary value in a ward file, the key
/// file and the wire.
class Base64Url
{
public:
    static QString encode(const QByteArray& bytes);

    /// The bytes text encodes, when text is exactly the unpadded base64url encoding of length
    /// bytes. Padding, characters outside the base64url alphabet, a different length and
    /// non-zero unused trailing bits are each refused, so every value has one spelling.
    static std::optional<QByteArray> decode(const QString& text, int length);
};
