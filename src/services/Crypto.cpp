#include "Crypto.h"

#include <sodium.h>

static_assert(static_cast<int>(crypto_box_PUBLICKEYBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_box_SECRETKEYBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_sign_PUBLICKEYBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_sign_SEEDBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_aead_xchacha20poly1305_ietf_KEYBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) == Crypto::NonceBytes);
static_assert(static_cast<int>(crypto_box_SEALBYTES) + Crypto::KeyBytes == Crypto::SealedKeyBytes);
static_assert(static_cast<int>(crypto_generichash_blake2b_PERSONALBYTES) == 16);

namespace
{

unsigned char* bytesOf(QByteArray& bytes)
{
    return reinterpret_cast<unsigned char*>(bytes.data());
}

const unsigned char* bytesOf(const QByteArray& bytes)
{
    return reinterpret_cast<const unsigned char*>(bytes.constData());
}

}  // namespace

bool Crypto::initialize()
{
    return sodium_init() >= 0;
}

QByteArray Crypto::randomBytes(int count)
{
    QByteArray bytes(count, '\0');
    randombytes_buf(bytesOf(bytes), static_cast<size_t>(bytes.size()));
    return bytes;
}

QByteArray Crypto::hash(const QByteArray& input, int length, const QByteArray& personalization)
{
    Q_ASSERT(personalization.size() <= static_cast<qsizetype>(crypto_generichash_blake2b_PERSONALBYTES));
    QByteArray personal = personalization.leftJustified(crypto_generichash_blake2b_PERSONALBYTES, '\0', true);
    QByteArray output(length, '\0');
    crypto_generichash_blake2b_salt_personal(bytesOf(output), static_cast<size_t>(output.size()),
                                             bytesOf(input), static_cast<unsigned long long>(input.size()),
                                             nullptr, 0,
                                             nullptr,
                                             bytesOf(personal));
    return output;
}

QByteArray Crypto::seal(const QByteArray& message, const QByteArray& publicKey)
{
    QByteArray sealed(message.size() + crypto_box_SEALBYTES, '\0');
    int result = crypto_box_seal(bytesOf(sealed), bytesOf(message),
                                 static_cast<unsigned long long>(message.size()), bytesOf(publicKey));
    Q_ASSERT(result == 0);
    Q_UNUSED(result);
    return sealed;
}

std::optional<QByteArray> Crypto::unseal(const QByteArray& sealed, const QByteArray& publicKey,
                                         const QByteArray& secretKey)
{
    if (sealed.size() < static_cast<qsizetype>(crypto_box_SEALBYTES))
    {
        return std::nullopt;
    }
    QByteArray message(sealed.size() - crypto_box_SEALBYTES, '\0');
    if (crypto_box_seal_open(bytesOf(message), bytesOf(sealed), static_cast<unsigned long long>(sealed.size()),
                             bytesOf(publicKey), bytesOf(secretKey)) != 0)
    {
        return std::nullopt;
    }
    return message;
}

QByteArray Crypto::encrypt(const QByteArray& plaintext, const QByteArray& associatedData,
                           const QByteArray& nonce, const QByteArray& key)
{
    QByteArray ciphertext(plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
    unsigned long long length = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(bytesOf(ciphertext), &length,
                                               bytesOf(plaintext), static_cast<unsigned long long>(plaintext.size()),
                                               bytesOf(associatedData), static_cast<unsigned long long>(associatedData.size()),
                                               nullptr, bytesOf(nonce), bytesOf(key));
    Q_ASSERT(length == static_cast<unsigned long long>(ciphertext.size()));
    return ciphertext;
}

std::optional<QByteArray> Crypto::decrypt(const QByteArray& ciphertext, const QByteArray& associatedData,
                                          const QByteArray& nonce, const QByteArray& key)
{
    if (ciphertext.size() < static_cast<qsizetype>(crypto_aead_xchacha20poly1305_ietf_ABYTES))
    {
        return std::nullopt;
    }
    QByteArray plaintext(ciphertext.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
    unsigned long long length = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(bytesOf(plaintext), &length, nullptr,
                                                   bytesOf(ciphertext), static_cast<unsigned long long>(ciphertext.size()),
                                                   bytesOf(associatedData), static_cast<unsigned long long>(associatedData.size()),
                                                   bytesOf(nonce), bytesOf(key)) != 0)
    {
        return std::nullopt;
    }
    return plaintext;
}

QByteArray Crypto::sealingPublicKey(const QByteArray& secretKey)
{
    QByteArray publicKey(crypto_box_PUBLICKEYBYTES, '\0');
    crypto_scalarmult_base(bytesOf(publicKey), bytesOf(secretKey));
    return publicKey;
}

QByteArray Crypto::verifyKey(const QByteArray& seed)
{
    QByteArray verifyKey(crypto_sign_PUBLICKEYBYTES, '\0');
    QByteArray signingKey(crypto_sign_SECRETKEYBYTES, '\0');
    crypto_sign_seed_keypair(bytesOf(verifyKey), bytesOf(signingKey), bytesOf(seed));
    return verifyKey;
}
