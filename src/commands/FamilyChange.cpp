#include "FamilyChange.h"
#include "ChangeJson.h"

#include <QJsonArray>
#include <QObject>

namespace
{

std::optional<Person> findMember(const Family& family, const PersonId& id)
{
    for (const Person& member : family.members())
    {
        if (member.id() == id)
        {
            return member;
        }
    }
    return std::nullopt;
}

bool containsPerson(const QList<Person>& people, const PersonId& id)
{
    for (const Person& person : people)
    {
        if (person.id() == id)
        {
            return true;
        }
    }
    return false;
}

QJsonArray personsToJson(const QList<Person>& people)
{
    QJsonArray array;
    for (const Person& person : people)
    {
        array.append(person.toJson());
    }
    return array;
}

QList<Person> personsFromJson(const QJsonArray& array)
{
    QList<Person> people;
    for (const QJsonValue& value : array)
    {
        people.append(Person::fromJson(value.toObject()));
    }
    return people;
}

QString orNone(const QString& value)
{
    if (value.isEmpty())
    {
        return QObject::tr("(none)");
    }
    return value;
}

QString genderText(const std::optional<Gender>& gender)
{
    if (!gender.has_value())
    {
        return QObject::tr("(none)");
    }
    if (*gender == Gender::Male)
    {
        return QObject::tr("male");
    }
    return QObject::tr("female");
}

}  // namespace

// ============================================================================
// MemberChange
// ============================================================================

std::optional<MemberChange> MemberChange::between(const Person& before, const Person& after)
{
    MemberChange change;
    change.m_name = changeBetween(before.name(), after.name());
    change.m_gender = changeBetween(before.gender(), after.gender());
    change.m_phone = changeBetween(before.phone(), after.phone());
    change.m_altPhone = changeBetween(before.altPhone(), after.altPhone());
    change.m_email = changeBetween(before.email(), after.email());
    change.m_birthday = changeBetween(before.birthday(), after.birthday());
    change.m_callings = changeBetween(before.callings(), after.callings());
    change.m_isParent = changeBetween(before.isParent(), after.isParent());
    change.m_wardUnitNumber = changeBetween(before.wardUnitNumber(), after.wardUnitNumber());
    change.m_stakeUnitNumber = changeBetween(before.stakeUnitNumber(), after.stakeUnitNumber());
    change.m_specialNeedNote = changeBetween(before.specialNeedNote(), after.specialNeedNote());
    if (change.isEmpty())
    {
        return std::nullopt;
    }
    return change;
}

bool MemberChange::holds(const Person& current) const
{
    return changeHolds(m_name, current.name())
        && changeHolds(m_gender, current.gender())
        && changeHolds(m_phone, current.phone())
        && changeHolds(m_altPhone, current.altPhone())
        && changeHolds(m_email, current.email())
        && changeHolds(m_birthday, current.birthday())
        && changeHolds(m_callings, current.callings())
        && changeHolds(m_isParent, current.isParent())
        && changeHolds(m_wardUnitNumber, current.wardUnitNumber())
        && changeHolds(m_stakeUnitNumber, current.stakeUnitNumber())
        && changeHolds(m_specialNeedNote, current.specialNeedNote());
}

Person MemberChange::appliedTo(const Person& current) const
{
    Person updated = current;
    updated.setName(changedValue(m_name, current.name()));
    updated.setGender(changedValue(m_gender, current.gender()));
    updated.setPhone(changedValue(m_phone, current.phone()));
    updated.setAltPhone(changedValue(m_altPhone, current.altPhone()));
    updated.setEmail(changedValue(m_email, current.email()));
    updated.setBirthday(changedValue(m_birthday, current.birthday()));
    updated.setCallings(changedValue(m_callings, current.callings()));
    updated.setIsParent(changedValue(m_isParent, current.isParent()));
    updated.setWardUnitNumber(changedValue(m_wardUnitNumber, current.wardUnitNumber()));
    updated.setStakeUnitNumber(changedValue(m_stakeUnitNumber, current.stakeUnitNumber()));
    updated.setSpecialNeedNote(changedValue(m_specialNeedNote, current.specialNeedNote()));
    return updated;
}

MemberChange MemberChange::swapped() const
{
    MemberChange change;
    change.m_name = changeSwapped(m_name);
    change.m_gender = changeSwapped(m_gender);
    change.m_phone = changeSwapped(m_phone);
    change.m_altPhone = changeSwapped(m_altPhone);
    change.m_email = changeSwapped(m_email);
    change.m_birthday = changeSwapped(m_birthday);
    change.m_callings = changeSwapped(m_callings);
    change.m_isParent = changeSwapped(m_isParent);
    change.m_wardUnitNumber = changeSwapped(m_wardUnitNumber);
    change.m_stakeUnitNumber = changeSwapped(m_stakeUnitNumber);
    change.m_specialNeedNote = changeSwapped(m_specialNeedNote);
    return change;
}

