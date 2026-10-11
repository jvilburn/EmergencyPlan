#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"

#include <QByteArray>
#include <QString>

#include <optional>

/// This device's identity: both keypairs, and the person they belong to once there
/// is one. One record, so the keys, the person, and the names fixed with them can
/// never get out of step.
struct IdentityRecord
{
    /// Who this device belongs to, and what it is called. Set once, when the person
    /// is created or joined; the names reach other members through the server.
    struct Person
    {
        MemberId member;
        QString displayName;
        QString deviceName;

        bool operator==(const Person& other) const;
        bool operator!=(const Person& other) const { return !(*this == other); }
    };

    /// The format this build writes and reads. A file of a later version is a newer
    /// app's, never a damaged one.
    static constexpr int CurrentVersion = 1;

    Keypair sealing;
    SigningKeypair signing;
    std::optional<Person> person;

    /// Only the secret halves are written - the sealing secret key and the signing
    /// seed - and the public ones are derived on reading, so a record can never
    /// pair a secret with someone else's public key.
    QByteArray toBytes() const;

    /// The fields toBytes writes and no others, at this build's version, each secret
    /// the right size; nullopt otherwise.
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
    NotFound,           ///< no file: a first run
    NeedsNewerVersion,  ///< written by a later version of the app; left alone
    Failed              ///< damaged or unreadable; never taken for a first run
};

/// Keeping NotFound apart from the rest is what stops a damaged file from looking
/// like a first run and minting a second identity unasked.
Load load(const QString& path, IdentityRecord& out, QString* errorMessage);

bool store(const QString& path, const IdentityRecord& record, QString* errorMessage);

/// Renames the file beside itself with the time in its name, so it can be restored
/// by hand: a ward never shared is sealed only to the keys in it. The new path, or
/// empty when it could not be moved.
QString moveAside(const QString& path, QString* errorMessage);

} // namespace IdentityFile
