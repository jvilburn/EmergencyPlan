#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>

/// Stake-level metadata from the stake lookup API.
/// Contains references to ward unit numbers (not Ward objects).
/// Ward objects are stored separately in Document::m_wards.
class Stake
{
public:
    Stake() = default;

    // ========================================================================
    // Getters
    // ========================================================================
    const QString& id() const { return m_unitNumber; }  // For JSON serialization template
    const QString& unitNumber() const { return m_unitNumber; }
    const QString& name() const { return m_name; }

    // Ward unit numbers in this stake (from stake API)
    const QSet<QString>& wardUnitNumbers() const { return m_wardUnitNumbers; }

    // Stake contact info
    const QString& phone() const { return m_phone; }
    const QString& email() const { return m_email; }

    // Stake center location (from stake lookup API)
    const QString& stakeCenterAddress() const { return m_stakeCenterAddress; }
    std::optional<double> stakeCenterLat() const { return m_stakeCenterLat; }
    std::optional<double> stakeCenterLng() const { return m_stakeCenterLng; }

    // ========================================================================
    // Setters
    // ========================================================================
    void setUnitNumber(const QString& unitNumber) { m_unitNumber = unitNumber; }
    void setName(const QString& name);
    void setWardUnitNumbers(const QSet<QString>& wardUnits) { m_wardUnitNumbers = wardUnits; }
    void setPhone(const QString& phone) { m_phone = phone; }
    void setEmail(const QString& email) { m_email = email; }
    void setStakeCenterAddress(const QString& address) { m_stakeCenterAddress = address; }
    void setStakeCenterLat(std::optional<double> lat) { m_stakeCenterLat = lat; }
    void setStakeCenterLng(std::optional<double> lng) { m_stakeCenterLng = lng; }

    // ========================================================================
    // JSON serialization
    // ========================================================================
    QJsonObject toJson() const;
    static Stake fromJson(const QJsonObject& json);

    // ========================================================================
    // Equality
    // ========================================================================
    bool operator==(const Stake& other) const;
    bool operator!=(const Stake& other) const { return !(*this == other); }

    // Check if any data is present
    bool isEmpty() const { return m_name.isEmpty() && m_unitNumber.isEmpty(); }

private:
    QString m_unitNumber;              // e.g., "510033"
    QString m_name;                    // "Winston-Salem North Carolina" (not "... Stake")

    // Ward unit numbers in this stake (from stake API, reference data)
    QSet<QString> m_wardUnitNumbers;

    // Stake contact info
    QString m_phone;                   // Stake phone number
    QString m_email;                   // Stake email address

    // Stake center location (from stake lookup API)
    QString m_stakeCenterAddress;      // Stake center building address
    std::optional<double> m_stakeCenterLat;  // Stake center latitude
    std::optional<double> m_stakeCenterLng;  // Stake center longitude
};
