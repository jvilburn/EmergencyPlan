#include "Identity.h"
#include "Crypto.h"

Identity Identity::generate()
{
    return fromSecrets(Crypto::randomBytes(Crypto::KeyBytes), Crypto::randomBytes(Crypto::KeyBytes));
}

Identity::Identity(const QByteArray& sealingSecret, const QByteArray& signingSeed)
    : m_sealingSecret(sealingSecret)
    , m_signingSeed(signingSeed)
    , m_publicKey(Crypto::sealingPublicKey(sealingSecret))
    , m_verifyKey(Crypto::verifyKey(signingSeed))
    , m_fingerprint(fingerprintOf(m_publicKey, m_verifyKey))
{
}

Identity Identity::fromSecrets(const QByteArray& sealingSecret, const QByteArray& signingSeed)
{
    return Identity(sealingSecret, signingSeed);
}

QByteArray Identity::fingerprintOf(const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return Crypto::hash(publicKey + verifyKey, Crypto::FingerprintBytes, "wp-fingerprint");
}

std::optional<QByteArray> Identity::unseal(const QByteArray& sealedKey) const
{
    return Crypto::unseal(sealedKey, m_publicKey, m_sealingSecret);
}
