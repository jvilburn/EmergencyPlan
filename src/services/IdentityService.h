#pragma once

#include "CryptoTypes.h"
#include "DeviceIdentity.h"
#include "IdentityFile.h"

#include <QString>

#include <memory>
#include <optional>

/// This device's keypairs, and the person it belongs to - member id, display name,
/// and device name - resolved before any document exists. Its product is the
/// DeviceIdentity value the core takes.
///
/// The keys exist from the first run; the person from the moment it is created
/// here or joined through linking. The names are fixed from then on: they reach
/// other members only through the roster, and a rename would be a roster change no
/// chain rule carries.
class IdentityService
{
public:
    /// The identity at path. A first run generates both keypairs and stores them
    /// at once. nullptr with errorMessage set when the file is there and cannot be
    /// read, or the first identity cannot be stored: a damaged file is never
    /// replaced by a new identity unasked.
    static std::unique_ptr<IdentityService> open(const QString& path, QString* errorMessage);

    /// Empty until a person exists.
    std::optional<DeviceIdentity> identity() const;

    bool hasPerson() const { return m_record.person.has_value(); }

    /// This device's public keys, which pairing presents before a person exists.
    DeviceKeys deviceKeys() const;

    /// The machine's host name, offered for editing on the first screen.
    static QString defaultDeviceName();

    /// A new person, under a fresh member id. Refused once a person exists, and
    /// for an empty name.
    bool createPerson(const QString& displayName, const QString& deviceName, QString* errorMessage);

    /// An existing person, by the member id and display name a linked device handed
    /// over. Refused once a person exists, and for an empty name or invalid id.
    bool joinPerson(const MemberId& member,
                    const QString& displayName,
                    const QString& deviceName,
                    QString* errorMessage);

private:
    IdentityService(const QString& path, const IdentityRecord& record);

    bool setPerson(const IdentityRecord::Person& person, QString* errorMessage);

    QString m_path;
    IdentityRecord m_record;
};
