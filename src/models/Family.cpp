#include "Family.h"

namespace
{

/// The kinds of member, in the order the member order rule places them.
enum class MemberKind
{
    Father,
    Mother,
    OtherParent,
    Child
};

MemberKind kindOf(const Person& person)
{
    if (!person.isParent())
    {
        return MemberKind::Child;
    }
    if (person.gender() == Gender::Male)
    {
        return MemberKind::Father;
    }
    if (person.gender() == Gender::Female)
    {
        return MemberKind::Mother;
    }
    return MemberKind::OtherParent;
}

/// True when the rule puts person ahead of member: an earlier kind, or an older child.
/// Both birth years are known.
bool comesBefore(const Person& person, const Person& member)
{
    MemberKind personKind = kindOf(person);
    MemberKind memberKind = kindOf(member);
    if (personKind != memberKind)
    {
        return personKind < memberKind;
    }
    return personKind == MemberKind::Child
        && *person.birthday().year() < *member.birthday().year();
}

}  // namespace

Family Family::create(const Address& address, const QList<Person>& members)
{
    Family family;
    family.m_id = FamilyId::generate();
    family.m_address = address;
    family.m_members = members;
    return family;
}

Family Family::createWithId(const FamilyId& id, const Address& address, const QList<Person>& members)
{
    Family family;
    family.m_id = id;
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

qsizetype Family::insertionIndexFor(const Person& person) const
{
    if (!person.birthday().year().has_value())
    {
        return m_members.size();
    }
    for (const Person& member : m_members)
    {
        if (!member.birthday().year().has_value())
        {
            return m_members.size();
        }
    }
    for (qsizetype i = 0; i < m_members.size(); ++i)
    {
        if (comesBefore(person, m_members[i]))
        {
            return i;
        }
    }
    return m_members.size();
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

QJsonObject Family::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();

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

    if (!m_note.isEmpty())
    {
        json["note"] = m_note;
    }

    return json;
}

Family Family::fromJson(JsonReader json)
{
    Family family;
    family.m_id = json.id<FamilyId>("id");
    family.m_address = Address::fromLines(json.optionalStrings("address"));
    const QList<JsonReader> members = json.optionalObjects("members");
    for (const JsonReader& member : members)
    {
        family.m_members.append(Person::fromJson(member));
    }
    family.m_note = json.optionalString("note");
    json.finish();
    return family;
}

bool Family::operator==(const Family& other) const
{
    return m_id == other.m_id
        && m_address == other.m_address
        && m_members == other.m_members
        && m_note == other.m_note;
}
