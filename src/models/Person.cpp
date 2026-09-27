#include "Person.h"
#include "StrictJson.h"

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

std::optional<Person> Person::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("name"),
                                  QLatin1String("phone"),
                                  QLatin1String("altPhone"),
                                  QLatin1String("email"),
                                  QLatin1String("gender"),
                                  QLatin1String("birthday"),
                                  QLatin1String("callings"),
                                  QLatin1String("stakeUnitNumber"),
                                  QLatin1String("wardUnitNumber"),
                                  QLatin1String("isParent"),
                                  QLatin1String("specialNeedNote")}))
    {
        return std::nullopt;
    }

    Person person;
    QString name;
    if (!StrictJson::readId(json, QLatin1String("id"), person.m_id)
        || !StrictJson::readString(json, QLatin1String("name"), name))
    {
        return std::nullopt;
    }
    person.m_name = Name(name);

    // Written only when not empty.
    QString phone;
    QString altPhone;
    if (!StrictJson::readOptionalNonEmptyString(json, QLatin1String("stakeUnitNumber"), person.m_stakeUnitNumber)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("wardUnitNumber"), person.m_wardUnitNumber)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("specialNeedNote"), person.m_specialNeedNote)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("email"), person.m_email)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("phone"), phone)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("altPhone"), altPhone))
    {
        return std::nullopt;
    }
    person.m_phone = Phone(phone);
    person.m_altPhone = Phone(altPhone);

    if (json.contains("gender"))
    {
        person.m_gender = Gender::fromJson(json.value("gender"));
        if (!person.m_gender.has_value())
        {
            return std::nullopt;
        }
    }

    if (json.contains("birthday"))
    {
        QJsonObject birthdayJson;
        if (!StrictJson::readObject(json, QLatin1String("birthday"), birthdayJson))
        {
            return std::nullopt;
        }
        // Written only for a birthday with a date.
        const std::optional<Birthday> birthday = Birthday::fromJson(birthdayJson);
        if (!birthday.has_value() || !birthday.value().hasDate())
        {
            return std::nullopt;
        }
        person.m_birthday = birthday.value();
    }

    if (json.contains("callings")
        && (!StrictJson::readStringArray(json, QLatin1String("callings"), person.m_callings)
            || person.m_callings.isEmpty()))
    {
        return std::nullopt;
    }

    // Written only when true.
    if (json.contains("isParent"))
    {
        if (!StrictJson::readBool(json, QLatin1String("isParent"), person.m_isParent) || !person.m_isParent)
        {
            return std::nullopt;
        }
    }

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
