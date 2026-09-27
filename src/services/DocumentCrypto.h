#pragma once

#include "CryptoTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

class AccessRoster;
struct DocumentHeader;

/// What walking the header chain proved about one device.
enum class ChainResult
{
    StillMember,    ///< the chain holds and the last header still lists this device
    Removed,        ///< the chain holds and it does not - the only result that deletes anything
    Broken          ///< some link failed; conclude nothing
};

/// What a signature is for.
///
/// Every signature is tagged. sign() and verify() prepend the tag's ASCII bytes
/// and one 0x00 byte to the message, so a signature made for one purpose never
/// verifies as another. The tag is never stored and never transmitted, and there
/// is no untagged overload, so no code path can sign raw bytes.
///
/// This is what stops a hostile server choosing login challenge bytes equal to a
/// header and collecting a real admin's signature over it.
enum class SignatureTag
{
    Auth,       ///< "auth"    - login challenge, the one message the server chooses
    Request,    ///< "request" - mutating request: document id and body hash
    Header,     ///< "header"  - DocumentHeader::signedBytes()
    Pairing     ///< "pairing" - the responder's handover to a device it just paired with
};

/// What a BLAKE2b digest is for.
///
/// Each value names a personalisation string mixed into BLAKE2b's parameter
/// block, so a digest made for one purpose can never equal one made for another
/// - whatever their inputs, and whatever their output lengths. Relying on the
/// output lengths differing would be true today and silently false the day
/// someone widens one of them.
///
/// These strings are part of the document format. Changing one invalidates every
/// fingerprint or hash of that kind.
enum class HashDomain
{
    Fingerprint,        ///< "wp-fingerprint" - a device's two public keys
    Payload,            ///< "wp-payload"     - the encrypted payload
    PairingCommit,      ///< "wp-pair-commit" - the initiator's keys and nonce
    PairingComparison,  ///< "wp-pair-number" - both devices' keys and the nonce
    Request             ///< "wp-request"     - a mutating request's canonical body
};

/// One device's sealing keys.
struct Keypair
{
    PublicKey publicKey;
    SecretKey secretKey;
};

/// One device's signing keys.
///
/// Kept independent of Keypair. libsodium offers
/// crypto_sign_ed25519_pk_to_curve25519, and the design declines it: two keys
/// cost nothing and keep the sealing key and the signing key each doing one job.
/// Do not add the conversion for convenience.
struct SigningKeypair
{
    VerifyKey verifyKey;
    SigningKey signingKey;
};

class DocumentCrypto
{
public:
    static constexpr int SignatureSize = 64;

    /// A data key sealed to one device: the 32-byte key plus an ephemeral public
    /// key and a MAC. Every recipient entry in a header carries exactly this many
    /// bytes, which is what the strict header parser checks.
    static constexpr int SealedDataKeySize = 80;

    /// XChaCha20-Poly1305 nonce. Fresh on every single write.
    static constexpr int NonceSize = 24;

    /// BLAKE2b over the ciphertext.
    static constexpr int PayloadHashSize = 32;

    /// Wraps sodium_init(). Must run before any other call here - libsodium's
    /// failure mode without it is platform-dependent, not a clean crash. Called
    /// once at startup by main(), and by the test runner.
    static bool initialise();

    static Keypair generateKeypair();
    static SigningKeypair generateSigningKeypair();
    static DataKey generateDataKey();

    /// Personalised BLAKE2b. Returns an empty array when outputSize is outside
    /// BLAKE2b's range. Every hash in this codebase goes through here, so no two
    /// purposes can share a digest.
    static QByteArray hash(HashDomain domain, const QByteArray& material, int outputSize);

    /// BLAKE2b over the X25519 public key followed by the Ed25519 verify key, so
    /// neither key of a device can change while its identity stays the same.
    /// Returns an invalid Fingerprint when either key is invalid.
    static Fingerprint fingerprintOf(const PublicKey& publicKey, const VerifyKey& verifyKey);

