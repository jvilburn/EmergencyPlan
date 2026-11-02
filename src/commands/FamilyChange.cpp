#include "FamilyChange.h"
#include "Document.h"
#include "FieldChangeJson.h"

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

bool containsPerson(const QList<PlacedMember>& placed, const PersonId& id)
{
    for (const PlacedMember& member : placed)
    {
        if (member.person.id() == id)
        {
            return true;
        }
    }
    return false;
}

QJsonArray placedToJson(const QList<PlacedMember>& placed)
{
    QJsonArray array;
    for (const PlacedMember& member : placed)
    {
        QJsonObject json;
        json["person"] = member.person.toJson();
        json["index"] = qint64(member.index);
        array.append(json);
    }
    return array;
}

QList<PlacedMember> placedFromJson(const QJsonArray& array)
{
    QList<PlacedMember> placed;
    for (const QJsonValue& value : array)
    {
        QJsonObject json = value.toObject();
        placed.append(PlacedMember{Person::fromJson(json["person"].toObject()), json["index"].toInteger()});
    }
    return placed;
}

/// The families as an object keyed by family id.
QJsonObject familiesToJson(const QHash<FamilyId, Family>& families)
{
    QJsonObject json;
    for (const Family& family : families)
    {
        json[family.id().toString()] = family.toJson();
    }
    return json;
}

QHash<FamilyId, Family> familiesFromJson(const QJsonObject& json)
{
    QHash<FamilyId, Family> families;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it)
    {
        Family family = Family::fromJson(it.value().toObject());
        families.insert(family.id(), family);
    }
    return families;
}

// ----------------------------------------------------------------------------
// Description text
// ----------------------------------------------------------------------------

QString orNone(const QString& value)
{
    if (value.isEmpty())
    {
        return QObject::tr("(none)");
    }
    return value;
}

QString valueText(const QString& value)
{
    return orNone(value);
}

QString valueText(const std::optional<Gender>& gender)
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

QString valueText(const Birthday& birthday)
{
    return orNone(birthday.display());
}

QString valueText(const QStringList& values)
{
    return orNone(values.join(", "));
}

/// isParent is the one yes-or-no member field, so a bool reads as parent or not.
QString valueText(bool isParent)
{
    if (isParent)
    {
        return QObject::tr("parent");
    }
    return QObject::tr("not a parent");
}

// ----------------------------------------------------------------------------
// Visitors over MemberChange::forEachField
// ----------------------------------------------------------------------------

/// Sets each field to the change from before's value to after's.
struct ChangeBetween
{
    const Person& before;
    const Person& after;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char*, Getter get, Setter, std::optional<FieldChange<T>>& field)
    {
        field = changeBetween<T>((before.*get)(), (after.*get)());
    }
};

/// Clears holds when a field's value in current holds for neither end of its change.
struct ChangesHold
{
    const Person& current;
    bool holds;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char*, Getter get, Setter, const std::optional<FieldChange<T>>& field)
    {
        holds = holds && changeHolds<T>(field, (current.*get)());
    }
};

/// Sets each changed field of person to its after value.
struct ApplyChanges
{
    Person& person;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char*, Getter get, Setter set, const std::optional<FieldChange<T>>& field)
    {
        (person.*set)(changedValue<T>(field, (person.*get)()));
    }
};

/// Exchanges each field's before and after.
struct SwapChanges
{
    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char*, Getter, Setter, std::optional<FieldChange<T>>& field)
    {
        field = changeSwapped<T>(field);
    }
};

/// Clears empty when any field has a change.
struct NoChanges
{
    bool empty;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char*, Getter, Setter, const std::optional<FieldChange<T>>& field)
    {
        empty = empty && !field.has_value();
    }
};

/// Appends a description line per changed field.
struct DescribeChanges
{
    const QString& firstName;
    QStringList& lines;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char*, const char* line, Getter, Setter, const std::optional<FieldChange<T>>& field)
    {
        if (field.has_value())
        {
            lines.append(QObject::tr(line).arg(firstName, valueText(field->after())));
        }
    }
};

/// Writes each changed field under its key.
struct WriteChanges
{
    QJsonObject& json;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char* key, const char*, Getter, Setter, const std::optional<FieldChange<T>>& field)
    {
        writeChange(json, key, field);
    }
};

/// Reads each field's change from under its key.
struct ReadChanges
{
    const QJsonObject& json;

    template<typename Getter, typename Setter, typename T>
    void operator()(const char* key, const char*, Getter, Setter, std::optional<FieldChange<T>>& field)
    {
        field = readChange<T>(json, key);
    }
};

}  // namespace

// ============================================================================
// MemberChange
// ============================================================================

