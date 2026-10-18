#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>

#include "Address.h"
#include "Id.h"
#include "Person.h"

class Family
{
public:
    Family() = default;

    // Factory method for creating new families
    static Family create(std::optional<double> latitude,
                         std::optional<double> longitude,
                         const Address& address,
                         const QList<Person>& members);

    // Factory method for creating families with a specific ID (used for ID preservation)
    static Family createWithId(
        const FamilyId& id,
        std::optional<double> latitude,
        std::optional<double> longitude,
        const Address& address,
        const QList<Person>& members);

    // Getters
    const FamilyId& id() const { return m_id; }
    std::optional<double> latitude() const { return m_latitude; }
    std::optional<double> longitude() const { return m_longitude; }
    const Address& address() const { return m_address; }
    const QList<Person>& members() const { return m_members; }

    // Computed properties from members
    QString surname() const;
    QString displayName() const;
    QList<Person> children() const;

    /// Where the member order rule puts person among the members: the index of the first
    /// member person comes before, by fathers (parents who are male), then mothers (parents
    /// who are female), then any other parent, then children oldest to youngest. The end
    /// when person comes after everyone, or when person's birth year or any member's is
    /// unknown.
    qsizetype insertionIndexFor(const Person& person) const;

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
    QStringList allCallings() const;

    // JSON serialization
    QJsonObject toJson() const;
    static Family fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Family& other) const;
    bool operator!=(const Family& other) const { return !(*this == other); }

private:
    FamilyId m_id;
    std::optional<double> m_latitude;
    std::optional<double> m_longitude;
    Address m_address;
    QList<Person> m_members;
};