    /// Seals the data key to one device's X25519 public key. Returns an empty
    /// array when the recipient key or the data key is invalid.
    ///
    /// Two seals of the same key to the same recipient differ, because
    /// crypto_box_seal generates a fresh ephemeral sender keypair each time.
    static QByteArray sealDataKey(const DataKey& dataKey, const PublicKey& recipient);

    /// Returns nullopt when this keypair is not the one the entry was sealed to.
    /// That is the ordinary "not addressed to me" case a reader hits on every
    /// recipient but its own - not an error to report.
    static std::optional<DataKey> unsealDataKey(const QByteArray& sealed, const Keypair& keys);

    /// Returns an empty array when the key is invalid.
    static QByteArray sign(SignatureTag tag, const QByteArray& message, const SigningKey& key);

    static bool verify(SignatureTag tag,
                       const QByteArray& message,
                       const QByteArray& signature,
                       const VerifyKey& key);

    // --- Payload ---

    /// Called on every single save.
    ///
    /// Never derive this from the save counter and never carry one over from a
    /// loaded header. A repeated nonce under the same key XORs two versions of
    /// the ward directory against each other and exposes the Poly1305 one-time
    /// key - which is to say it hands over the document and the ability to forge
    /// one. The temptation is to make it deterministic for testing; resist it.
    static QByteArray generateNonce();

    /// Compresses, then encrypts under header.nonce with header.associatedData()
    /// as associated data. Returns an empty array on bad input.
    static QByteArray encryptPayload(const QByteArray& plaintext,
                                     const DataKey& key,
                                     const DocumentHeader& header);

    /// Returns nullopt when the ciphertext does not authenticate under this key
    /// and this header, or when what comes out does not decompress.
    static std::optional<QByteArray> decryptPayload(const QByteArray& sealed,
                                                    const DataKey& key,
                                                    const DocumentHeader& header);

    static QByteArray payloadHash(const QByteArray& ciphertext);

    // --- Header signing ---

    static QByteArray signHeader(const DocumentHeader& header, const SigningKey& key);

    /// Against the writer keys the header carries, which parse has already tied to
    /// lastWriter - so every caller checks a signature the same way, and a leave,
    /// whose writer has just left the roster, needs no special case.
    static bool verifyHeader(const DocumentHeader& header);

    /// Who the writer is, which verifyHeader deliberately does not answer: a device in
    /// this version's own roster, with the keys the header carries for it - or, when
    /// the header no longer lists its writer, a leave. That a leaver was a member is
    /// proven by the chain, or, with no base copy, by the payload decrypting at all.
    static bool writerIsAccountedFor(const DocumentHeader& header, const AccessRoster& roster);

    // --- The membership chain ---

    /// Rule 4 within one header: every entry names a real device whose
    /// fingerprint matches its keys, each device appears once and under one
    /// person, no two devices share a key, the admin list is canonical and names
    /// only people in the header - at least one of them when anyone is listed -
    /// and the fields a finished write must carry are present. verifyLink checks
    /// it on both sides; the server also checks it on a newly registered
    /// document's first header, which has no previous header to link from.
    static bool isCoherent(const DocumentHeader& header);

    /// Whether one header may follow another. The single statement of the rule:
    /// verifyChain applies it pairwise, and the server applies it to every
    /// upload.
    static bool verifyLink(const DocumentHeader& previous, const DocumentHeader& next);

    /// Walks headers forward from an already-authenticated anchor - the base
    /// copy's header, never the local file's, whose offline saves are not in the
    /// server's chain.
    ///
    /// This is what a removed device relies on: it can no longer decrypt, so the
    /// headers are all the evidence it has.
    static ChainResult verifyChain(const DocumentHeader& anchor,
                                   const QList<DocumentHeader>& later,
                                   const Fingerprint& self);
};