std::optional<MemberChange> MemberChange::between(const Person& before, const Person& after)
{
    MemberChange change;
    ChangeBetween visitor{before, after};
    forEachField(change, visitor);
    if (change.isEmpty())
    {
        return std::nullopt;
    }
    return change;
}

bool MemberChange::holds(const Person& current) const
{
    ChangesHold visitor{current, true};
    forEachField(*this, visitor);
    return visitor.holds;
}

Person MemberChange::appliedTo(const Person& current) const
{
    Person updated = current;
    ApplyChanges visitor{updated};
    forEachField(*this, visitor);
    return updated;
}

MemberChange MemberChange::swapped() const
{
    MemberChange change = *this;
    SwapChanges visitor;
    forEachField(change, visitor);
    return change;
}

QStringList MemberChange::describe(const QString& firstName) const
{
    QStringList lines;
    DescribeChanges visitor{firstName, lines};
    forEachField(*this, visitor);
    return lines;
}

QJsonObject MemberChange::toJson() const
{
    QJsonObject json;
    WriteChanges visitor{json};
    forEachField(*this, visitor);
    return json;
}

MemberChange MemberChange::fromJson(const QJsonObject& json)
{
    MemberChange change;
    ReadChanges visitor{json};
    forEachField(change, visitor);
    return change;
}

bool MemberChange::isEmpty() const
{
    NoChanges visitor{true};
    forEachField(*this, visitor);
    return visitor.empty;
}

// ============================================================================
// FamilyChange
// ============================================================================

std::optional<FamilyChange> FamilyChange::between(const Family& opened, const Family& saved)
{
    QHash<PersonId, MemberChange> members;
    QList<PlacedMember> removed;
    for (qsizetype i = 0; i < opened.members().size(); ++i)
    {
        const Person& before = opened.members()[i];
        std::optional<Person> after = findMember(saved, before.id());
        if (!after.has_value())
        {
            removed.append(PlacedMember{before, i});
            continue;
        }
        std::optional<MemberChange> change = MemberChange::between(before, *after);
        if (change.has_value())
        {
            members.insert(before.id(), *change);
        }
    }

    QList<PlacedMember> added;
    for (qsizetype i = 0; i < saved.members().size(); ++i)
    {
        const Person& after = saved.members()[i];
        if (!findMember(opened, after.id()).has_value())
        {
            added.append(PlacedMember{after, i});
        }
    }

    std::optional<FieldChange<Address>> address = changeBetween(opened.address(), saved.address());
    std::optional<FieldChange<QString>> note = changeBetween(opened.note(), saved.note());
    if (!address.has_value() && !note.has_value() && members.isEmpty() && added.isEmpty() && removed.isEmpty())
    {
        return std::nullopt;
    }
    return FamilyChange(address, note, members, added, removed);
}

FamilyChange::FamilyChange(const std::optional<FieldChange<Address>>& address,
                           const std::optional<FieldChange<QString>>& note,
                           const QHash<PersonId, MemberChange>& members,
                           const QList<PlacedMember>& added,
                           const QList<PlacedMember>& removed)
    : m_address(address)
    , m_note(note)
    , m_members(members)
    , m_added(added)
    , m_removed(removed)
{
}

bool FamilyChange::holds(const Family& current) const
{
    if (!changeHolds(m_address, current.address()) || !changeHolds(m_note, current.note()))
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
    for (const PlacedMember& removed : m_removed)
    {
        std::optional<Person> member = findMember(current, removed.person.id());
        if (member.has_value() && *member != removed.person)
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
    for (const PlacedMember& added : m_added)
    {
        if (!findMember(current, added.person.id()).has_value())
        {
            members.insert(qMin(added.index, members.size()), added.person);
        }
    }

    Family updated = current;
    updated.setAddress(changedValue(m_address, current.address()));
    updated.setNote(changedValue(m_note, current.note()));
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
    return FamilyChange(changeSwapped(m_address), changeSwapped(m_note), members, m_removed, m_added);
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
    for (const PlacedMember& added : m_added)
    {
        lines.append(QObject::tr("added: %1").arg(added.person.displayName()));
    }
    for (const PlacedMember& removed : m_removed)
    {
        lines.append(QObject::tr("removed: %1").arg(removed.person.displayName()));
    }
    if (m_address.has_value())
    {
        lines.append(QObject::tr("address: %1").arg(orNone(m_address->after().full())));
    }
    if (m_note.has_value())
    {
        lines.append(QObject::tr("note: %1").arg(orNone(m_note->after())));
    }
    return lines;
}

QJsonObject FamilyChange::toJson() const
{
    QJsonObject json;
    writeChange(json, "address", m_address);
    writeChange(json, "note", m_note);
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
        json["added"] = placedToJson(m_added);
    }
    if (!m_removed.isEmpty())
    {
        json["removed"] = placedToJson(m_removed);
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
                        readChange<QString>(json, "note"),
                        members,
                        placedFromJson(json["added"].toArray()),
                        placedFromJson(json["removed"].toArray()));
}

