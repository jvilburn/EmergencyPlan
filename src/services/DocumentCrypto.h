#pragma once

#include "CryptoTypes.h"

#include <QByteArray>

#include <optional>

struct DocumentHeader;

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
    Payload             ///< "wp-payload"     - the encrypted payload, for the base copy's record
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

    /// The seed an Ed25519 signing keypair is made from.
    static constexpr int SigningSeedSize = 32;

    /// Wraps sodium_init(). Must run before any other call here - libsodium's
    /// failure mode without it is platform-dependent, not a clean crash. Called
    /// once at startup by main(), and by the test runner.
    static bool initialise();

    static Keypair generateKeypair();
    static SigningKeypair generateSigningKeypair();

    /// The sealing keypair a secret key belongs to. Invalid keys for an invalid secret.
    static Keypair keypairFromSecret(const SecretKey& secretKey);

    /// The seed a signing key was made from; empty for an invalid key.
    static QByteArray seedOf(const SigningKey& signingKey);

    /// The signing keypair a seed makes. Invalid keys for a seed of the wrong size.
    static SigningKeypair signingKeypairFromSeed(const QByteArray& seed);

    static DataKey generateDataKey();

    /// Cryptographically random bytes, for ids no one may guess or repeat.
    static QByteArray randomBytes(int size);

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

    /// The one thing a device signs: the server's login challenge. Empty when the
    /// key is invalid or the challenge empty.
    static QByteArray signChallenge(const QByteArray& challenge, const SigningKey& key);

    static bool challengeVerifies(const QByteArray& challenge,
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
};
