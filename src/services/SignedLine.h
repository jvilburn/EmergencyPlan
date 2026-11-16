#pragma once

#include "Id.h"

#include <QByteArray>

/// What a signed line is for.
enum class SignedCall
{
    Create,  ///< the first version of a ward, sent to the server
    Push,    ///< a later version, sent to the server
    Update   ///< a version in an update file
};

/// The UTF-8 line a device signs, which names what it is for, the ward, and the exact bytes:
/// wardplanning-v1 <call> <document> <header hash> <payload hash>, with single spaces.
class SignedLine
{
public:
    /// The line for version, a version of document in a ward file's framing: each hash is
    /// BLAKE2b-256 of the bytes on its side of the version's first newline, in base64url.
    static QByteArray ofVersion(SignedCall call, const DocumentId& document, const QByteArray& version);

    /// The line that opens document's live channel, with - for each hash it lacks.
    static QByteArray ofChannel(const DocumentId& document);
};