QStringList MemberChange::describe(const QString& firstName) const
{
    QStringList lines;
    if (m_name.has_value())
    {
        lines.append(QObject::tr("%1, name: %2").arg(firstName, m_name->after().full()));
    }
    if (m_gender.has_value())
    {
        lines.append(QObject::tr("%1, gender: %2").arg(firstName, genderText(m_gender->after())));
    }
    if (m_phone.has_value())
    {
        lines.append(QObject::tr("%1, phone: %2").arg(firstName, orNone(m_phone->after())));
    }
    if (m_altPhone.has_value())
    {
        lines.append(QObject::tr("%1, other phone: %2").arg(firstName, orNone(m_altPhone->after())));
    }
    if (m_email.has_value())
    {
        lines.append(QObject::tr("%1, email: %2").arg(firstName, orNone(m_email->after())));
    }
    if (m_birthday.has_value())
    {
        lines.append(QObject::tr("%1, birthday: %2").arg(firstName, orNone(m_birthday->after().dateDisplay())));
    }
    if (m_callings.has_value())
    {
        lines.append(QObject::tr("%1, callings: %2").arg(firstName, orNone(m_callings->after().join(", "))));
    }
    if (m_isParent.has_value())
    {
        if (m_isParent->after())
        {
            lines.append(QObject::tr("%1, parent").arg(firstName));
        }
        else
        {
            lines.append(QObject::tr("%1, not a parent").arg(firstName));
        }
    }
    if (m_wardUnitNumber.has_value())
    {
        lines.append(QObject::tr("%1, ward unit: %2").arg(firstName, orNone(m_wardUnitNumber->after())));
    }
    if (m_stakeUnitNumber.has_value())
    {
        lines.append(QObject::tr("%1, stake unit: %2").arg(firstName, orNone(m_stakeUnitNumber->after())));
    }
    if (m_specialNeedNote.has_value())
    {
        lines.append(QObject::tr("%1, special need: %2").arg(firstName, orNone(m_specialNeedNote->after())));
    }
    return lines;
}

QJsonObject MemberChange::toJson() const
{
    QJsonObject json;
    writeChange(json, "name", m_name);
    writeChange(json, "gender", m_gender);
    writeChange(json, "phone", m_phone);
    writeChange(json, "altPhone", m_altPhone);
    writeChange(json, "email", m_email);
    writeChange(json, "birthday", m_birthday);
    writeChange(json, "callings", m_callings);
    writeChange(json, "isParent", m_isParent);
    writeChange(json, "wardUnitNumber", m_wardUnitNumber);
    writeChange(json, "stakeUnitNumber", m_stakeUnitNumber);
    writeChange(json, "specialNeedNote", m_specialNeedNote);
    return json;
}

MemberChange MemberChange::fromJson(const QJsonObject& json)
{
    MemberChange change;
    change.m_name = readChange<Name>(json, "name");
    change.m_gender = readChange<std::optional<Gender>>(json, "gender");
    change.m_phone = readChange<Phone>(json, "phone");
    change.m_altPhone = readChange<Phone>(json, "altPhone");
    change.m_email = readChange<QString>(json, "email");
    change.m_birthday = readChange<Birthday>(json, "birthday");
    change.m_callings = readChange<QStringList>(json, "callings");
    change.m_isParent = readChange<bool>(json, "isParent");
    change.m_wardUnitNumber = readChange<QString>(json, "wardUnitNumber");
    change.m_stakeUnitNumber = readChange<QString>(json, "stakeUnitNumber");
    change.m_specialNeedNote = readChange<QString>(json, "specialNeedNote");
    return change;
}

bool MemberChange::isEmpty() const
{
    return !m_name.has_value()
        && !m_gender.has_value()
        && !m_phone.has_value()
        && !m_altPhone.has_value()
        && !m_email.has_value()
        && !m_birthday.has_value()
        && !m_callings.has_value()
        && !m_isParent.has_value()
        && !m_wardUnitNumber.has_value()
        && !m_stakeUnitNumber.has_value()
        && !m_specialNeedNote.has_value();
}

// ============================================================================
// FamilyChange
// ============================================================================

