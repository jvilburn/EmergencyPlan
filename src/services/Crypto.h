#pragma once

#include <QByteArray>
#include <optional>

/// The libsodium calls the format makes, over QByteArray.
class Crypto
{
public:
    static constexpr int KeyBytes = 32;          ///< a data key, a public, secret or verify key, a seed
    static constexpr int NonceBytes = 24;        ///< an XChaCha20-Poly1305 nonce
    static constexpr int SealedKeyBytes = 80;    ///< a data key sealed with crypto_box_seal
    static constexpr int FingerprintBytes = 16;
    static constexpr int IdBytes = 16;           ///< a document id or a user id

    /// Initializes libsodium. Nothing else here may run until it has answered true.
    static bool initialize();

    /// count bytes from randombytes_buf.
    static QByteArray randomBytes(int count);

    /// Unkeyed BLAKE2b of input, length bytes long, with a zero salt and personalization
    /// zero-padded to 16 bytes; an empty personalization is all zeros.
    static QByteArray hash(const QByteArray& input, int length, const QByteArray& personalization);

    /// message sealed to an X25519 public key with crypto_box_seal.
    static QByteArray seal(const QByteArray& message, const QByteArray& publicKey);

    /// What sealed holds, or none when it does not open with this keypair.
    static std::optional<QByteArray> unseal(const QByteArray& sealed, const QByteArray& publicKey,
                                            const QByteArray& secretKey);

    /// plaintext encrypted with XChaCha20-Poly1305 under key and nonce, authenticating
    /// associatedData: the ciphertext followed by its 16-byte tag.
    static QByteArray encrypt(const QByteArray& plaintext, const QByteArray& associatedData,
                              const QByteArray& nonce, const QByteArray& key);

    /// The plaintext, or none when the tag does not verify for this key, nonce and
    /// associatedData.
    static std::optional<QByteArray> decrypt(const QByteArray& ciphertext, const QByteArray& associatedData,
                                             const QByteArray& nonce, const QByteArray& key);

    /// The X25519 public key of a secret key: crypto_scalarmult_base.
    static QByteArray sealingPublicKey(const QByteArray& secretKey);

    /// The Ed25519 verify key of a seed: the public half of crypto_sign_seed_keypair.
    static QByteArray verifyKey(const QByteArray& seed);
};
