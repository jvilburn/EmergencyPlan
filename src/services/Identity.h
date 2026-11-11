#pragma once

#include <QByteArray>
#include <optional>

/// The two keypairs a device acts under: an X25519 keypair the data key is sealed to, and an
/// Ed25519 keypair that signs. Both derive from two 32-byte secrets, which are all the key
/// file holds.
class Identity
{
public:
    /// A new identity from fresh random secrets.
    static Identity generate();

    static Identity fromSecrets(const QByteArray& sealingSecret, const QByteArray& signingSeed);

    /// An invite's identity: subkeys 1 and 2 of seed under the context wpinvite seed its
    /// X25519 keypair, as crypto_box_seed_keypair makes it, and its Ed25519 keypair.
    static Identity fromInviteSeed(const QByteArray& seed);

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

    /// This device's Ed25519 signature of message, which verifyKey verifies.
    QByteArray sign(const QByteArray& message) const;

private:
    Identity(const QByteArray& sealingSecret, const QByteArray& signingSeed);

    QByteArray m_sealingSecret;
    QByteArray m_signingSeed;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
    QByteArray m_fingerprint;
};
