#include "Family.h"

Family Family::create(std::optional<double> latitude,
                      std::optional<double> longitude,
                      const Address& address,
                      const QList<Person>& members)
{
    Family family;
    family.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    family.m_latitude = latitude;
    family.m_longitude = longitude;
    family.m_address = address;
    family.m_members = members;
    return family;
}

QString Family::surname() const
{
    if (m_members.isEmpty())
    {
        return QString();
    }
    return m_members[0].surname();
}

QString Family::displayName() const
{
    if (m_members.isEmpty())
    {
        return QString();
    }

    QString result = surname();
    QStringList givenNames;
    for (const Person& member : m_members)
    {
        if (member.isParent())
        {
            givenNames.append(member.givenNames());
        }
    }

    // Fall back to first member's given name if no parents
    if (givenNames.isEmpty())
    {
        givenNames.append(m_members[0].givenNames());
    }

    result += ", " + givenNames.join(" & ");
    return result;
}

QList<Person> Family::parents() const
{
    QList<Person> result;
    for (const Person& member : m_members)
    {
        if (member.isParent())
        {
            result.append(member);
        }
    }
    return result;
}

QList<Person> Family::children() const
{
    QList<Person> result;
    for (const Person& member : m_members)
    {
        if (!member.isParent())
        {
            result.append(member);
        }
    }
    return result;
}

void Family::setLatitude(std::optional<double> latitude)
{
    m_latitude = latitude;
}

void Family::setLongitude(std::optional<double> longitude)
{
    m_longitude = longitude;
}

void Family::setLocation(std::optional<double> latitude, std::optional<double> longitude)
{
    m_latitude = latitude;
    m_longitude = longitude;
}

void Family::setAddress(const Address& address)
{
    m_address = address;
}

void Family::setMembers(const QList<Person>& members)
{
    m_members = members;
}

Phone Family::displayPhone() const
{
    for (const Person& member : m_members)
    {
        Phone phone = member.displayPhone();
        if (!phone.isEmpty())
        {
            return phone;
        }
    }
    return Phone();
}

QString Family::displayEmail() const
{
    for (const Person& member : m_members)
    {
        if (!member.email().isEmpty())
        {
            return member.email();
        }
    }
    return QString();
}

bool Family::hasContact() const
{
    for (const Person& member : m_members)
    {
        if (member.hasContact())
        {
            return true;
        }
    }
    return false;
}

QList<Phone> Family::allPhoneNumbers() const
{
    QSet<QString> seen;
    QList<Phone> phones;
    for (const Person& member : m_members)
    {
        for (const Phone& phone : member.allPhoneNumbers())
        {
            if (!seen.contains(phone))
            {
                seen.insert(phone);
                phones.append(phone);
            }
        }
    }
    return phones;
}

QStringList Family::allEmails() const
{
    QStringList emails;
    for (const Person& member : m_members)
    {
        if (!member.email().isEmpty())
        {
            emails.append(member.email());
        }
    }
    emails.removeDuplicates();
    return emails;
}

QStringList Family::allCallings() const
{
    QStringList callings;
    for (const Person& member : m_members)
    {
        callings.append(member.callings());
    }
    callings.removeDuplicates();
    return callings;
}

bool Family::hasCallings() const
{
    for (const Person& member : m_members)
    {
        if (member.hasCallings())
        {
            return true;
        }
    }
    return false;
}

QJsonObject Family::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;

    if (m_latitude.has_value())
    {
        json["latitude"] = m_latitude.value();
    }
    if (m_longitude.has_value())
    {
        json["longitude"] = m_longitude.value();
    }
    if (!m_address.isEmpty())
    {
        QJsonArray addressArray;
        for (const QString& line : m_address.lines())
        {
            addressArray.append(line);
        }
        json["address"] = addressArray;
    }

    if (!m_members.isEmpty())
    {
        QJsonArray membersArray;
        for (const Person& member : m_members)
        {
            membersArray.append(member.toJson());
        }
        json["members"] = membersArray;
    }

    return json;
}

Family Family::fromJson(const QJsonObject& json)
{
    Family family;
    family.m_id = json["id"].toString();

    if (json.contains("latitude"))
    {
        family.m_latitude = json["latitude"].toDouble();
    }
    if (json.contains("longitude"))
    {
        family.m_longitude = json["longitude"].toDouble();
    }

    if (json.contains("address"))
    {
        QJsonArray addressArray = json["address"].toArray();
        for (const QJsonValue& value : addressArray)
        {
            family.m_address.addLine(value.toString());
        }
    }

    if (json.contains("members"))
    {
        QJsonArray membersArray = json["members"].toArray();
        for (const QJsonValue& value : membersArray)
        {
            family.m_members.append(Person::fromJson(value.toObject()));
        }
    }

    return family;
}

bool Family::operator==(const Family& other) const
{
    return m_id == other.m_id
        && m_latitude == other.m_latitude
        && m_longitude == other.m_longitude
        && m_address == other.m_address
        && m_members == other.m_members;
}
