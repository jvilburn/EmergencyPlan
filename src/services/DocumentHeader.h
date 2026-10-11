#pragma once

#include "CryptoTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/// One device's claim on a document: its fingerprint, and the data key sealed to
/// its X25519 public key, which the payload's sealing set holds. Another way of
/// carrying the key would be a new format version, which an older build refuses
/// whole.
struct RecipientEntry
{
    Fingerprint fingerprint;
    QByteArray sealedKey;

    QJsonObject toJson() const;

    bool operator==(const RecipientEntry& other) const;
    bool operator!=(const RecipientEntry& other) const { return !(*this == other); }
};

/// Why a header could not be read.
enum class HeaderParse
{
    Ok,
    NeedsNewerVersion,  ///< a later format version
    Malformed           ///< missing or ill-formed in a version this build does know
};

/// The plaintext header of a document file. All of it is the payload's associated
/// data, so anyone holding the data key detects any change to it on decrypt.
struct DocumentHeader
{
    static constexpr int CurrentFormatVersion = 3;

    /// 128 random bits as 32 hex characters.
    static constexpr int DocumentIdLength = 32;

    /// The largest save counter a header may carry: 2^53 - 1, the largest integer
    /// a JSON number holds exactly in every reader, and far beyond any real count
    /// of saves.
    static constexpr qint64 MaxSaveCounter = (qint64(1) << 53) - 1;

    int formatVersion = CurrentFormatVersion;

    /// Set at creation and never changed. Bound into the associated data, so only
    /// someone already holding the data key can change it, and a modified file
    /// fails to decrypt.
    QString documentId;

    /// Set only on a copy: the document it came from.
    QString originDocumentId;

    qint64 saveCounter = 0;
    QByteArray nonce;                   ///< DocumentCrypto::NonceSize bytes

    /// Never empty, one per device in the sealing set, each fingerprint once.
    QList<RecipientEntry> recipients;

    QJsonObject toJson() const;

    /// The only reader. Nothing is skipped and nothing is carried through unread:
    /// a tolerant reader mostly misreads.
    static HeaderParse parse(const QJsonObject& json, DocumentHeader& out);

    /// What the payload is authenticated against: the whole header, canonical, and
    /// built from the same serialised strings toJson writes.
    QByteArray associatedData() const;

    std::optional<RecipientEntry> entryFor(const Fingerprint& fingerprint) const;

    bool operator==(const DocumentHeader& other) const;
    bool operator!=(const DocumentHeader& other) const { return !(*this == other); }
};
