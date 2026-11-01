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
    static Family create(const Address& address, const QList<Person>& members);

    // Factory method for creating families with a specific ID (used for ID preservation)
    static Family createWithId(const FamilyId& id, const Address& address, const QList<Person>& members);

    // Getters
    const FamilyId& id() const { return m_id; }
    const Address& address() const { return m_address; }
    const QList<Person>& members() const { return m_members; }
    const QString& note() const { return m_note; }

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
    void setAddress(const Address& address);
    void setMembers(const QList<Person>& members);
    void setNote(const QString& note) { m_note = note; }

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
    Address m_address;
    QList<Person> m_members;
    QString m_note;
};
