#pragma once

#include "Id.h"
#include "Identity.h"
#include "WardHeader.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <optional>

enum class UpdateReadStatus
{
    Read,
    Newer,      ///< the version is in a format above every version this app reads
    Unreadable  ///< fails validation; problem says where
};

/// An update file split into its parts, with the version's header read.
struct UpdateRead
{
    UpdateReadStatus status;
    QString problem;                   ///< when Unreadable
    QByteArray signature;              ///< when Read
    QByteArray version;                ///< when Read: the version, in a ward file's framing
    std::optional<WardHeader> header;  ///< when Read
};

/// A .wardupdate file: one line holding the writer's Ed25519 signature over the version's
/// update line, one newline, then the version in a ward file's framing, with no device part.
class UpdateFile
{
public:
    /// The bytes of an update file carrying version, a version of document, signed by writer.
    static QByteArray write(const DocumentId& document, const QByteArray& version, const Identity& writer);

    /// bytes split at the signature line's newline, the signature read as base64url of 64
    /// bytes, and the version's header line read as WardHeader::parse reads it.
    static UpdateRead read(const QByteArray& bytes);

    /// True when update's signature is verifyKey's signature of its version's update line.
    static bool verifies(const UpdateRead& update, const QByteArray& verifyKey);

    /// The name an update of the ward named wardName, written on date, is saved under, such as
    /// Tanglewood Ward 2026-11-08.wardupdate.
    static QString fileName(const QString& wardName, const QDate& date);

private:
    /// The line a version of document is signed over:
    /// wardplanning-v1 update <document> <header hash> <payload hash>, each hash BLAKE2b-256
    /// of the bytes on its side of the version's first newline, all three in base64url.
    static QByteArray signedLine(const DocumentId& document, const QByteArray& version);
};
