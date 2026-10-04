#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"

#include <QByteArray>
#include <QString>

#include <optional>

/// This device's secret identity: both keypairs, and the person they belong to
/// once there is one.
struct IdentityRecord
{
    Keypair sealing;
    SigningKeypair signing;
    std::optional<MemberId> member;

    /// Only the secret halves are written - the sealing secret key and the signing
    /// seed - and the public ones are derived on reading, so a record can never
    /// pair a secret with someone else's public key.
    QByteArray toBytes() const;

    /// The fields toBytes writes and no others, each secret the right size;
    /// nullopt otherwise.
    static std::optional<IdentityRecord> fromBytes(const QByteArray& bytes);

    bool operator==(const IdentityRecord& other) const;
    bool operator!=(const IdentityRecord& other) const { return !(*this == other); }
};

/// The identity as a file in this device's local app data, readable only by its
/// user - the way an SSH key sits in ~/.ssh. Local app data does not roam, so the
/// private keys stay on this machine.
namespace IdentityFile
{

enum class Load
{
    Loaded,
    NotFound,   ///< no file: a first run
    Failed      ///< a file that cannot be read or understood; never taken for a first run
};

/// Keeping NotFound and Failed apart is what stops a damaged file from looking
/// like a first run and minting a second identity.
Load load(const QString& path, IdentityRecord& out, QString* errorMessage);

bool store(const QString& path, const IdentityRecord& record, QString* errorMessage);

} // namespace IdentityFile
