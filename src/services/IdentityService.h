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
/// The keys exist from the first run; the person from the moment it is created on
/// the first screen.
class IdentityService
{
public:
    /// The identity at path. A first run generates both keypairs and stores them
    /// at once. nullptr when the file is a newer version's, is damaged, or the first
    /// identity cannot be stored; outcome says which, and errorMessage why. A file
    /// that is there is never replaced unasked.
    static std::unique_ptr<IdentityService> open(const QString& path,
                                                 IdentityFile::Load& outcome,
                                                 QString* errorMessage);

    /// A fresh identity in place of the one at path, which is moved aside rather
    /// than deleted. Asked for by the person: a damaged file, or a device that was
    /// set up as the wrong person before it joined any ward. nullptr when the old
    /// file cannot be moved or the new one stored.
    static std::unique_ptr<IdentityService> startOver(const QString& path, QString* errorMessage);

    /// Empty until a person exists.
    std::optional<DeviceIdentity> identity() const;

    /// The machine's host name, offered for editing on the first screen.
    static QString defaultDeviceName();

    /// A new person, under a fresh member id, with the names a person typed. Refused
    /// once a person exists, and for an empty name.
    bool createPerson(const QString& displayName, const QString& deviceName, QString* errorMessage);

private:
    IdentityService(const QString& path, const IdentityRecord& record);

    bool setPerson(const IdentityRecord::Person& person, QString* errorMessage);

    QString m_path;
    IdentityRecord m_record;
};
