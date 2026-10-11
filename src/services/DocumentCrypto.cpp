#include "DocumentCrypto.h"

#include "DocumentHeader.h"

#include <sodium.h>

static_assert(DocumentCrypto::SignatureSize == crypto_sign_BYTES);
static_assert(DocumentCrypto::SealedDataKeySize == DataKey::Size + crypto_box_SEALBYTES);
static_assert(DocumentCrypto::NonceSize == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(DocumentCrypto::SigningSeedSize == crypto_sign_SEEDBYTES);
static_assert(DocumentCrypto::PayloadHashSize >= crypto_generichash_blake2b_BYTES_MIN);
static_assert(DocumentCrypto::PayloadHashSize <= crypto_generichash_blake2b_BYTES_MAX);

namespace
{

/// The personalisation string for each domain, zero-padded to
/// crypto_generichash_blake2b_PERSONALBYTES.
///
/// The static_asserts are what keep them short enough. A runtime check would
/// compile out of a release build, which is exactly where a silently truncated
/// personalisation would do its damage.
constexpr char FingerprintDomain[] = "wp-fingerprint";
constexpr char PayloadDomain[] = "wp-payload";

static_assert(sizeof(FingerprintDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);
static_assert(sizeof(PayloadDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);

const char* domainText(HashDomain domain)
{
    switch (domain)
    {
    case HashDomain::Fingerprint:
        return FingerprintDomain;
    case HashDomain::Payload:
        return PayloadDomain;
    }

    // Unreachable for a value of the enumeration. A new domain added without a
    // case here must not quietly share another's parameter block.
    Q_ASSERT(false);
    return nullptr;
}

const unsigned char* asUnsigned(const QByteArray& bytes)
{
    return reinterpret_cast<const unsigned char*>(bytes.constData());
}

unsigned char* asUnsigned(QByteArray& bytes)
{
    return reinterpret_cast<unsigned char*>(bytes.data());
}

} // namespace

bool DocumentCrypto::initialise()
{
    // 0 on success, 1 when another caller already initialised it, -1 on failure.
    return sodium_init() >= 0;
}

Keypair DocumentCrypto::generateKeypair()
{
    QByteArray publicBytes(PublicKey::Size, '\0');
    QByteArray secretBytes(SecretKey::Size, '\0');

    crypto_box_keypair(asUnsigned(publicBytes), asUnsigned(secretBytes));

    Keypair keys;
    keys.publicKey.bytes = publicBytes;
    keys.secretKey.bytes = secretBytes;
    return keys;
}

SigningKeypair DocumentCrypto::generateSigningKeypair()
{
    QByteArray verifyBytes(VerifyKey::Size, '\0');
    QByteArray signingBytes(SigningKey::Size, '\0');

    crypto_sign_keypair(asUnsigned(verifyBytes), asUnsigned(signingBytes));

    SigningKeypair keys;
    keys.verifyKey.bytes = verifyBytes;
    keys.signingKey.bytes = signingBytes;
    return keys;
}

Keypair DocumentCrypto::keypairFromSecret(const SecretKey& secretKey)
{
    Keypair keys;
    if (!secretKey.isValid())
    {
        return keys;
    }

    QByteArray publicBytes(PublicKey::Size, '\0');
    crypto_scalarmult_base(asUnsigned(publicBytes), asUnsigned(secretKey.bytes));
    keys.publicKey.bytes = publicBytes;
    keys.secretKey = secretKey;
    return keys;
}

QByteArray DocumentCrypto::seedOf(const SigningKey& signingKey)
{
    if (!signingKey.isValid())
    {
        return QByteArray();
    }

    QByteArray seed(SigningSeedSize, '\0');
    crypto_sign_ed25519_sk_to_seed(asUnsigned(seed), asUnsigned(signingKey.bytes));
    return seed;
}

SigningKeypair DocumentCrypto::signingKeypairFromSeed(const QByteArray& seed)
{
    SigningKeypair keys;
    if (seed.size() != SigningSeedSize)
    {
        return keys;
    }

    QByteArray verifyBytes(VerifyKey::Size, '\0');
    QByteArray signingBytes(SigningKey::Size, '\0');
    crypto_sign_seed_keypair(asUnsigned(verifyBytes), asUnsigned(signingBytes), asUnsigned(seed));
    keys.verifyKey.bytes = verifyBytes;
    keys.signingKey.bytes = signingBytes;
    return keys;
}

QByteArray DocumentCrypto::randomBytes(int size)
{
    QByteArray bytes(size, '\0');
    randombytes_buf(asUnsigned(bytes), bytes.size());
    return bytes;
}

DataKey DocumentCrypto::generateDataKey()
{
    DataKey key;
    key.bytes = randomBytes(DataKey::Size);
    return key;
}

QByteArray DocumentCrypto::hash(HashDomain domain, const QByteArray& material, int outputSize)
{
    if (outputSize < crypto_generichash_blake2b_BYTES_MIN
        || outputSize > crypto_generichash_blake2b_BYTES_MAX)
    {
        return QByteArray();
    }

    const char* text = domainText(domain);
    if (text == nullptr)
    {
        return QByteArray();
    }

    QByteArray personal(crypto_generichash_blake2b_PERSONALBYTES, '\0');
    const QByteArray domainBytes(text);
    personal.replace(0, domainBytes.size(), domainBytes);

    // The salt is a second parameter-block field this design has no use for.
    // Passing an explicit zero-filled salt says so, and behaves the same as the
    // null libsodium also accepts.
    const QByteArray salt(crypto_generichash_blake2b_SALTBYTES, '\0');

    QByteArray digest(outputSize, '\0');
    if (crypto_generichash_blake2b_salt_personal(asUnsigned(digest),
                                                 digest.size(),
                                                 asUnsigned(material),
                                                 material.size(),
                                                 nullptr,
                                                 0,
                                                 asUnsigned(salt),
                                                 asUnsigned(personal))
        != 0)
    {
        return QByteArray();
    }

    return digest;
}

Fingerprint DocumentCrypto::fingerprintOf(const PublicKey& publicKey, const VerifyKey& verifyKey)
{
    Fingerprint fingerprint;
    if (!publicKey.isValid() || !verifyKey.isValid())
    {
        return fingerprint;
    }

    fingerprint.bytes = hash(HashDomain::Fingerprint,
                             publicKey.bytes + verifyKey.bytes,
                             Fingerprint::Size);
    return fingerprint;
}

QByteArray DocumentCrypto::sealDataKey(const DataKey& dataKey, const PublicKey& recipient)
{
    if (!dataKey.isValid() || !recipient.isValid())
    {
        return QByteArray();
    }

    // A sealed box is XSalsa20-Poly1305 internally, not the XChaCha20-Poly1305
    // the payload uses. That difference is immaterial for 32 bytes of key
    // material, and it is what crypto_box_seal provides - do not "fix" it.
    QByteArray sealed(SealedDataKeySize, '\0');
    if (crypto_box_seal(asUnsigned(sealed),
                        asUnsigned(dataKey.bytes),
                        dataKey.bytes.size(),
                        asUnsigned(recipient.bytes))
        != 0)
    {
        return QByteArray();
    }

    return sealed;
}

std::optional<DataKey> DocumentCrypto::unsealDataKey(const QByteArray& sealed, const Keypair& keys)
{
    if (sealed.size() != SealedDataKeySize || !keys.publicKey.isValid() || !keys.secretKey.isValid())
    {
        return std::nullopt;
    }

    QByteArray opened(DataKey::Size, '\0');
    if (crypto_box_seal_open(asUnsigned(opened),
                             asUnsigned(sealed),
                             sealed.size(),
                             asUnsigned(keys.publicKey.bytes),
                             asUnsigned(keys.secretKey.bytes))
        != 0)
    {
        return std::nullopt;
    }

    DataKey key;
    key.bytes = opened;
    return key;
}

QByteArray DocumentCrypto::signChallenge(const QByteArray& challenge, const SigningKey& key)
{
    if (!key.isValid() || challenge.isEmpty())
    {
        return QByteArray();
    }

    QByteArray signature(SignatureSize, '\0');
    if (crypto_sign_detached(asUnsigned(signature),
                             nullptr,
                             asUnsigned(challenge),
                             challenge.size(),
                             asUnsigned(key.bytes))
        != 0)
    {
        return QByteArray();
    }

    return signature;
}

bool DocumentCrypto::challengeVerifies(const QByteArray& challenge,
                                       const QByteArray& signature,
                                       const VerifyKey& key)
{
    if (!key.isValid() || challenge.isEmpty() || signature.size() != SignatureSize)
    {
        return false;
    }

    return crypto_sign_verify_detached(asUnsigned(signature),
                                       asUnsigned(challenge),
                                       challenge.size(),
                                       asUnsigned(key.bytes))
        == 0;
}

// ---------------------------------------------------------------------------
// Payload
// ---------------------------------------------------------------------------

QByteArray DocumentCrypto::generateNonce()
{
    QByteArray nonce(NonceSize, '\0');
    randombytes_buf(asUnsigned(nonce), nonce.size());
    return nonce;
}

QByteArray DocumentCrypto::encryptPayload(const QByteArray& plaintext,
                                          const DataKey& key,
                                          const DocumentHeader& header)
{
    // decryptPayload cannot tell an empty document from a decompression failure,
    // so an empty one is refused here rather than written and then unreadable.
    if (!key.isValid() || header.nonce.size() != NonceSize || plaintext.isEmpty())
    {
        return QByteArray();
    }

    const QByteArray compressed = qCompress(plaintext);
    const QByteArray associated = header.associatedData();

    QByteArray sealed(compressed.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
    unsigned long long sealedLength = 0;

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(asUnsigned(sealed),
                                                   &sealedLength,
                                                   asUnsigned(compressed),
                                                   compressed.size(),
                                                   asUnsigned(associated),
                                                   associated.size(),
                                                   nullptr,
                                                   asUnsigned(header.nonce),
                                                   asUnsigned(key.bytes))
        != 0)
    {
        return QByteArray();
    }

    sealed.resize(qsizetype(sealedLength));
    return sealed;
}

std::optional<QByteArray> DocumentCrypto::decryptPayload(const QByteArray& sealed,
                                                         const DataKey& key,
                                                         const DocumentHeader& header)
{
    if (!key.isValid() || header.nonce.size() != NonceSize
        || sealed.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES)
    {
        return std::nullopt;
    }

    const QByteArray associated = header.associatedData();

    QByteArray compressed(sealed.size(), '\0');
    unsigned long long compressedLength = 0;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(asUnsigned(compressed),
                                                   &compressedLength,
                                                   nullptr,
                                                   asUnsigned(sealed),
                                                   sealed.size(),
                                                   asUnsigned(associated),
                                                   associated.size(),
                                                   asUnsigned(header.nonce),
                                                   asUnsigned(key.bytes))
        != 0)
    {
        return std::nullopt;
    }

    compressed.resize(qsizetype(compressedLength));

    const QByteArray plaintext = qUncompress(compressed);
    if (plaintext.isEmpty())
    {
        // qUncompress reports every failure as an empty result, so an empty
        // result is a failure. A document is never empty.
        return std::nullopt;
    }

    return plaintext;
}

QByteArray DocumentCrypto::payloadHash(const QByteArray& ciphertext)
{
    return hash(HashDomain::Payload, ciphertext, PayloadHashSize);
}
