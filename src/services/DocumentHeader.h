#pragma once

#include "CryptoTypes.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/// How a recipient entry carries the data key.
///
/// There is one kind. A new kind is a new format version, so an older build
/// refuses the whole file rather than reading part of it.
enum class RecipientKind
{
    X25519Sealed = 1
};

/// One device's claim on a document: its keys in the clear, and the data key
/// sealed to them.
struct RecipientEntry
{
    RecipientKind kind = RecipientKind::X25519Sealed;
    MemberId member;            ///< groups a person's devices
    Fingerprint fingerprint;    ///< fingerprintOf(publicKey, verifyKey)
    PublicKey publicKey;        ///< public; lets any reader recompute the fingerprint
    VerifyKey verifyKey;        ///< public; lets the header chain verify without a roster
    QByteArray sealedKey;

    QJsonObject toJson() const;

    bool operator==(const RecipientEntry& other) const;
    bool operator!=(const RecipientEntry& other) const { return !(*this == other); }
};

/// Why a header could not be read.
enum class HeaderParse
{
    Ok,
    NeedsNewerVersion,  ///< a later format, or a recipient kind this build does not know
    Malformed           ///< missing or ill-formed in a version this build does know
};

/// The plaintext header of a document file.
struct DocumentHeader
{
    static constexpr int CurrentFormatVersion = 2;

    /// 128 random bits as 32 hex characters.
    static constexpr int DocumentIdLength = 32;

    /// The largest save counter a header may carry: 2^53 - 1, the largest integer
    /// a JSON number holds exactly in every reader, the companion server's
    /// included, and far beyond any real count of saves.
    static constexpr qint64 MaxSaveCounter = (qint64(1) << 53) - 1;

    /// The most one upload may raise the save counter: about a million saves
    /// between publishes, months of heavy offline editing. The chain requires only
    /// that the counter rises, because a client walks it with headers missing; the
    /// server checks this bound on every upload, so no member can push the counter
    /// to MaxSaveCounter and freeze the ward.
    static constexpr qint64 MaxSaveCounterStep = qint64(1) << 20;

    int formatVersion = CurrentFormatVersion;

    /// Set at creation and never changed. Bound into the associated data, so only
    /// someone already holding the data key can change it, and a modified file
    /// fails to decrypt.
    QString documentId;

    /// Set only on a copy: the document it came from.
    QString originDocumentId;

    int keyGeneration = 1;
    qint64 saveCounter = 0;
    Fingerprint lastWriter;

    /// The last writer's own keys, so a header verifies its signature without its
    /// writer being looked up anywhere — which matters for a leave, whose writer
    /// has just left. parse refuses keys whose fingerprint is not lastWriter.
    PublicKey writerPublicKey;
    VerifyKey writerVerifyKey;

    QDateTime lastWriteTime;            ///< UTC
    QByteArray nonce;                   ///< DocumentCrypto::NonceSize bytes

    /// Empty only in the last person's leave, which nobody can open or follow.
    QList<RecipientEntry> recipients;

    /// The member ids of the people who are admins, in ascending order of hex, with
    /// no repeats — one admin set, one serialisation. Carried in the clear because
    /// a removed device proves its removal from headers alone, and that proof has
    /// to show the remover was an admin.
    QList<MemberId> admins;

    /// The two fields outside the associated data: the hash is taken of the
    /// ciphertext the associated data produces, and the signature covers both.
    QByteArray payloadHash;
    QByteArray writerSignature;

    QJsonObject toJson() const;

    /// The only reader. Nothing is skipped and nothing is carried through unread:
    /// a tolerant reader mostly misreads.
    static HeaderParse parse(const QJsonObject& json, DocumentHeader& out);

    /// What the payload is authenticated against. Canonical, and built from the
    /// same serialised strings toJson writes — never from the in-memory values,
    /// because a QDateTime that has been through a round trip is not the one that
    /// went in.
    ///
    /// Excludes payloadHash and writerSignature, which are derived from it.
    QByteArray associatedData() const;

    /// associatedData() followed by the length-prefixed payload hash. This is
    /// what the writer signs, and it is what lets a header be verified without
    /// its payload — which is what the membership chain walks.
    QByteArray signedBytes() const;

    std::optional<RecipientEntry> entryFor(const Fingerprint& fingerprint) const;
    bool isAdmin(const MemberId& member) const { return admins.contains(member); }

    bool operator==(const DocumentHeader& other) const;
    bool operator!=(const DocumentHeader& other) const { return !(*this == other); }
};