std::optional<FamilyChange> FamilyChange::between(const Family& opened, const Family& saved)
{
    QHash<PersonId, MemberChange> members;
    QList<Person> removed;
    for (const Person& before : opened.members())
    {
        std::optional<Person> after = findMember(saved, before.id());
        if (!after.has_value())
        {
            removed.append(before);
            continue;
        }
        std::optional<MemberChange> change = MemberChange::between(before, *after);
        if (change.has_value())
        {
            members.insert(before.id(), *change);
        }
    }

    QList<Person> added;
    for (const Person& after : saved.members())
    {
        if (!findMember(opened, after.id()).has_value())
        {
            added.append(after);
        }
    }

    std::optional<Change<Address>> address = changeBetween(opened.address(), saved.address());
    if (!address.has_value() && members.isEmpty() && added.isEmpty() && removed.isEmpty())
    {
        return std::nullopt;
    }
    return FamilyChange(address, members, added, removed);
}

FamilyChange::FamilyChange(const std::optional<Change<Address>>& address,
                           const QHash<PersonId, MemberChange>& members,
                           const QList<Person>& added,
                           const QList<Person>& removed)
    : m_address(address)
    , m_members(members)
    , m_added(added)
    , m_removed(removed)
{
}

bool FamilyChange::holds(const Family& current) const
{
    if (!changeHolds(m_address, current.address()))
    {
        return false;
    }
    for (auto it = m_members.constBegin(); it != m_members.constEnd(); ++it)
    {
        std::optional<Person> member = findMember(current, it.key());
        if (!member.has_value() || !it.value().holds(*member))
        {
            return false;
        }
    }
    for (const Person& person : m_removed)
    {
        std::optional<Person> member = findMember(current, person.id());
        if (member.has_value() && *member != person)
        {
            return false;
        }
    }
    return true;
}

Family FamilyChange::appliedTo(const Family& current) const
{
    QList<Person> members;
    for (const Person& member : current.members())
    {
        if (containsPerson(m_removed, member.id()))
        {
            continue;
        }
        auto change = m_members.constFind(member.id());
        if (change == m_members.constEnd())
        {
            members.append(member);
        }
        else
        {
            members.append(change.value().appliedTo(member));
        }
    }
    for (const Person& person : m_added)
    {
        if (!findMember(current, person.id()).has_value())
        {
            members.append(person);
        }
    }

    Family updated = current;
    updated.setAddress(changedValue(m_address, current.address()));
    updated.setMembers(members);
    return updated;
}

FamilyChange FamilyChange::swapped() const
{
    QHash<PersonId, MemberChange> members;
    for (auto it = m_members.constBegin(); it != m_members.constEnd(); ++it)
    {
        members.insert(it.key(), it.value().swapped());
    }
    return FamilyChange(changeSwapped(m_address), members, m_removed, m_added);
}

QStringList FamilyChange::describe(const Family& opened) const
{
    QStringList lines;
    for (const Person& member : opened.members())
    {
        auto change = m_members.constFind(member.id());
        if (change != m_members.constEnd())
        {
            lines.append(change.value().describe(member.name().firstName()));
        }
    }
    for (const Person& person : m_added)
    {
        lines.append(QObject::tr("added: %1").arg(person.displayName()));
    }
    for (const Person& person : m_removed)
    {
        lines.append(QObject::tr("removed: %1").arg(person.displayName()));
    }
    if (m_address.has_value())
    {
        lines.append(QObject::tr("address: %1").arg(orNone(m_address->after().full())));
    }
    return lines;
}

QJsonObject FamilyChange::toJson() const
{
    QJsonObject json;
    writeChange(json, "address", m_address);
    if (!m_members.isEmpty())
    {
        QJsonObject members;
        for (auto it = m_members.constBegin(); it != m_members.constEnd(); ++it)
        {
            members[it.key().toString()] = it.value().toJson();
        }
        json["members"] = members;
    }
    if (!m_added.isEmpty())
    {
        json["added"] = personsToJson(m_added);
    }
    if (!m_removed.isEmpty())
    {
        json["removed"] = personsToJson(m_removed);
    }
    return json;
}

FamilyChange FamilyChange::fromJson(const QJsonObject& json)
{
    QHash<PersonId, MemberChange> members;
    QJsonObject membersJson = json["members"].toObject();
    for (auto it = membersJson.constBegin(); it != membersJson.constEnd(); ++it)
    {
        members.insert(PersonId::fromString(it.key()), MemberChange::fromJson(it.value().toObject()));
    }
    return FamilyChange(readChange<Address>(json, "address"),
                        members,
                        personsFromJson(json["added"].toArray()),
                        personsFromJson(json["removed"].toArray()));
}
