#include "Identity.h"
#include "Crypto.h"

Identity Identity::generate()
{
    return fromSecrets(Crypto::randomBytes(Crypto::KeyBytes), Crypto::randomBytes(Crypto::KeyBytes));
}

Identity Identity::fromSecrets(const QByteArray& sealingSecret, const QByteArray& signingSeed)
{
    Identity identity;
    identity.m_sealingSecret = sealingSecret;
    identity.m_signingSeed = signingSeed;
    identity.m_publicKey = Crypto::sealingPublicKey(sealingSecret);
    identity.m_verifyKey = Crypto::verifyKey(signingSeed);
    identity.m_fingerprint = fingerprintOf(identity.m_publicKey, identity.m_verifyKey);
    return identity;
}

QByteArray Identity::fingerprintOf(const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return Crypto::hash(publicKey + verifyKey, Crypto::FingerprintBytes, "wp-fingerprint");
}

std::optional<QByteArray> Identity::unseal(const QByteArray& sealedKey) const
{
    return Crypto::unseal(sealedKey, m_publicKey, m_sealingSecret);
}
