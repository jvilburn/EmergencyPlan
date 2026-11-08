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
    if (!m_birthday.isEmpty())
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
    if (!m_note.isEmpty())
    {
        json["note"] = m_note;
    }

    return json;
}

Person Person::fromJson(JsonReader json)
{
    Person person;
    person.m_id = json.id<PersonId>("id");
    person.m_name = Name(json.string("name"));
    person.m_phone = Phone(json.optionalString("phone"));
    person.m_altPhone = Phone(json.optionalString("altPhone"));
    person.m_email = json.optionalString("email");
    person.m_gender = json.optionalWord("gender", Gender::values(), &Gender::toString);
    std::optional<JsonReader> birthday = json.optionalObject("birthday");
    if (birthday.has_value())
    {
        person.m_birthday = Birthday::fromJson(*birthday);
    }
    person.m_callings = json.optionalStrings("callings");
    person.m_stakeUnitNumber = json.optionalString("stakeUnitNumber");
    person.m_wardUnitNumber = json.optionalString("wardUnitNumber");
    person.m_isParent = json.optionalFlag("isParent");
    person.m_specialNeedNote = json.optionalString("specialNeedNote");
    person.m_note = json.optionalString("note");
    json.finish();
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
        && m_specialNeedNote == other.m_specialNeedNote
        && m_note == other.m_note;
}
