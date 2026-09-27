#pragma once

#include "CryptoTypes.h"

#include <QJsonObject>
#include <QString>

#include <optional>

/// One installation that may open a document: its name, and its two public keys.
///
/// The private halves never leave the machine that generated them, so a roster
/// entry is safe to write into the payload where every member can read it.
class RosterDevice
{
public:
    RosterDevice() = default;

    static RosterDevice create(const QString& deviceName,
                               const PublicKey& publicKey,
                               const VerifyKey& verifyKey);

    const QString& deviceName() const { return m_deviceName; }
    const PublicKey& publicKey() const { return m_publicKey; }
    const VerifyKey& verifyKey() const { return m_verifyKey; }

    void setDeviceName(const QString& deviceName) { m_deviceName = deviceName; }

    /// Derived from both public keys, so neither can change while this device's
    /// identity stays the same. A header entry carries only this, which is why
    /// revocation needs the roster: you cannot seal to a fingerprint.
    ///
    /// Computed once when the device is made or read, so lookups, sorting and
    /// equality never hash - and a model's equality never calls into libsodium.
    const Fingerprint& fingerprint() const { return m_fingerprint; }

    /// Exactly what fromJson requires, so anything valid here reads back from
    /// the payload. The name is part of it: it is what lets a person unlink the
    /// right device.
    bool isValid() const;

    QJsonObject toJson() const;

    /// Returns nullopt on any missing or ill-formed field. Nothing is defaulted:
    /// a device this build cannot read in full is not a device it should act on.
    static std::optional<RosterDevice> fromJson(const QJsonObject& json);

    bool operator==(const RosterDevice& other) const;
    bool operator!=(const RosterDevice& other) const { return !(*this == other); }

private:
    QString m_deviceName;
    PublicKey m_publicKey;
    VerifyKey m_verifyKey;
    Fingerprint m_fingerprint;
};
