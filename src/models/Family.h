#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <optional>

#include "Address.h"
#include "Person.h"

class Family
{
public:
    Family() = default;

    // Factory method for creating new families
    static Family create(std::optional<double> latitude = std::nullopt,
                         std::optional<double> longitude = std::nullopt,
                         const Address& address = Address(),
                         const QList<Person>& members = QList<Person>());

    // Factory method for creating families with a specific ID (used for ID preservation)
    static Family createWithId(
        const QString& id,
        std::optional<double> latitude = std::nullopt,
        std::optional<double> longitude = std::nullopt,
        const Address& address = Address(),
        const QList<Person>& members = QList<Person>());

    // Getters
    const QString& id() const { return m_id; }
    std::optional<double> latitude() const { return m_latitude; }
    std::optional<double> longitude() const { return m_longitude; }
    const Address& address() const { return m_address; }
    const QList<Person>& members() const { return m_members; }

    // Computed properties from members
    QString surname() const;
    QString displayName() const;
    QList<Person> parents() const;
    QList<Person> children() const;

    // Setters
    void setLatitude(std::optional<double> latitude);
    void setLongitude(std::optional<double> longitude);
    void setLocation(std::optional<double> latitude, std::optional<double> longitude);
    void setAddress(const Address& address);
    void setMembers(const QList<Person>& members);

    // Computed properties
    bool isMapped() const { return m_latitude.has_value() && m_longitude.has_value(); }

    // Contact helpers - aggregate from all members
    Phone displayPhone() const;
    QString displayEmail() const;
    bool hasContact() const;
    QList<Phone> allPhoneNumbers() const;
    QStringList allEmails() const;
    QStringList allCallings() const;
    bool hasCallings() const;

    // JSON serialization
    QJsonObject toJson() const;
    static Family fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Family& other) const;
    bool operator!=(const Family& other) const { return !(*this == other); }

private:
    QString m_id;
    std::optional<double> m_latitude;
    std::optional<double> m_longitude;
    Address m_address;
    QList<Person> m_members;
};
