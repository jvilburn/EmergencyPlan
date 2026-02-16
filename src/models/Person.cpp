#include "Person.h"

Person Person::create(const Name& name,
                      bool isParent,
                      const Phone& phone,
                      const Phone& altPhone,
                      const Email& email,
                      std::optional<Gender> gender,
                      const Birthday& birthday,
                      const QStringList& callings)
{
    Person person;
    person.m_id = PersonId::generate();
    person.m_name = name;
    person.m_isParent = isParent;
    person.m_phone = phone;
    person.m_altPhone = altPhone;
    person.m_email = email;
    person.m_gender = gender;
    person.m_birthday = birthday;
    person.m_callings = callings;
    return person;
}

Person Person::createWithId(
    const PersonId& id,
    const Name& name,
    std::optional<Gender> gender,
    const Birthday& birthday,
    const Phone& phone,
    const Phone& altPhone,
    const QString& email,
    const QStringList& callings,
    bool isParent,
    const QString& wardUnitNumber,
    const QString& stakeUnitNumber)
{
    Person person;
    person.m_id = id;
    person.m_name = name;
    person.m_gender = gender;
    person.m_birthday = birthday;
    person.m_phone = phone;
    person.m_altPhone = altPhone;
    person.m_email = email;
    person.m_callings = callings;
    person.m_isParent = isParent;
    person.m_wardUnitNumber = wardUnitNumber;
    person.m_stakeUnitNumber = stakeUnitNumber;
    return person;
}

Phone Person::displayPhone() const
{
    if (!m_phone.isEmpty())
    {
        return m_phone;
    }
    return m_altPhone;
}

QList<Phone> Person::allPhoneNumbers() const
{
    QList<Phone> phones;
    if (!m_phone.isEmpty())
    {
        phones.append(m_phone);
    }
    if (!m_altPhone.isEmpty())
    {
        phones.append(m_altPhone);
    }
    return phones;
}

QJsonObject Person::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;

    if (!m_phone.isEmpty())
    {
        json["phone"] = m_phone;
    }
    if (!m_altPhone.isEmpty())
    {
        json["altPhone"] = m_altPhone;
    }
    if (!m_email.isEmpty())
    {
        json["email"] = m_email;
    }
    if (m_gender.has_value())
    {
        json["gender"] = m_gender->toJson();
    }
    if (m_birthday.hasDate())
    {
        json["birthday"] = m_birthday.toJson();
    }
    if (!m_callings.isEmpty())
    {
        QJsonArray callingsArray;
        for (const QString& calling : m_callings)
        {
            callingsArray.append(calling);
        }
        json["callings"] = callingsArray;
    }
    if (!m_stakeUnitNumber.isEmpty())
    {
        json["stakeUnitNumber"] = m_stakeUnitNumber;
    }
    if (!m_wardUnitNumber.isEmpty())
    {
        json["wardUnitNumber"] = m_wardUnitNumber;
    }
    if (m_isParent)
    {
        json["isParent"] = true;
    }
    if (!m_specialNeedNote.isEmpty())
    {
        json["specialNeedNote"] = m_specialNeedNote;
    }

    return json;
}

Person Person::fromJson(const QJsonObject& json)
{
    Person person;
    person.m_id = PersonId::fromString(json["id"].toString());
    person.m_name = Name(json["name"].toString());
    person.m_phone = Phone(json["phone"].toString());
    person.m_altPhone = Phone(json["altPhone"].toString());
    person.m_email = json["email"].toString();

    if (json.contains("gender"))
    {
        person.m_gender = Gender::fromString(json["gender"].toString());
    }
    if (json.contains("birthday"))
    {
        person.m_birthday = Birthday::fromJson(json["birthday"].toObject());
    }

    if (json.contains("callings"))
    {
        QJsonArray callingsArray = json["callings"].toArray();
        for (const QJsonValue& value : callingsArray)
        {
            person.m_callings.append(value.toString());
        }
    }

    person.m_stakeUnitNumber = json["stakeUnitNumber"].toString();
    person.m_wardUnitNumber = json["wardUnitNumber"].toString();
    person.m_isParent = json["isParent"].toBool();  // false if missing
    person.m_specialNeedNote = json["specialNeedNote"].toString();  // empty if missing

    return person;
}

bool Person::operator==(const Person& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_phone == other.m_phone
        && m_altPhone == other.m_altPhone
        && m_email == other.m_email
        && m_gender == other.m_gender
        && m_birthday == other.m_birthday
        && m_callings == other.m_callings
        && m_stakeUnitNumber == other.m_stakeUnitNumber
        && m_wardUnitNumber == other.m_wardUnitNumber
        && m_isParent == other.m_isParent
        && m_specialNeedNote == other.m_specialNeedNote;
}
