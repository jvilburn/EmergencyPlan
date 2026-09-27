#pragma once

#include <QByteArray>
#include <QString>

// Fixed-size byte wrappers, so the compiler catches a public key passed where a
// data key belongs.
//
// <sodium.h> is deliberately absent: WardPlanningLib exports its include
// directories PUBLIC, so including it here would propagate libsodium into every
// widget translation unit. The sizes are written as literals and checked against
// libsodium's own constants by static_assert in CryptoTypes.cpp.

/// X25519 public key: seals the data key to one device.
struct PublicKey
{
    static constexpr int Size = 32;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const PublicKey& other) const { return bytes == other.bytes; }
    bool operator!=(const PublicKey& other) const { return !(*this == other); }
};

/// X25519 secret key. Never leaves the device that generated it.
struct SecretKey
{
    static constexpr int Size = 32;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const SecretKey& other) const { return bytes == other.bytes; }
    bool operator!=(const SecretKey& other) const { return !(*this == other); }
};

/// Ed25519 verify key: checks header, removal, pairing and auth signatures.
struct VerifyKey
{
    static constexpr int Size = 32;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const VerifyKey& other) const { return bytes == other.bytes; }
    bool operator!=(const VerifyKey& other) const { return !(*this == other); }
};

/// Ed25519 signing key. Never leaves the device that generated it.
///
/// Size is 64, not 32: libsodium's Ed25519 secret key carries the seed and the
/// public key together. Writing 32 here compiles and then fails at run time
/// inside crypto_sign_detached.
struct SigningKey
{
    static constexpr int Size = 64;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const SigningKey& other) const { return bytes == other.bytes; }
    bool operator!=(const SigningKey& other) const { return !(*this == other); }
};

/// The symmetric key encrypting one document's payload.
struct DataKey
{
    static constexpr int Size = 32;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const DataKey& other) const { return bytes == other.bytes; }
    bool operator!=(const DataKey& other) const { return !(*this == other); }
};

/// Identifies one device by both of its public keys at once.
///
/// Taken over the X25519 public key followed by the Ed25519 verify key, so
/// neither key of a device can change while its identity stays the same.
/// 16 bytes is 128 bits, the design's floor, and a legal BLAKE2b output length.
struct Fingerprint
{
    static constexpr int Size = 16;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const Fingerprint& other) const { return bytes == other.bytes; }
    bool operator!=(const Fingerprint& other) const { return !(*this == other); }

    QString toHex() const;

    /// For reading aloud when two people compare fingerprints: four-character
    /// groups separated by '-'.
    QString toGroupedHex() const;

    /// Returns an invalid Fingerprint when the text is not exactly Size bytes of
    /// hex. Accepts the grouped form as well as plain hex.
    static Fingerprint fromHex(const QString& hex);
};

/// One device's two public keys, as they travel together through pairing and the
/// server. The private halves never leave the device.
struct DeviceKeys
{
    PublicKey publicKey;
    VerifyKey verifyKey;

    bool isValid() const { return publicKey.isValid() && verifyKey.isValid(); }
    bool operator==(const DeviceKeys& other) const { return publicKey == other.publicKey && verifyKey == other.verifyKey; }
    bool operator!=(const DeviceKeys& other) const { return !(*this == other); }
};

/// Identifies a person on the access roster: 128 random bits, never reused.
///
/// Not the directory's PersonId (src/models/Id.h): a MemberId identifies a
/// person who may open the document, a PersonId a member of the ward directory.
struct MemberId
{
    static constexpr int Size = 16;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const MemberId& other) const { return bytes == other.bytes; }
    bool operator!=(const MemberId& other) const { return !(*this == other); }

    QString toHex() const;

    /// Returns an invalid MemberId when the text is not exactly Size bytes of hex.
    static MemberId fromHex(const QString& hex);
};
