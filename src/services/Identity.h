#pragma once

#include <QByteArray>
#include <optional>

/// The two keypairs a device acts under: an X25519 keypair the data key is sealed to, and an
/// Ed25519 keypair that signs. Both derive from two 32-byte secrets, which are all the key
/// file holds.
class Identity
{
public:
    Identity() = default;

    /// A new identity from fresh random secrets.
    static Identity generate();

    static Identity fromSecrets(const QByteArray& sealingSecret, const QByteArray& signingSeed);

    /// 16-byte BLAKE2b over the X25519 public key followed by the Ed25519 verify key,
    /// personalized wp-fingerprint. A changed key is a different device.
    static QByteArray fingerprintOf(const QByteArray& publicKey, const QByteArray& verifyKey);

    const QByteArray& sealingSecret() const { return m_sealingSecret; }
    const QByteArray& signingSeed() const { return m_signingSeed; }
    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }
    const QByteArray& fingerprint() const { return m_fingerprint; }

    /// The data key sealedKey holds, or none when it was not sealed to this identity.
    std::optional<QByteArray> unseal(const QByteArray& sealedKey) const;

private:
    QByteArray m_sealingSecret;
    QByteArray m_signingSeed;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
    QByteArray m_fingerprint;
};