// ============================================================================
// FamilyCollectionChange
// ============================================================================

FamilyCollectionChange FamilyCollectionChange::between(const Document& document,
                                                       const QHash<FamilyId, Family>& imported,
                                                       const QSet<FamilyId>& removed)
{
    QHash<FamilyId, Family> added;
    QHash<FamilyId, FamilyChange> changed;
    for (const Family& family : imported)
    {
        std::optional<Family> existing = document.findFamilyById(family.id());
        if (!existing.has_value())
        {
            added.insert(family.id(), family);
            continue;
        }
        std::optional<FamilyChange> change = FamilyChange::between(*existing, family);
        if (change.has_value())
        {
            changed.insert(family.id(), *change);
        }
    }

    QHash<FamilyId, Family> lastSeen;
    for (const FamilyId& id : removed)
    {
        std::optional<Family> existing = document.findFamilyById(id);
        if (existing.has_value())
        {
            lastSeen.insert(id, *existing);
        }
    }
    return FamilyCollectionChange(added, changed, lastSeen);
}

FamilyCollectionChange::FamilyCollectionChange(const QHash<FamilyId, Family>& added,
                                               const QHash<FamilyId, FamilyChange>& changed,
                                               const QHash<FamilyId, Family>& removed)
    : m_added(added)
    , m_changed(changed)
    , m_removed(removed)
{
}

bool FamilyCollectionChange::isEmpty() const
{
    return m_added.isEmpty()
        && m_changed.isEmpty()
        && m_removed.isEmpty();
}

bool FamilyCollectionChange::holds(const Document& current) const
{
    for (auto it = m_changed.constBegin(); it != m_changed.constEnd(); ++it)
    {
        std::optional<Family> family = current.findFamilyById(it.key());
        if (!family.has_value() || !it.value().holds(*family))
        {
            return false;
        }
    }
    for (const Family& removed : m_removed)
    {
        std::optional<Family> family = current.findFamilyById(removed.id());
        if (family.has_value() && *family != removed)
        {
            return false;
        }
    }
    return true;
}

void FamilyCollectionChange::applyTo(Document& document) const
{
    for (const Family& added : m_added)
    {
        if (!document.findFamilyById(added.id()).has_value())
        {
            document.setFamily(added);
        }
    }
    for (auto it = m_changed.constBegin(); it != m_changed.constEnd(); ++it)
    {
        document.setFamily(it.value().appliedTo(document.findFamilyById(it.key()).value()));
    }
    for (const Family& removed : m_removed)
    {
        document.removeFamily(removed.id());
    }
}

FamilyCollectionChange FamilyCollectionChange::swapped() const
{
    QHash<FamilyId, FamilyChange> changed;
    for (auto it = m_changed.constBegin(); it != m_changed.constEnd(); ++it)
    {
        changed.insert(it.key(), it.value().swapped());
    }
    return FamilyCollectionChange(m_removed, changed, m_added);
}

QList<Address> FamilyCollectionChange::addressesGiven() const
{
    QList<Address> addresses;
    for (const Family& added : m_added)
    {
        if (!added.address().isEmpty())
        {
            addresses.append(added.address());
        }
    }
    for (const FamilyChange& change : m_changed)
    {
        if (change.address().has_value() && !change.address()->after().isEmpty())
        {
            addresses.append(change.address()->after());
        }
    }
    return addresses;
}

QJsonObject FamilyCollectionChange::toJson() const
{
    QJsonObject json;
    if (!m_added.isEmpty())
    {
        json["added"] = familiesToJson(m_added);
    }
    if (!m_changed.isEmpty())
    {
        QJsonObject changed;
        for (auto it = m_changed.constBegin(); it != m_changed.constEnd(); ++it)
        {
            changed[it.key().toString()] = it.value().toJson();
        }
        json["changed"] = changed;
    }
    if (!m_removed.isEmpty())
    {
        json["removed"] = familiesToJson(m_removed);
    }
    return json;
}

FamilyCollectionChange FamilyCollectionChange::fromJson(const QJsonObject& json)
{
    QHash<FamilyId, FamilyChange> changed;
    QJsonObject changedJson = json["changed"].toObject();
    for (auto it = changedJson.constBegin(); it != changedJson.constEnd(); ++it)
    {
        changed.insert(FamilyId::fromString(it.key()), FamilyChange::fromJson(it.value().toObject()));
    }
    return FamilyCollectionChange(familiesFromJson(json["added"].toObject()),
                                  changed,
                                  familiesFromJson(json["removed"].toObject()));
}
