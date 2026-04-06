#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>

/// Ward-level metadata (from PDF header and chapel lookup)
class Ward
{
public:
    Ward() = default;

    // ========================================================================
    // Getters
    // ========================================================================
    const QString& id() const { return m_unitNumber; }  // For JSON serialization template
    const QString& name() const { return m_name; }
    const QString& unitNumber() const { return m_unitNumber; }
    const QString& stakeUnitNumber() const { return m_stakeUnitNumber; }
    const std::optional<QString>& chapelAddress() const { return m_chapelAddress; }
    std::optional<double> chapelLat() const { return m_chapelLat; }
    std::optional<double> chapelLng() const { return m_chapelLng; }
    const std::optional<QString>& chapelPhone() const { return m_chapelPhone; }
    const std::optional<QString>& meetingTime() const { return m_meetingTime; }

    // ========================================================================
    // Setters (for mutable Document operations)
    // ========================================================================
    void setName(const QString& name);
    void setUnitNumber(const QString& unitNumber) { m_unitNumber = unitNumber; }
    void setStakeUnitNumber(const QString& stakeUnitNumber) { m_stakeUnitNumber = stakeUnitNumber; }
    void setChapelAddress(const std::optional<QString>& address) { m_chapelAddress = address; }
    void setChapelLat(std::optional<double> lat) { m_chapelLat = lat; }
    void setChapelLng(std::optional<double> lng) { m_chapelLng = lng; }
    void setChapelPhone(const std::optional<QString>& phone) { m_chapelPhone = phone; }
    void setMeetingTime(const std::optional<QString>& meetingTime) { m_meetingTime = meetingTime; }

    // ========================================================================
    // JSON serialization
    // ========================================================================
    QJsonObject toJson() const;
    static Ward fromJson(const QJsonObject& json);

    // ========================================================================
    // Equality
    // ========================================================================
    bool operator==(const Ward& other) const;
    bool operator!=(const Ward& other) const { return !(*this == other); }

    // Check if any data is present
    bool isEmpty() const { return m_name.isEmpty() && m_unitNumber.isEmpty(); }

private:
    // From PDF header (name stored without " Ward" suffix)
    QString m_name;                          // "Example" (not "Example Ward")
    QString m_unitNumber;                    // "123456"
    QString m_stakeUnitNumber;               // "654321" (from ward lookup)

    // From chapel lookup
    std::optional<QString> m_chapelAddress;  // "123 Chapel Street, Anytown, ST 12345"
    std::optional<double> m_chapelLat;       // 40.0
    std::optional<double> m_chapelLng;       // -111.0
    std::optional<QString> m_chapelPhone;    // "+1 555-555-1234"
    std::optional<QString> m_meetingTime;    // "Sunday 9:00 AM"
};
