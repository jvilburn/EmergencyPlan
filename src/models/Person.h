#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <optional>

#include "Birthday.h"
#include "Email.h"
#include "Gender.h"
#include "Name.h"
#include "Phone.h"

class Person
{
public:
    Person() = default;

    // Factory method for creating new persons
    static Person create(const Name& name,
                         bool isParent,
                         const Phone& phone = Phone(),
                         const Phone& altPhone = Phone(),
                         const Email& email = Email(),
                         std::optional<Gender> gender = std::nullopt,
                         const Birthday& birthday = Birthday(),
                         const QStringList& callings = QStringList());

    // Getters
    const QString& id() const { return m_id; }
    const Name& name() const { return m_name; }
    QString givenNames() const { return m_name.givenNames(); }
    QString surname() const { return m_name.surname(); }
    const Phone& phone() const { return m_phone; }
    const Phone& altPhone() const { return m_altPhone; }
    const QString& email() const { return m_email; }
    std::optional<Gender> gender() const { return m_gender; }
    const Birthday& birthday() const { return m_birthday; }
    const QStringList& callings() const { return m_callings; }
    const QString& stakeUnitNumber() const { return m_stakeUnitNumber; }
    const QString& wardUnitNumber() const { return m_wardUnitNumber; }
    bool isParent() const { return m_isParent; }

    // Setters
    void setName(const Name& name) { m_name = name; }
    void setGivenNames(const QString& givenNames) { m_name = Name(m_name.surname(), givenNames); }
    void setSurname(const QString& surname) { m_name = Name(surname, m_name.givenNames()); }
    void setPhone(const Phone& phone) { m_phone = phone; }
    void setAltPhone(const Phone& altPhone) { m_altPhone = altPhone; }
    void setEmail(const QString& email) { m_email = email; }
    void setGender(std::optional<Gender> gender) { m_gender = gender; }
    void setBirthday(const Birthday& birthday) { m_birthday = birthday; }
    void setCallings(const QStringList& callings) { m_callings = callings; }
    void setStakeUnitNumber(const QString& stakeUnitNumber) { m_stakeUnitNumber = stakeUnitNumber; }
    void setWardUnitNumber(const QString& wardUnitNumber) { m_wardUnitNumber = wardUnitNumber; }
    void setIsParent(bool isParent) { m_isParent = isParent; }

    // Display name: "Surname, GivenNames" (delegates to Name)
    QString displayName() const { return m_name.full(); }

    // Computed properties
    bool hasCallings() const { return !m_callings.isEmpty(); }
    bool hasContact() const { return !m_phone.isEmpty() || !m_altPhone.isEmpty() || !m_email.isEmpty(); }
    Phone displayPhone() const;
    QList<Phone> allPhoneNumbers() const;

    // Birth date helpers (delegated to Birthday)
    std::optional<int> age() const { return m_birthday.age(); }
    bool isChild() const { return m_birthday.isChild(); }
    bool isAdult() const { return m_birthday.isAdult(); }
    QString birthDateDisplay() const { return m_birthday.dateDisplay(); }
    QString ageDisplay() const { return m_birthday.ageDisplay(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Person fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Person& other) const;
    bool operator!=(const Person& other) const { return !(*this == other); }

private:
    QString m_id;
    Name m_name;
    Phone m_phone;
    Phone m_altPhone;
    QString m_email;
    std::optional<Gender> m_gender;
    Birthday m_birthday;
    QStringList m_callings;
    QString m_stakeUnitNumber;  // Associates person with a stake
    QString m_wardUnitNumber;   // Associates person with a ward
    bool m_isParent = false;
};
