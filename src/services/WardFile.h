#pragma once

#include "Identity.h"
#include "WardHeader.h"
#include "WardPayload.h"

#include <QByteArray>
#include <QString>
#include <optional>

enum class WardReadStatus
{
    Read,
    Newer,       ///< written in a format above every version this app reads
    Unreadable,  ///< fails validation; problem says which check
    Deleted,     ///< a deleted ward's final version: its header names no recipient
    NotNamed     ///< the header names recipients, and the reader is not one of them
};

struct WardRead
{
    WardReadStatus status;
    QString problem;                     ///< when Unreadable
    std::optional<WardHeader> header;    ///< whenever the header line parsed
    QByteArray dataKey;                  ///< when Read
    std::optional<WardPayload> payload;  ///< when Read
};

/// The .wardplanning format: the header line, one newline, then the payload, compressed with
/// qCompress and encrypted with XChaCha20-Poly1305 against the header line's exact bytes.
/// A version on the server travels in the same framing.
class WardFile
{
public:
    /// The bytes of a write of payload: a fresh header sealing dataKey to exactly the devices
    /// and open invites the ward's access list holds, then the payload encrypted against that
    /// header.
    static QByteArray write(const DocumentId& document, const QByteArray& dataKey, const WardPayload& payload);

    /// bytes read as reader reads them, as kind, in this order: the format; whether the header
    /// names anyone, and the reader; the data key unseals; the payload decrypts against the
    /// header line and reads; the header matches the access list exactly.
    static WardRead read(const QByteArray& bytes, const Identity& reader, PayloadKind kind);
};
