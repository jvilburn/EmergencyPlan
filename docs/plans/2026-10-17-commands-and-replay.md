# Commands and Replay Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Every write to a ward is an immutable, serialisable command that runs through one
commit step, enters an in-memory journal, replays onto another version of the document by the
one rule (apply where the start condition or the result holds, otherwise abandon the whole
replay to the to-redo list), and is undone by running its inverse through the same step.

**Architecture:** `Command` answers `Outcome::Applied` or `Outcome::Conflicted` from a `const`
`execute`, builds its `inverse`, serialises through `CommandRegistry`, and carries a stored
`Description`. Update commands hold typed `Change<T>` and `SetChange<Id>` values for the
fields they change only. `WardSession` owns the document, the `Journal`, the undo and redo
stacks and the to-redo list, with no global state; `DocumentManager` and `EmergencyManager`
become thin wrappers over the app's one session. The existing plaintext save and load path
stays as it is (plan 2 replaces it); push, fetch, landed marks and access-list commands are
plan 3, so replay is exercised against a second document version built in the test.

**Tech Stack:** Qt 6.11 (Core, Gui, Widgets, Test), C++17, MSVC 2022, CMake + Ninja via
`build.bat`, Qt Test.

**Design authority:** `docs/SHARED_WARD_DESIGN.md`. Section names in *italics* below are its
sections. Code sites come from `docs/SHARED_WARD_PLAN_NOTES.md`.

---

## Conventions for every task

- **The executor does not build.** The project's `CLAUDE.md` forbids running cmake or the app
  from this shell. The user builds with `build.bat` and runs the suite as
  `build\bin\WardPlanningTests.exe`. Each task's tests are written in that task and run when
  the user builds.
- **Core first.** Task 1 is the whole design-bearing core. Every later task is a consequence
  of it. Red between tasks, in the build and in the app, is expected.
- **Delete with the last caller**, in the same task.
- **Docs change in the task whose code they describe.**
- **Style:** `docs/CODING_STYLE.md`. Templates keep their bodies in headers because the
  compiler needs them there; that is the one place a header holds a multi-line function.
- **Commits:** one per task, the message given in the task, plain, no attribution lines.
  Before each commit, `git branch --show-current` prints the implementation branch.
- **Every new or changed command type** adds its entry to `CommandRegistry::types()` and its
  samples to `tests/services/CommandSamples.cpp` in the same task, so the round-trip test
  covers it.

---

## Task 1: The core

The command contract, the per-field change shape, the journal, and `WardSession`'s commit step,
replay, abandonment and undo, with `UpdateFamilyCommand` as the command that proves the shape,
and the test harness that runs all of it against test-built versions.

**Files:**
- Rewrite: `src/commands/Command.h`
- Create: `src/commands/Command.cpp`
- Create: `src/commands/Change.h`
- Create: `src/commands/ChangeJson.h`, `src/commands/ChangeJson.cpp`
- Create: `src/commands/FamilyChange.h`, `src/commands/FamilyChange.cpp`
- Rewrite: `src/commands/FamilyCommands.h`, `src/commands/FamilyCommands.cpp`
  (`AddFamilyCommand`, `DeleteFamilyCommand` and `SetFamiliesCommand` have no callers and are
  deleted; `UpdateFamilyCommand` is rewritten)
- Create: `src/commands/CommandRegistry.h`, `src/commands/CommandRegistry.cpp`
- Create: `src/services/Journal.h`, `src/services/Journal.cpp`
- Create: `src/services/WardSession.h`, `src/services/WardSession.cpp`
- Delete: `src/commands/CommandHistory.h`, `src/commands/CommandHistory.cpp` (the contract it
  ran, `execute` plus `undo`, is gone; `WardSession` holds the undo stack)
- Modify: `CMakeLists.txt` (source lists)
- Rewrite: `tests/CMakeLists.txt`
- Create: `tests/main.cpp`
- Create: `tests/services/TestWard.h`, `tests/services/TestWard.cpp`
- Create: `tests/services/TestCommands.h`, `tests/services/TestCommands.cpp`
- Create: `tests/services/CommandSamples.h`, `tests/services/CommandSamples.cpp`
- Create: `tests/services/ChangeTest.h`, `tests/services/ChangeTest.cpp`
- Create: `tests/services/UpdateFamilyCommandTest.h`, `tests/services/UpdateFamilyCommandTest.cpp`
- Create: `tests/services/CommandRoundTripTest.h`, `tests/services/CommandRoundTripTest.cpp`
- Create: `tests/services/WardSessionTest.h`, `tests/services/WardSessionTest.cpp`

### `src/commands/Command.h`

```cpp
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "DocumentChange.h"

class Document;

/// What running a command did to the version in front of it. A command whose result
/// already holds answers Applied too: nothing downstream tells the two apart.
enum class Outcome
{
    Applied,
    Conflicted
};

/// What the user did, in their words: a heading, and a line per value set. Rendered
/// through tr() when the command is built and stored with it, so it stays right after
/// the things it names are renamed or deleted.
class Description
{
public:
    Description() = default;
    Description(const QString& heading, const QStringList& lines);

    const QString& heading() const { return m_heading; }
    const QStringList& lines() const { return m_lines; }

    /// "Undo: <heading>" over the same lines, for an inverse.
    Description undone() const;

    QJsonObject toJson() const;
    static Description fromJson(const QJsonObject& json);

    bool operator==(const Description& other) const;
    bool operator!=(const Description& other) const { return !(*this == other); }

private:
    QString m_heading;
    QStringList m_lines;
};

class Command;
using CommandPtr = std::unique_ptr<Command>;

/// A write to the document, run the same way the first time and on replay.
///
/// A command is immutable once constructed. Its start condition (what the user saw), its
/// result and its description are fixed by the constructor; execute reads the document in
/// front of it and leaves the command as it was.
class Command
{
public:
    virtual ~Command() = default;

    /// The name this type has in the journal and in CommandRegistry.
    virtual QString type() const = 0;

    /// Applies when the start condition holds or the result already holds, and answers
    /// Applied. Otherwise answers Conflicted and leaves the document untouched.
    virtual Outcome execute(Document& document) const = 0;

    /// The command that takes this one back, described by description().undone().
    /// An empty pointer means this command is not undoable.
    virtual CommandPtr inverse() const = 0;

    /// The wording of the rule that refuses this command on this document, or an empty
    /// string when no rule does. Content commands have no rules.
    virtual QString refusal(const Document& document) const;

    /// This command's own fields. CommandRegistry adds the type and the description.
    virtual QJsonObject toJson() const = 0;

    /// What a run changes, for targeted view updates and cache rebuilds.
    virtual DocumentChange documentChange() const = 0;

    const Description& description() const { return m_description; }

protected:
    explicit Command(const Description& description);

private:
    Description m_description;
};
```

### `src/commands/Command.cpp`

```cpp
#include "Command.h"

#include <QJsonArray>
#include <QObject>

Description::Description(const QString& heading, const QStringList& lines)
    : m_heading(heading)
    , m_lines(lines)
{
}

Description Description::undone() const
{
    return Description(QObject::tr("Undo: %1").arg(m_heading), m_lines);
}

QJsonObject Description::toJson() const
{
    QJsonObject json;
    json["heading"] = m_heading;
    if (!m_lines.isEmpty())
    {
        json["lines"] = QJsonArray::fromStringList(m_lines);
    }
    return json;
}

Description Description::fromJson(const QJsonObject& json)
{
    QStringList lines;
    QJsonArray linesArray = json["lines"].toArray();
    for (const QJsonValue& value : linesArray)
    {
        lines.append(value.toString());
    }
    return Description(json["heading"].toString(), lines);
}

bool Description::operator==(const Description& other) const
{
    return m_heading == other.m_heading
        && m_lines == other.m_lines;
}

Command::Command(const Description& description)
    : m_description(description)
{
}

QString Command::refusal(const Document& document) const
{
    Q_UNUSED(document);
    return QString();
}
```

### `src/commands/Change.h`

```cpp
#pragma once

#include <QSet>
#include <optional>

/// One value a command changes, as the user saw it and as they set it.
///
/// The value holds for the command when it is at either end: at before, the command sets
/// it; at after, the command's result already holds there. Any other value means the
/// ground moved, and the command conflicts.
template<typename T>
class Change
{
public:
    Change(const T& before, const T& after) : m_before(before), m_after(after) {}

    const T& before() const { return m_before; }
    const T& after() const { return m_after; }

    bool holds(const T& current) const { return current == m_before || current == m_after; }
    Change swapped() const { return Change(m_after, m_before); }

private:
    T m_before;
    T m_after;
};

/// The change from before to after, or none when they are equal.
template<typename T>
std::optional<Change<T>> changeBetween(const T& before, const T& after) { return before == after ? std::nullopt : std::optional<Change<T>>(Change<T>(before, after)); }

/// True when there is no change to this value, or the value holds for its change.
template<typename T>
bool changeHolds(const std::optional<Change<T>>& change, const T& current) { return !change.has_value() || change->holds(current); }

/// The value the change sets, or current when there is no change to it.
template<typename T>
T changedValue(const std::optional<Change<T>>& change, const T& current) { return change.has_value() ? change->after() : current; }

template<typename T>
std::optional<Change<T>> changeSwapped(const std::optional<Change<T>>& change) { return change.has_value() ? std::optional<Change<T>>(change->swapped()) : std::nullopt; }

/// A change to a set of ids, held as the ids added and the ids removed. An addition is done
/// when the id is present and a removal when it is absent, so a set change holds whatever
/// the set holds, and two users adding different ids to one set both apply.
template<typename Id>
class SetChange
{
public:
    SetChange(const QSet<Id>& added, const QSet<Id>& removed) : m_added(added), m_removed(removed) {}

    static SetChange between(const QSet<Id>& before, const QSet<Id>& after) { return SetChange(after - before, before - after); }

    const QSet<Id>& added() const { return m_added; }
    const QSet<Id>& removed() const { return m_removed; }
    bool isEmpty() const { return m_added.isEmpty() && m_removed.isEmpty(); }

    QSet<Id> appliedTo(const QSet<Id>& current) const { return (current | m_added) - m_removed; }
    SetChange swapped() const { return SetChange(m_removed, m_added); }

private:
    QSet<Id> m_added;
    QSet<Id> m_removed;
};
```

`SetChange` is the shape every set field uses (team members, asset people, tag people and
families); its first command is `UpdateTeamCommand` in task 4, and `ChangeTest` covers it here.

### `src/commands/ChangeJson.h`

```cpp
#pragma once

#include "Address.h"
#include "Birthday.h"
#include "Change.h"
#include "Gender.h"
#include "Name.h"
#include "Phone.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <optional>

/// JSON for the values commands change. Each value reads back through the reader for its
/// own type, so a change's before and after round-trip exactly.
QJsonValue toJsonValue(const QString& value);
QJsonValue toJsonValue(bool value);
QJsonValue toJsonValue(const QStringList& value);
QJsonValue toJsonValue(const Address& value);
QJsonValue toJsonValue(const Birthday& value);
QJsonValue toJsonValue(const std::optional<Gender>& value);

template<typename T>
T fromJsonValue(const QJsonValue& value);

template<>
QString fromJsonValue<QString>(const QJsonValue& value);
template<>
Name fromJsonValue<Name>(const QJsonValue& value);
template<>
Phone fromJsonValue<Phone>(const QJsonValue& value);
template<>
bool fromJsonValue<bool>(const QJsonValue& value);
template<>
QStringList fromJsonValue<QStringList>(const QJsonValue& value);
template<>
Address fromJsonValue<Address>(const QJsonValue& value);
template<>
Birthday fromJsonValue<Birthday>(const QJsonValue& value);
template<>
std::optional<Gender> fromJsonValue<std::optional<Gender>>(const QJsonValue& value);

template<typename T>
QJsonObject changeToJson(const Change<T>& change)
{
    QJsonObject json;
    json["before"] = toJsonValue(change.before());
    json["after"] = toJsonValue(change.after());
    return json;
}

template<typename T>
Change<T> changeFromJson(const QJsonObject& json)
{
    return Change<T>(fromJsonValue<T>(json["before"]), fromJsonValue<T>(json["after"]));
}

/// Writes the change under key, or nothing when there is no change.
template<typename T>
void writeChange(QJsonObject& json, const QString& key, const std::optional<Change<T>>& change)
{
    if (change.has_value())
    {
        json[key] = changeToJson(*change);
    }
}

/// The change under key, or none when the key is absent.
template<typename T>
std::optional<Change<T>> readChange(const QJsonObject& json, const QString& key)
{
    if (!json.contains(key))
    {
        return std::nullopt;
    }
    return changeFromJson<T>(json[key].toObject());
}
```

### `src/commands/ChangeJson.cpp`

```cpp
#include "ChangeJson.h"

#include <QJsonArray>

QJsonValue toJsonValue(const QString& value)
{
    return QJsonValue(value);
}

QJsonValue toJsonValue(bool value)
{
    return QJsonValue(value);
}

QJsonValue toJsonValue(const QStringList& value)
{
    return QJsonArray::fromStringList(value);
}

QJsonValue toJsonValue(const Address& value)
{
    return QJsonArray::fromStringList(value.lines());
}

QJsonValue toJsonValue(const Birthday& value)
{
    return value.toJson();
}

QJsonValue toJsonValue(const std::optional<Gender>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

template<>
QString fromJsonValue<QString>(const QJsonValue& value)
{
    return value.toString();
}

template<>
Name fromJsonValue<Name>(const QJsonValue& value)
{
    return Name(value.toString());
}

template<>
Phone fromJsonValue<Phone>(const QJsonValue& value)
{
    return Phone(value.toString());
}

template<>
bool fromJsonValue<bool>(const QJsonValue& value)
{
    return value.toBool();
}

template<>
QStringList fromJsonValue<QStringList>(const QJsonValue& value)
{
    QStringList strings;
    QJsonArray array = value.toArray();
    for (const QJsonValue& item : array)
    {
        strings.append(item.toString());
    }
    return strings;
}

template<>
Address fromJsonValue<Address>(const QJsonValue& value)
{
    Address address;
    QJsonArray lines = value.toArray();
    for (const QJsonValue& line : lines)
    {
        address.addLine(line.toString());
    }
    return address;
}

template<>
Birthday fromJsonValue<Birthday>(const QJsonValue& value)
{
    return Birthday::fromJson(value.toObject());
}

template<>
std::optional<Gender> fromJsonValue<std::optional<Gender>>(const QJsonValue& value)
{
    return Gender::fromJson(value);
}
```

### `src/commands/FamilyChange.h`

```cpp
#pragma once

#include "Change.h"
#include "Family.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <optional>

/// The fields of one member that an edit changes, each judged on its own: one still at its
/// before value is set, one already at its after value is left, and one at anything else
/// makes the whole command a conflict.
class MemberChange
{
public:
    MemberChange() = default;

    /// The fields that differ between the member as opened and as saved, or none when
    /// nothing differs.
    static std::optional<MemberChange> between(const Person& before, const Person& after);

    bool holds(const Person& current) const;
    Person appliedTo(const Person& current) const;
    MemberChange swapped() const;

    /// A line per field set, naming the member by firstName.
    QStringList describe(const QString& firstName) const;

    QJsonObject toJson() const;
    static MemberChange fromJson(const QJsonObject& json);

private:
    bool isEmpty() const;

    std::optional<Change<Name>> m_name;
    std::optional<Change<std::optional<Gender>>> m_gender;
    std::optional<Change<Phone>> m_phone;
    std::optional<Change<Phone>> m_altPhone;
    std::optional<Change<QString>> m_email;
    std::optional<Change<Birthday>> m_birthday;
    std::optional<Change<QStringList>> m_callings;
    std::optional<Change<bool>> m_isParent;
    std::optional<Change<QString>> m_wardUnitNumber;
    std::optional<Change<QString>> m_stakeUnitNumber;
    std::optional<Change<QString>> m_specialNeedNote;
};

/// What an edit changes in one family: its address, a change per member keyed by PersonId,
/// the members it adds, and the members it removes as last seen. Members are matched by
/// PersonId, and their order is not part of any start condition.
class FamilyChange
{
public:
    /// The difference between the family as opened and as saved, or none when nothing
    /// differs.
    static std::optional<FamilyChange> between(const Family& opened, const Family& saved);

    FamilyChange(const std::optional<Change<Address>>& address,
                 const QHash<PersonId, MemberChange>& members,
                 const QList<Person>& added,
                 const QList<Person>& removed);

    /// The start condition or the result holds: the address and every changed member are at
    /// before or after, and every removed member is gone or exactly as last seen. An added
    /// member never conflicts; its result holds when that PersonId is already there.
    bool holds(const Family& current) const;
    Family appliedTo(const Family& current) const;
    FamilyChange swapped() const;

    /// The lines of what to do again: member lines in the opened family's member order,
    /// then additions, removals and the address.
    QStringList describe(const Family& opened) const;

    QJsonObject toJson() const;
    static FamilyChange fromJson(const QJsonObject& json);

private:
    std::optional<Change<Address>> m_address;
    QHash<PersonId, MemberChange> m_members;
    QList<Person> m_added;
    QList<Person> m_removed;
};
```

### `src/commands/FamilyChange.cpp`

```cpp
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
```

### `src/commands/FamilyCommands.h`

```cpp
#pragma once

#include "Command.h"
#include "FamilyChange.h"

#include <memory>

/// An edit to one family, built from the difference between what the edit opened with and
/// what it saved. Holds the before and after of the changed fields only.
class UpdateFamilyCommand : public Command
{
public:
    static constexpr const char* TypeName = "updateFamily";

    /// The edit from the family as opened to the family as saved, or an empty pointer when
    /// nothing differs.
    static std::unique_ptr<UpdateFamilyCommand> fromEdit(const Family& opened, const Family& saved);

    UpdateFamilyCommand(const FamilyId& familyId, const FamilyChange& change, const Description& description);

    QString type() const override { return TypeName; }
    Outcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const Description& description);

private:
    FamilyId m_familyId;
    FamilyChange m_change;
};
```

### `src/commands/FamilyCommands.cpp`

```cpp
#include "FamilyCommands.h"
#include "Document.h"

#include <QObject>

std::unique_ptr<UpdateFamilyCommand> UpdateFamilyCommand::fromEdit(const Family& opened, const Family& saved)
{
    std::optional<FamilyChange> change = FamilyChange::between(opened, saved);
    if (!change.has_value())
    {
        return nullptr;
    }
    Description description(QObject::tr("%1 family").arg(opened.surname()), change->describe(opened));
    return std::make_unique<UpdateFamilyCommand>(opened.id(), *change, description);
}

UpdateFamilyCommand::UpdateFamilyCommand(const FamilyId& familyId,
                                         const FamilyChange& change,
                                         const Description& description)
    : Command(description)
    , m_familyId(familyId)
    , m_change(change)
{
}

Outcome UpdateFamilyCommand::execute(Document& document) const
{
    std::optional<Family> family = document.findFamilyById(m_familyId);
    if (!family.has_value() || !m_change.holds(*family))
    {
        return Outcome::Conflicted;
    }
    document.updateFamily(m_change.appliedTo(*family));
    return Outcome::Applied;
}

CommandPtr UpdateFamilyCommand::inverse() const
{
    return std::make_unique<UpdateFamilyCommand>(m_familyId, m_change.swapped(), description().undone());
}

QJsonObject UpdateFamilyCommand::toJson() const
{
    QJsonObject json;
    json["familyId"] = m_familyId.toString();
    json["change"] = m_change.toJson();
    return json;
}

DocumentChange UpdateFamilyCommand::documentChange() const
{
    return DocumentChange::family().updated(m_familyId);
}

CommandPtr UpdateFamilyCommand::fromJson(const QJsonObject& json, const Description& description)
{
    return std::make_unique<UpdateFamilyCommand>(FamilyId::fromString(json["familyId"].toString()),
                                                 FamilyChange::fromJson(json["change"].toObject()),
                                                 description);
}
```

### `src/commands/CommandRegistry.h`

```cpp
#pragma once

#include "Command.h"

#include <QJsonObject>
#include <QList>
#include <QString>

/// A command type the journal can rebuild.
struct CommandType
{
    QString name;
    CommandPtr (*fromJson)(const QJsonObject& json, const Description& description);
};

/// The command types, and the serialised form a command takes in the journal.
class CommandRegistry
{
public:
    /// Every command type, each listed once by its TypeName.
    static const QList<CommandType>& types();

    /// A command as the journal holds it: its type, its description and its fields.
    static QJsonObject toJson(const Command& command);

    /// The command a journal entry holds, rebuilt as the command it was.
    static CommandPtr fromJson(const QJsonObject& json);

    /// The description a journal entry holds, read without rebuilding the command.
    static Description descriptionOf(const QJsonObject& json);
};
```

### `src/commands/CommandRegistry.cpp`

```cpp
#include "CommandRegistry.h"
#include "FamilyCommands.h"

const QList<CommandType>& CommandRegistry::types()
{
    static const QList<CommandType> types = {
        {UpdateFamilyCommand::TypeName, &UpdateFamilyCommand::fromJson},
    };
    return types;
}

QJsonObject CommandRegistry::toJson(const Command& command)
{
    QJsonObject json;
    json["type"] = command.type();
    json["description"] = command.description().toJson();
    json["fields"] = command.toJson();
    return json;
}

CommandPtr CommandRegistry::fromJson(const QJsonObject& json)
{
    QString name = json["type"].toString();
    for (const CommandType& type : types())
    {
        if (type.name == name)
        {
            return type.fromJson(json["fields"].toObject(), descriptionOf(json));
        }
    }
    Q_ASSERT_X(false, "CommandRegistry::fromJson", "a journal entry names a type the registry lacks");
    return nullptr;
}

Description CommandRegistry::descriptionOf(const QJsonObject& json)
{
    return Description::fromJson(json["description"].toObject());
}
```

### `src/services/Journal.h`

```cpp
#pragma once

#include "Command.h"

#include <QJsonObject>
#include <QList>

/// One command this device ran, serialised once when it entered the journal and kept as
/// it is.
struct JournalEntry
{
    qint64 sequence;
    QJsonObject command;
};

/// This device's commands that it has not yet seen land, in the order they were made. Each
/// entry takes the next sequence number from a counter kept with the journal.
class Journal
{
public:
    Journal() = default;

    /// Appends the command's serialised form under the next sequence number.
    void append(const Command& command);

    /// Empties the journal. The sequence counter keeps its value, so entries made after
    /// number above every entry made before.
    void clear();

    bool isEmpty() const { return m_entries.isEmpty(); }
    const QList<JournalEntry>& entries() const { return m_entries; }

    /// The entries' descriptions, in order.
    QList<Description> descriptions() const;

private:
    QList<JournalEntry> m_entries;
    qint64 m_lastSequence = 0;
};
```

### `src/services/Journal.cpp`

```cpp
#include "Journal.h"
#include "CommandRegistry.h"

void Journal::append(const Command& command)
{
    ++m_lastSequence;
    m_entries.append(JournalEntry{m_lastSequence, CommandRegistry::toJson(command)});
}

void Journal::clear()
{
    m_entries.clear();
}

QList<Description> Journal::descriptions() const
{
    QList<Description> descriptions;
    for (const JournalEntry& entry : m_entries)
    {
        descriptions.append(CommandRegistry::descriptionOf(entry.command));
    }
    return descriptions;
}
```

### `src/services/WardSession.h`

```cpp
#pragma once

#include "Command.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Journal.h"

#include <QList>
#include <QObject>
#include <QString>
#include <vector>

/// How the commit step ended for one command.
enum class CommitStatus
{
    Applied,
    Conflicted,
    Refused
};

struct CommitResult
{
    CommitStatus status;
    QString refusal;  ///< the rule's wording, when status is Refused
};

/// How a replay onto a version ended.
enum class ReplayStatus
{
    FastForwarded,  ///< the journal was empty, and the version was taken as it stands
    Replayed,       ///< every entry applied on top of the version
    Abandoned       ///< one entry stopped it, and the version was taken as it stands
};

/// The descriptions of one abandoned journal, kept for the user to work from.
struct RedoGroup
{
    QList<Description> descriptions;
    int clashed;        ///< index in descriptions of the entry that stopped the replay
    CommitResult stop;  ///< Conflicted, or Refused with the rule's wording
};

/// One open ward: its document, journal, undo stack and to-redo list, and the commit step
/// and replay that change them. Holds no global state, so a test creates as many sessions
/// as it needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    explicit WardSession(QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_journal; }
    const QList<RedoGroup>& toRedo() const { return m_toRedo; }

    /// Opens a ward: its document replaces the open one by assignment, and the journal, the
    /// to-redo list and the undo stack start empty.
    void open(const Document& document);

    /// The commit step: the command's rule, then execute, then the journal. A command that
    /// applies clears the redo side, and enters the undo stack when it has an inverse. One
    /// that conflicts or is refused changes nothing and enters neither.
    CommitResult commit(CommandPtr command);

    /// Runs the inverse of the latest undoable command through the commit step. When it
    /// applies, the command moves to the redo side; otherwise it stays where it is.
    CommitResult undo();

    /// Runs the latest undone command again through the commit step. When it applies, the
    /// command moves back to the undo side.
    CommitResult redo();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    QString undoHeading() const;
    QString redoHeading() const;

    /// Replays the journal onto version. An empty journal takes the version as it stands.
    /// Otherwise each entry is rebuilt from its serialised form and put through the rule and
    /// execute, in order, on a copy of the version; when every entry applies, the copy
    /// becomes the document. One conflict or refusal abandons the whole replay: the version
    /// is taken as it stands, the journal's descriptions become a group on the to-redo list,
    /// and the journal and the undo stack are emptied.
    ReplayStatus replayOnto(const Document& version);

    /// Empties the to-redo list, once the user has dismissed it.
    void dismissToRedo();

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

private:
    /// A command on the undo or redo side, with the inverse built when it was committed.
    struct UndoEntry
    {
        CommandPtr command;
        CommandPtr inverse;
    };

    CommitResult run(const Command& command);
    void abandon(const Document& version, int clashed, const CommitResult& stop);
    void replaceDocument(const Document& document);
    void clearUndoStack();

    Document m_document;
    Journal m_journal;
    std::vector<UndoEntry> m_undoStack;
    std::vector<UndoEntry> m_redoStack;
    QList<RedoGroup> m_toRedo;

    static constexpr int UndoLimit = 100;
};
```

### `src/services/WardSession.cpp`

```cpp
#include "WardSession.h"
#include "CommandRegistry.h"

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal};
    }
    if (command.execute(document) == Outcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString()};
}

}  // namespace

WardSession::WardSession(QObject* parent)
    : QObject(parent)
{
}

void WardSession::open(const Document& document)
{
    m_journal = Journal();
    m_toRedo.clear();
    clearUndoStack();
    replaceDocument(document);
}

CommitResult WardSession::commit(CommandPtr command)
{
    CommitResult result = run(*command);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }

    m_redoStack.clear();
    CommandPtr inverse = command->inverse();
    if (inverse)
    {
        m_undoStack.push_back(UndoEntry{std::move(command), std::move(inverse)});
        if (static_cast<int>(m_undoStack.size()) > UndoLimit)
        {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    emit undoStackChanged();
    return result;
}

CommitResult WardSession::undo()
{
    Q_ASSERT(canUndo());
    CommitResult result = run(*m_undoStack.back().inverse);
    if (result.status == CommitStatus::Applied)
    {
        m_redoStack.push_back(std::move(m_undoStack.back()));
        m_undoStack.pop_back();
        emit undoStackChanged();
    }
    return result;
}

CommitResult WardSession::redo()
{
    Q_ASSERT(canRedo());
    CommitResult result = run(*m_redoStack.back().command);
    if (result.status == CommitStatus::Applied)
    {
        m_undoStack.push_back(std::move(m_redoStack.back()));
        m_redoStack.pop_back();
        emit undoStackChanged();
    }
    return result;
}

QString WardSession::undoHeading() const
{
    if (m_undoStack.empty())
    {
        return QString();
    }
    return m_undoStack.back().command->description().heading();
}

QString WardSession::redoHeading() const
{
    if (m_redoStack.empty())
    {
        return QString();
    }
    return m_redoStack.back().command->description().heading();
}

ReplayStatus WardSession::replayOnto(const Document& version)
{
    if (m_journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayStatus::FastForwarded;
    }

    Document replayed = version;
    replayed.onDocumentChanged(DocumentChange::full());
    const QList<JournalEntry>& entries = m_journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(entries[i].command);
        CommitResult result = attempt(*command, replayed);
        if (result.status != CommitStatus::Applied)
        {
            abandon(version, i, result);
            return ReplayStatus::Abandoned;
        }
    }
    replaceDocument(replayed);
    return ReplayStatus::Replayed;
}

void WardSession::dismissToRedo()
{
    m_toRedo.clear();
}

CommitResult WardSession::run(const Command& command)
{
    CommitResult result = attempt(command, m_document);
    if (result.status == CommitStatus::Applied)
    {
        m_journal.append(command);
        emit documentChanged(command.documentChange());
    }
    return result;
}

void WardSession::abandon(const Document& version, int clashed, const CommitResult& stop)
{
    m_toRedo.append(RedoGroup{m_journal.descriptions(), clashed, stop});
    m_journal.clear();
    clearUndoStack();
    replaceDocument(version);
}

void WardSession::replaceDocument(const Document& document)
{
    m_document = document;
    m_document.onDocumentChanged(DocumentChange::full());
    emit documentChanged(DocumentChange::full());
}

void WardSession::clearUndoStack()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStackChanged();
}
```

### `CMakeLists.txt`

In `COMMAND_SOURCES` replace `src/commands/CommandHistory.cpp` with:

```cmake
    src/commands/Command.cpp
    src/commands/ChangeJson.cpp
    src/commands/FamilyChange.cpp
    src/commands/CommandRegistry.cpp
```

In `COMMAND_HEADERS` replace `src/commands/CommandHistory.h` with:

```cmake
    src/commands/Change.h
    src/commands/ChangeJson.h
    src/commands/FamilyChange.h
    src/commands/CommandRegistry.h
```

Add `src/services/Journal.cpp` and `src/services/WardSession.cpp` to `SERVICE_SOURCES`, and
`src/services/Journal.h` and `src/services/WardSession.h` to `SERVICE_HEADERS`.

### `tests/CMakeLists.txt`

```cmake
# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(WardPlanningTests
    main.cpp
    services/TestWard.h
    services/TestWard.cpp
    services/TestCommands.h
    services/TestCommands.cpp
    services/CommandSamples.h
    services/CommandSamples.cpp
    services/ChangeTest.h
    services/ChangeTest.cpp
    services/UpdateFamilyCommandTest.h
    services/UpdateFamilyCommandTest.cpp
    services/CommandRoundTripTest.h
    services/CommandRoundTripTest.cpp
    services/WardSessionTest.h
    services/WardSessionTest.cpp
)

target_include_directories(WardPlanningTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(WardPlanningTests PRIVATE
    EmergencyPlanLib
    Qt6::Test
)

add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
```

### `tests/main.cpp`

```cpp
#include "ChangeTest.h"
#include "CommandRoundTripTest.h"
#include "UpdateFamilyCommandTest.h"
#include "WardSessionTest.h"

#include <QApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    ChangeTest changeTest(nullptr);
    UpdateFamilyCommandTest updateFamilyCommandTest(nullptr);
    CommandRoundTripTest commandRoundTripTest(nullptr);
    WardSessionTest wardSessionTest(nullptr);

    int failures = 0;
    failures += QTest::qExec(&changeTest, argc, argv);
    failures += QTest::qExec(&updateFamilyCommandTest, argc, argv);
    failures += QTest::qExec(&commandRoundTripTest, argc, argv);
    failures += QTest::qExec(&wardSessionTest, argc, argv);
    return failures;
}
```

Enums, `Document` and `QJsonObject` are compared with `QVERIFY(a == b)` throughout, since the
suite defines no `QTest::toString` for them; `QCOMPARE` is used for strings, string lists and
integers.

### `tests/services/TestWard.h`

```cpp
#pragma once

#include "Document.h"
#include "Family.h"
#include "Person.h"

/// A small ward with fixed ids, and helpers for building versions of it.
///
/// The Smith family at 12 Elm St, Provo, UT: Bob (555-1111, bob@example.com) and Alice
/// (555-3333, alice@example.com), both parents. The Jones family at 4 Oak Ave, Provo, UT:
/// Carol (555-4444, carol@example.com), a parent.
class TestWard
{
public:
    static FamilyId smithId() { return FamilyId::fromString("family-smith"); }
    static FamilyId jonesId() { return FamilyId::fromString("family-jones"); }
    static PersonId bobId() { return PersonId::fromString("person-bob"); }
    static PersonId aliceId() { return PersonId::fromString("person-alice"); }
    static PersonId carolId() { return PersonId::fromString("person-carol"); }

    static Person bob();
    static Person alice();
    static Person carol();
    static Family smith();
    static Family jones();
    static Address address(const QString& street, const QString& city);

    /// The ward holding the Smith and Jones families, with its lookup caches built.
    static Document document();

    /// family with the member of the same id replaced by member.
    static Family withMember(const Family& family, const Person& member);

    /// family with that member's phone set to phone.
    static Family withPhone(const Family& family, const PersonId& id, const QString& phone);

    /// document with the family of the same id replaced by family, caches rebuilt.
    static Document withFamily(const Document& document, const Family& family);

    /// The family with that id; throws std::bad_optional_access when it is absent.
    static Family family(const Document& document, const FamilyId& id);

    /// The person with that id; throws std::bad_optional_access when it is absent.
    static Person person(const Document& document, const PersonId& id);
};
```

### `tests/services/TestWard.cpp`

```cpp
#include "TestWard.h"

Person TestWard::bob()
{
    return Person::createWithId(bobId(), Name("Smith", "Bob"), Gender::Male, Birthday(),
                                Phone("555-1111"), Phone(), "bob@example.com", QStringList(),
                                true, QString(), QString());
}

Person TestWard::alice()
{
    return Person::createWithId(aliceId(), Name("Smith", "Alice"), Gender::Female, Birthday(),
                                Phone("555-3333"), Phone(), "alice@example.com", QStringList(),
                                true, QString(), QString());
}

Person TestWard::carol()
{
    return Person::createWithId(carolId(), Name("Jones", "Carol"), Gender::Female, Birthday(),
                                Phone("555-4444"), Phone(), "carol@example.com", QStringList(),
                                true, QString(), QString());
}

Address TestWard::address(const QString& street, const QString& city)
{
    Address address;
    address.addLine(street);
    address.addLine(city);
    return address;
}

Family TestWard::smith()
{
    return Family::createWithId(smithId(), std::nullopt, std::nullopt,
                                address("12 Elm St", "Provo, UT"), {bob(), alice()});
}

Family TestWard::jones()
{
    return Family::createWithId(jonesId(), std::nullopt, std::nullopt,
                                address("4 Oak Ave", "Provo, UT"), {carol()});
}

Document TestWard::document()
{
    Document document;
    document.addFamily(smith());
    document.addFamily(jones());
    document.onDocumentChanged(DocumentChange::full());
    return document;
}

Family TestWard::withMember(const Family& family, const Person& member)
{
    QList<Person> members = family.members();
    for (Person& existing : members)
    {
        if (existing.id() == member.id())
        {
            existing = member;
        }
    }
    Family updated = family;
    updated.setMembers(members);
    return updated;
}

Family TestWard::withPhone(const Family& family, const PersonId& id, const QString& phone)
{
    for (const Person& member : family.members())
    {
        if (member.id() == id)
        {
            Person updated = member;
            updated.setPhone(Phone(phone));
            return withMember(family, updated);
        }
    }
    Q_ASSERT_X(false, "TestWard::withPhone", "the family has no member with that id");
    return family;
}

Document TestWard::withFamily(const Document& document, const Family& family)
{
    Document updated = document;
    updated.updateFamily(family);
    updated.onDocumentChanged(DocumentChange::full());
    return updated;
}

Family TestWard::family(const Document& document, const FamilyId& id)
{
    return document.findFamilyById(id).value();
}

Person TestWard::person(const Document& document, const PersonId& id)
{
    return document.findPersonById(id).value();
}
```

Task 3 drops the two `std::nullopt` coordinate arguments when `Family` loses its coordinates.

### `tests/services/TestCommands.h`

```cpp
#pragma once

#include "Command.h"
#include "Id.h"

/// A test command with no inverse that sets a member's phone whatever it holds, refused
/// with ruleWording when that is not empty. It stands in for a command with a rule, which
/// no content command has, and for a write the undo stack does not hold. It is not in
/// CommandRegistry, so it never takes part in a replay.
class ForcePhoneCommand : public Command
{
public:
    ForcePhoneCommand(const PersonId& personId, const QString& phone, const QString& ruleWording);

    QString type() const override { return "testForcePhone"; }
    Outcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

private:
    PersonId m_personId;
    QString m_phone;
    QString m_ruleWording;
};
```

### `tests/services/TestCommands.cpp`

```cpp
#include "TestCommands.h"
#include "Document.h"
#include "TestWard.h"

ForcePhoneCommand::ForcePhoneCommand(const PersonId& personId, const QString& phone, const QString& ruleWording)
    : Command(Description("Force phone", QStringList()))
    , m_personId(personId)
    , m_phone(phone)
    , m_ruleWording(ruleWording)
{
}

Outcome ForcePhoneCommand::execute(Document& document) const
{
    FamilyId familyId = document.familyIdForPerson(m_personId).value();
    Family family = TestWard::family(document, familyId);
    document.updateFamily(TestWard::withPhone(family, m_personId, m_phone));
    return Outcome::Applied;
}

CommandPtr ForcePhoneCommand::inverse() const
{
    return nullptr;
}

QString ForcePhoneCommand::refusal(const Document& document) const
{
    Q_UNUSED(document);
    return m_ruleWording;
}

QJsonObject ForcePhoneCommand::toJson() const
{
    QJsonObject json;
    json["personId"] = m_personId.toString();
    json["phone"] = m_phone;
    return json;
}

DocumentChange ForcePhoneCommand::documentChange() const
{
    return DocumentChange::full();
}
```

### `tests/services/CommandSamples.h`

```cpp
#pragma once

#include "Command.h"
#include "Document.h"

#include <QString>
#include <vector>

/// Which outcome a sample's document gives its command.
enum class SampleCase
{
    StartHolds,   ///< applies and changes the document
    ResultHolds,  ///< applies and leaves the document as it was
    Conflicts     ///< conflicts and leaves the document as it was
};

struct CommandSample
{
    SampleCase sampleCase;
    Document document;
    CommandPtr command;
};

/// Samples of each registered command type, one per outcome the type can give.
class CommandSamples
{
public:
    /// The samples for the type with that name; empty for a type with none.
    static std::vector<CommandSample> forType(const QString& type);
};
```

### `tests/services/CommandSamples.cpp`

```cpp
#include "CommandSamples.h"
#include "FamilyCommands.h"
#include "TestWard.h"

namespace
{

/// The Smith family with every field of Bob changed, Alice removed, Dan added and the
/// address changed, so the sample round-trips every value type a family edit holds.
Family smithWithEveryFieldChanged()
{
    Person bob = TestWard::bob();
    bob.setName(Name("Smith", "Robert"));
    bob.setGender(std::nullopt);
    bob.setPhone(Phone("555-2222"));
    bob.setAltPhone(Phone("555-2223"));
    bob.setEmail("robert@example.com");
    bob.setBirthday(Birthday::create(1980, 7, 4));
    bob.setCallings({"Clerk"});
    bob.setIsParent(false);
    bob.setWardUnitNumber("123456");
    bob.setStakeUnitNumber("654321");
    bob.setSpecialNeedNote("Uses a wheelchair; the ramp is at the back door.");

    Person dan = Person::createWithId(PersonId::fromString("person-dan"), Name("Smith", "Dan"),
                                      Gender::Male, Birthday(), Phone(), Phone(), QString(),
                                      QStringList(), false, QString(), QString());

    Family saved = TestWard::smith();
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));
    saved.setMembers({bob, dan});
    return saved;
}

std::vector<CommandSample> updateFamilySamples()
{
    Family opened = TestWard::smith();
    Family saved = smithWithEveryFieldChanged();
    Family moved = TestWard::withPhone(opened, TestWard::bobId(), "555-9999");

    std::vector<CommandSample> samples;
    samples.push_back(CommandSample{SampleCase::StartHolds,
                                    TestWard::document(),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.push_back(CommandSample{SampleCase::ResultHolds,
                                    TestWard::withFamily(TestWard::document(), saved),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.push_back(CommandSample{SampleCase::Conflicts,
                                    TestWard::withFamily(TestWard::document(), moved),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    return samples;
}

}  // namespace

std::vector<CommandSample> CommandSamples::forType(const QString& type)
{
    if (type == UpdateFamilyCommand::TypeName)
    {
        return updateFamilySamples();
    }
    return {};
}
```

### `tests/services/ChangeTest.h`

```cpp
#pragma once

#include <QObject>

class ChangeTest : public QObject
{
    Q_OBJECT

public:
    explicit ChangeTest(QObject* parent);

private slots:
    void changeHoldsAtBeforeOrAfterOnly();
    void changeSwappedExchangesItsEnds();
    void changeBetweenEqualValuesIsNone();
    void setChangeBetweenHoldsTheIdsAddedAndRemoved();
    void setChangeAdditionPresentAndRemovalAbsentAreDone();
    void setChangesAddingDifferentIdsBothLand();
    void setChangeSwappedTakesItselfBack();
};
```

### `tests/services/ChangeTest.cpp`

```cpp
#include "ChangeTest.h"
#include "Change.h"
#include "Id.h"

#include <QTest>

namespace
{

PersonId a() { return PersonId::fromString("a"); }
PersonId b() { return PersonId::fromString("b"); }
PersonId c() { return PersonId::fromString("c"); }

}  // namespace

ChangeTest::ChangeTest(QObject* parent)
    : QObject(parent)
{
}

void ChangeTest::changeHoldsAtBeforeOrAfterOnly()
{
    Change<QString> change("555-1111", "555-2222");
    QVERIFY(change.holds("555-1111"));
    QVERIFY(change.holds("555-2222"));
    QVERIFY(!change.holds("555-9999"));
    QVERIFY(!change.holds(QString()));
}

void ChangeTest::changeSwappedExchangesItsEnds()
{
    Change<QString> swapped = Change<QString>("555-1111", "555-2222").swapped();
    QCOMPARE(swapped.before(), QString("555-2222"));
    QCOMPARE(swapped.after(), QString("555-1111"));
}

void ChangeTest::changeBetweenEqualValuesIsNone()
{
    QVERIFY(!changeBetween(QString("x"), QString("x")).has_value());
    std::optional<Change<QString>> change = changeBetween(QString("x"), QString("y"));
    QVERIFY(change.has_value());
    QCOMPARE(change->before(), QString("x"));
    QCOMPARE(change->after(), QString("y"));
}

void ChangeTest::setChangeBetweenHoldsTheIdsAddedAndRemoved()
{
    SetChange<PersonId> change = SetChange<PersonId>::between({a(), b()}, {b(), c()});
    QVERIFY(change.added() == QSet<PersonId>({c()}));
    QVERIFY(change.removed() == QSet<PersonId>({a()}));
}

void ChangeTest::setChangeAdditionPresentAndRemovalAbsentAreDone()
{
    SetChange<PersonId> change({c()}, {a()});
    QVERIFY(change.appliedTo({b(), c()}) == QSet<PersonId>({b(), c()}));
    QVERIFY(change.appliedTo({a(), b()}) == QSet<PersonId>({b(), c()}));
}

void ChangeTest::setChangesAddingDifferentIdsBothLand()
{
    SetChange<PersonId> mine = SetChange<PersonId>::between({a()}, {a(), b()});
    SetChange<PersonId> theirs = SetChange<PersonId>::between({a()}, {a(), c()});
    QVERIFY(mine.appliedTo(theirs.appliedTo({a()})) == QSet<PersonId>({a(), b(), c()}));
    QVERIFY(theirs.appliedTo(mine.appliedTo({a()})) == QSet<PersonId>({a(), b(), c()}));
}

void ChangeTest::setChangeSwappedTakesItselfBack()
{
    SetChange<PersonId> change = SetChange<PersonId>::between({a(), b()}, {b(), c()});
    QSet<PersonId> changed = change.appliedTo({a(), b()});
    QVERIFY(changed == QSet<PersonId>({b(), c()}));
    QVERIFY(change.swapped().appliedTo(changed) == QSet<PersonId>({a(), b()}));
}
```

### `tests/services/UpdateFamilyCommandTest.h`

```cpp
#pragma once

#include <QObject>

class UpdateFamilyCommandTest : public QObject
{
    Q_OBJECT

public:
    explicit UpdateFamilyCommandTest(QObject* parent);

private slots:
    void editHoldsOnlyTheFieldsThatDiffer();
    void editThatChangesNothingIsNoCommand();
    void descriptionIsAHeadingAndALinePerValueSet();
    void appliesWhereItsStartConditionHolds();
    void appliesAndChangesNothingWhereItsResultHolds();
    void conflictsAndTouchesNothingWhereAFieldMoved();
    void conflictsWhereTheFamilyIsGone();
    void conflictsWhereAChangedMemberIsGone();
    void editsToDifferentMembersBothApply();
    void memberRemovalConflictsAfterThatMemberChanged();
    void memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem();
    void memberAdditionHoldsWhenThatPersonIsAlreadyThere();
    void inverseSwapsTheChangeUnderAnUndoHeading();
};
```

### `tests/services/UpdateFamilyCommandTest.cpp`

```cpp
#include "UpdateFamilyCommandTest.h"
#include "FamilyCommands.h"
#include "Document.h"
#include "TestWard.h"

#include <QTest>

namespace
{

std::unique_ptr<UpdateFamilyCommand> bobPhoneEdit(const QString& phone)
{
    Family opened = TestWard::smith();
    return UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), phone));
}

Family smithWithout(const PersonId& id)
{
    QList<Person> members;
    for (const Person& member : TestWard::smith().members())
    {
        if (member.id() != id)
        {
            members.append(member);
        }
    }
    Family family = TestWard::smith();
    family.setMembers(members);
    return family;
}

Person dan()
{
    return Person::createWithId(PersonId::fromString("person-dan"), Name("Smith", "Dan"),
                                Gender::Male, Birthday(), Phone(), Phone(), QString(),
                                QStringList(), false, QString(), QString());
}

}  // namespace

UpdateFamilyCommandTest::UpdateFamilyCommandTest(QObject* parent)
    : QObject(parent)
{
}

void UpdateFamilyCommandTest::editHoldsOnlyTheFieldsThatDiffer()
{
    QJsonObject change = bobPhoneEdit("555-2222")->toJson()["change"].toObject();

    QCOMPARE(change.keys(), QStringList({"members"}));
    QJsonObject members = change["members"].toObject();
    QCOMPARE(members.keys(), QStringList({TestWard::bobId().toString()}));
    QJsonObject bob = members[TestWard::bobId().toString()].toObject();
    QCOMPARE(bob.keys(), QStringList({"phone"}));
    QCOMPARE(bob["phone"].toObject()["before"].toString(), QString("555-1111"));
    QCOMPARE(bob["phone"].toObject()["after"].toString(), QString("555-2222"));
}

void UpdateFamilyCommandTest::editThatChangesNothingIsNoCommand()
{
    QVERIFY(UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smith()) == nullptr);
    QVERIFY(bobPhoneEdit("555-2222") != nullptr);
}

void UpdateFamilyCommandTest::descriptionIsAHeadingAndALinePerValueSet()
{
    Family opened = TestWard::smith();
    Person alice = TestWard::alice();
    alice.setEmail(QString());
    Family saved = TestWard::withMember(TestWard::withPhone(opened, TestWard::bobId(), "555-2222"), alice);
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));

    std::unique_ptr<UpdateFamilyCommand> command = UpdateFamilyCommand::fromEdit(opened, saved);

    QCOMPARE(command->description().heading(), QString("Smith family"));
    QCOMPARE(command->description().lines(), QStringList({"Bob, phone: 555-2222",
                                                          "Alice, email: (none)",
                                                          "address: 14 Elm St, Provo, UT"}));
}

void UpdateFamilyCommandTest::appliesWhereItsStartConditionHolds()
{
    Document document = TestWard::document();

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == Outcome::Applied);

    QCOMPARE(QString(TestWard::person(document, TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(TestWard::person(document, TestWard::aliceId()) == TestWard::alice());
    QVERIFY(TestWard::family(document, TestWard::jonesId()) == TestWard::jones());
}

void UpdateFamilyCommandTest::appliesAndChangesNothingWhereItsResultHolds()
{
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-2222"));
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == Outcome::Applied);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::conflictsAndTouchesNothingWhereAFieldMoved()
{
    Family opened = TestWard::smith();
    Family saved = TestWard::withPhone(opened, TestWard::bobId(), "555-2222");
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));
    std::unique_ptr<UpdateFamilyCommand> command = UpdateFamilyCommand::fromEdit(opened, saved);
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(opened, TestWard::bobId(), "555-9999"));
    Document before = document;

    QVERIFY(command->execute(document) == Outcome::Conflicted);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
}

void UpdateFamilyCommandTest::conflictsWhereTheFamilyIsGone()
{
    Document document = TestWard::document();
    document.removeFamily(TestWard::smithId());
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == Outcome::Conflicted);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::conflictsWhereAChangedMemberIsGone()
{
    Document document = TestWard::withFamily(TestWard::document(), smithWithout(TestWard::bobId()));
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == Outcome::Conflicted);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::editsToDifferentMembersBothApply()
{
    Family opened = TestWard::smith();
    Person alice = TestWard::alice();
    alice.setEmail("alice@new.example.com");
    std::unique_ptr<UpdateFamilyCommand> bobEdit = bobPhoneEdit("555-2222");
    std::unique_ptr<UpdateFamilyCommand> aliceEdit = UpdateFamilyCommand::fromEdit(opened, TestWard::withMember(opened, alice));

    Document bobFirst = TestWard::document();
    QVERIFY(bobEdit->execute(bobFirst) == Outcome::Applied);
    QVERIFY(aliceEdit->execute(bobFirst) == Outcome::Applied);
    Document aliceFirst = TestWard::document();
    QVERIFY(aliceEdit->execute(aliceFirst) == Outcome::Applied);
    QVERIFY(bobEdit->execute(aliceFirst) == Outcome::Applied);

    QCOMPARE(QString(TestWard::person(bobFirst, TestWard::bobId()).phone()), QString("555-2222"));
    QCOMPARE(TestWard::person(bobFirst, TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QVERIFY(bobFirst == aliceFirst);
}

void UpdateFamilyCommandTest::memberRemovalConflictsAfterThatMemberChanged()
{
    std::unique_ptr<UpdateFamilyCommand> removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(TestWard::smith(), TestWard::aliceId(), "555-7777"));

    QVERIFY(removal->execute(document) == Outcome::Conflicted);

    QCOMPARE(QString(TestWard::person(document, TestWard::aliceId()).phone()), QString("555-7777"));
}

void UpdateFamilyCommandTest::memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem()
{
    std::unique_ptr<UpdateFamilyCommand> removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::document();

    QVERIFY(removal->execute(document) == Outcome::Applied);
    QVERIFY(!document.findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(1));

    QVERIFY(removal->inverse()->execute(document) == Outcome::Applied);
    QVERIFY(TestWard::person(document, TestWard::aliceId()) == TestWard::alice());
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(2));
}

void UpdateFamilyCommandTest::memberAdditionHoldsWhenThatPersonIsAlreadyThere()
{
    Family withDan = TestWard::smith();
    withDan.setMembers({TestWard::bob(), TestWard::alice(), dan()});
    std::unique_ptr<UpdateFamilyCommand> addition = UpdateFamilyCommand::fromEdit(TestWard::smith(), withDan);
    Document document = TestWard::withFamily(TestWard::document(), withDan);
    Document before = document;

    QVERIFY(addition->execute(document) == Outcome::Applied);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(3));
}

void UpdateFamilyCommandTest::inverseSwapsTheChangeUnderAnUndoHeading()
{
    std::unique_ptr<UpdateFamilyCommand> edit = bobPhoneEdit("555-2222");
    CommandPtr inverse = edit->inverse();

    QCOMPARE(inverse->description().heading(), QString("Undo: Smith family"));
    QCOMPARE(inverse->description().lines(), edit->description().lines());

    Document document = TestWard::document();
    QVERIFY(edit->execute(document) == Outcome::Applied);
    QVERIFY(inverse->execute(document) == Outcome::Applied);
    QVERIFY(document == TestWard::document());

    Document moved = TestWard::withFamily(TestWard::document(),
                                          TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-9999"));
    Document movedBefore = moved;
    QVERIFY(inverse->execute(moved) == Outcome::Conflicted);
    QVERIFY(moved == movedBefore);
}
```

### `tests/services/CommandRoundTripTest.h`

```cpp
#pragma once

#include <QObject>

/// Every registered command type survives the journal's round trip, judged by behaviour.
class CommandRoundTripTest : public QObject
{
    Q_OBJECT

public:
    explicit CommandRoundTripTest(QObject* parent);

private slots:
    void everyRegisteredTypeHasSamples();
    void sampleRebuiltFromItsEntryBehavesAsTheOriginal_data();
    void sampleRebuiltFromItsEntryBehavesAsTheOriginal();
    void runningACommandDoesNotChangeIt();
};
```

### `tests/services/CommandRoundTripTest.cpp`

```cpp
#include "CommandRoundTripTest.h"
#include "CommandRegistry.h"
#include "CommandSamples.h"
#include "FamilyCommands.h"
#include "TestWard.h"

#include <QSet>
#include <QTest>

CommandRoundTripTest::CommandRoundTripTest(QObject* parent)
    : QObject(parent)
{
}

void CommandRoundTripTest::everyRegisteredTypeHasSamples()
{
    QVERIFY(!CommandRegistry::types().isEmpty());
    QSet<QString> names;
    for (const CommandType& type : CommandRegistry::types())
    {
        QVERIFY2(!names.contains(type.name), qPrintable(type.name));
        names.insert(type.name);

        std::vector<CommandSample> samples = CommandSamples::forType(type.name);
        QVERIFY2(!samples.empty(), qPrintable(type.name));
        bool hasStartHolds = false;
        for (const CommandSample& sample : samples)
        {
            QCOMPARE(sample.command->type(), type.name);
            hasStartHolds = hasStartHolds || sample.sampleCase == SampleCase::StartHolds;
        }
        QVERIFY2(hasStartHolds, qPrintable(type.name));
    }
    QVERIFY(names.contains(UpdateFamilyCommand::TypeName));
}

void CommandRoundTripTest::sampleRebuiltFromItsEntryBehavesAsTheOriginal_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<int>("index");
    for (const CommandType& type : CommandRegistry::types())
    {
        std::vector<CommandSample> samples = CommandSamples::forType(type.name);
        for (int i = 0; i < static_cast<int>(samples.size()); ++i)
        {
            QTest::newRow(qPrintable(QString("%1 #%2").arg(type.name).arg(i))) << type.name << i;
        }
    }
}

void CommandRoundTripTest::sampleRebuiltFromItsEntryBehavesAsTheOriginal()
{
    QFETCH(QString, type);
    QFETCH(int, index);
    std::vector<CommandSample> samples = CommandSamples::forType(type);
    const CommandSample& sample = samples[index];

    QJsonObject entry = CommandRegistry::toJson(*sample.command);
    CommandPtr rebuilt = CommandRegistry::fromJson(entry);
    QVERIFY(rebuilt != nullptr);
    QCOMPARE(rebuilt->type(), type);
    QVERIFY(CommandRegistry::toJson(*rebuilt) == entry);
    QVERIFY(rebuilt->description() == sample.command->description());

    Document byOriginal = sample.document;
    Document byRebuilt = sample.document;
    Outcome original = sample.command->execute(byOriginal);
    Outcome again = rebuilt->execute(byRebuilt);

    Outcome expected = sample.sampleCase == SampleCase::Conflicts ? Outcome::Conflicted : Outcome::Applied;
    QVERIFY(original == expected);
    QVERIFY(again == expected);
    QVERIFY(byOriginal == byRebuilt);
    if (sample.sampleCase == SampleCase::StartHolds)
    {
        QVERIFY(byOriginal != sample.document);
    }
    else
    {
        QVERIFY(byOriginal == sample.document);
    }
}

void CommandRoundTripTest::runningACommandDoesNotChangeIt()
{
    Family opened = TestWard::smith();
    std::unique_ptr<UpdateFamilyCommand> command =
        UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));
    QJsonObject before = CommandRegistry::toJson(*command);

    Document applied = TestWard::document();
    QVERIFY(command->execute(applied) == Outcome::Applied);
    QCOMPARE(QString(TestWard::person(applied, TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(CommandRegistry::toJson(*command) == before);

    Document moved = TestWard::withFamily(TestWard::document(),
                                          TestWard::withPhone(opened, TestWard::bobId(), "555-9999"));
    Document movedBefore = moved;
    QVERIFY(command->execute(moved) == Outcome::Conflicted);
    QVERIFY(moved == movedBefore);
}
```

### `tests/services/WardSessionTest.h`

```cpp
#pragma once

#include <QObject>

class WardSessionTest : public QObject
{
    Q_OBJECT

public:
    explicit WardSessionTest(QObject* parent);

private slots:
    void commitAppliesAndJournalsTheSerialisedCommand();
    void editConfirmedAfterARemoteChangeToItsFamilyConflicts();
    void refusedCommandChangesNothingAndJournalsNothing();
    void emptyJournalFastForwards();
    void offlineEditsToDifferentMembersBothApply();
    void noteAndPhoneEditsToOnePersonBothApply();
    void journalThatFitsReplaysInFull();
    void oneConflictAbandonsTheWholeReplay();
    void commandWhoseTargetWasDeletedConflicts();
    void toRedoDescriptionsStayAsWrittenAfterARename();
    void laterAbandonmentAddsItsOwnGroup();
    void newCommandAfterAnAbandonedReplayJournalsAfresh();
    void undoRunsTheInverseThroughTheCommitStep();
    void undoAfterARemoteEditToAnotherFamilyApplies();
    void undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack();
    void commandWithoutAnInverseNeverEntersTheUndoStack();
    void newCommandClearsTheRedoSide();
    void undoStackHoldsAtMostOneHundred();
    void dismissingTheToRedoListEmptiesIt();
};
```

### `tests/services/WardSessionTest.cpp`

```cpp
#include "WardSessionTest.h"
#include "CommandRegistry.h"
#include "FamilyCommands.h"
#include "TestCommands.h"
#include "TestWard.h"
#include "WardSession.h"

#include <QTest>

namespace
{

CommitResult commitEdit(WardSession& session, const Family& opened, const Family& saved)
{
    return session.commit(UpdateFamilyCommand::fromEdit(opened, saved));
}

Family familyIn(const WardSession& session, const FamilyId& id)
{
    return TestWard::family(session.document(), id);
}

QString phoneOf(const Document& document, const PersonId& id)
{
    return TestWard::person(document, id).phone();
}

/// Commits a phone edit to the member, opened on the session's current family.
CommitResult commitPhone(WardSession& session, const FamilyId& familyId, const PersonId& personId, const QString& phone)
{
    Family opened = familyIn(session, familyId);
    return commitEdit(session, opened, TestWard::withPhone(opened, personId, phone));
}

/// Commits an address edit to the family, opened on the session's current family.
CommitResult commitAddress(WardSession& session, const FamilyId& familyId, const Address& address)
{
    Family opened = familyIn(session, familyId);
    Family saved = opened;
    saved.setAddress(address);
    return commitEdit(session, opened, saved);
}

/// Commits an email edit to Alice, opened on the session's current Smith family.
CommitResult commitAliceEmail(WardSession& session, const QString& email)
{
    Family opened = familyIn(session, TestWard::smithId());
    Person alice = TestWard::person(session.document(), TestWard::aliceId());
    alice.setEmail(email);
    return commitEdit(session, opened, TestWard::withMember(opened, alice));
}

/// A session with three entries in its journal - Carol's phone, Bob's phone, the Smith
/// address - abandoned onto a version where Bob's phone moved to 555-9999.
void abandonThreeEdits(WardSession& mine)
{
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999");
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Abandoned);
}

}  // namespace

WardSessionTest::WardSessionTest(QObject* parent)
    : QObject(parent)
{
}

void WardSessionTest::commitAppliesAndJournalsTheSerialisedCommand()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    Family opened = familyIn(session, TestWard::smithId());
    std::unique_ptr<UpdateFamilyCommand> command =
        UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));
    QJsonObject expected = CommandRegistry::toJson(*command);

    CommitResult result = session.commit(std::move(command));

    QVERIFY(result.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
    QCOMPARE(session.journal().entries()[0].sequence, qint64(1));
    QVERIFY(session.journal().entries()[0].command == expected);
    QVERIFY(session.canUndo());
    QCOMPARE(session.undoHeading(), QString("Smith family"));
}

void WardSessionTest::editConfirmedAfterARemoteChangeToItsFamilyConflicts()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());

    Family opened = familyIn(mine, TestWard::smithId());
    QVERIFY(commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999").status == CommitStatus::Applied);
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::FastForwarded);

    CommitResult result = commitEdit(mine, opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(!mine.canUndo());
}

void WardSessionTest::refusedCommandChangesNothingAndJournalsNothing()
{
    WardSession session(nullptr);
    session.open(TestWard::document());

    CommitResult refused = session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-2222", "You are the only admin."));

    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(refused.refusal, QString("You are the only admin."));
    QVERIFY(session.document() == TestWard::document());
    QVERIFY(session.journal().isEmpty());

    CommitResult allowed = session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-2222", QString()));

    QVERIFY(allowed.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
}

void WardSessionTest::emptyJournalFastForwards()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(theirs, TestWard::jonesId(), TestWard::carolId(), "555-5555");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::FastForwarded);

    QVERIFY(mine.document() == theirs.document());
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-5555"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(mine.toRedo().isEmpty());
}

void WardSessionTest::offlineEditsToDifferentMembersBothApply()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAliceEmail(theirs, "alice@new.example.com");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(TestWard::person(mine.document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QVERIFY(mine.toRedo().isEmpty());
    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
}

void WardSessionTest::noteAndPhoneEditsToOnePersonBothApply()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    Family opened = familyIn(theirs, TestWard::smithId());
    Person bob = TestWard::bob();
    bob.setSpecialNeedNote("Oxygen at night");
    commitEdit(theirs, opened, TestWard::withMember(opened, bob));

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    Person replayed = TestWard::person(mine.document(), TestWard::bobId());
    QCOMPARE(QString(replayed.phone()), QString("555-2222"));
    QCOMPARE(replayed.specialNeedNote(), QString("Oxygen at night"));
}

void WardSessionTest::journalThatFitsReplaysInFull()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitAliceEmail(theirs, "alice@new.example.com");

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-5555"));
    QCOMPARE(familyIn(mine, TestWard::smithId()).address().full(), QString("14 Elm St, Provo, UT"));
    QCOMPARE(TestWard::person(mine.document(), TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QCOMPARE(mine.journal().entries().size(), qsizetype(3));
}

void WardSessionTest::oneConflictAbandonsTheWholeReplay()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(mine, TestWard::smithId(), TestWard::address("14 Elm St", "Provo, UT"));
    commitPhone(theirs, TestWard::smithId(), TestWard::bobId(), "555-9999");
    QVERIFY(mine.canUndo());

    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Abandoned);

    QVERIFY(mine.document() == theirs.document());
    QCOMPARE(phoneOf(mine.document(), TestWard::carolId()), QString("555-4444"));
    QCOMPARE(familyIn(mine, TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
    QVERIFY(mine.journal().isEmpty());
    QVERIFY(!mine.canUndo());
    QVERIFY(!mine.canRedo());
    QCOMPARE(mine.toRedo().size(), qsizetype(1));
    const RedoGroup& group = mine.toRedo()[0];
    QCOMPARE(group.descriptions.size(), qsizetype(3));
    QCOMPARE(group.descriptions[0].heading(), QString("Jones family"));
    QCOMPARE(group.descriptions[0].lines(), QStringList({"Carol, phone: 555-5555"}));
    QCOMPARE(group.descriptions[1].heading(), QString("Smith family"));
    QCOMPARE(group.descriptions[1].lines(), QStringList({"Bob, phone: 555-2222"}));
    QCOMPARE(group.descriptions[2].heading(), QString("Smith family"));
    QCOMPARE(group.descriptions[2].lines(), QStringList({"address: 14 Elm St, Provo, UT"}));
    QCOMPARE(group.clashed, 1);
    QVERIFY(group.stop.status == CommitStatus::Conflicted);
}

void WardSessionTest::commandWhoseTargetWasDeletedConflicts()
{
    WardSession mine(nullptr);
    mine.open(TestWard::document());
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-5555");
    Document version = TestWard::document();
    version.removeFamily(TestWard::jonesId());

    QVERIFY(mine.replayOnto(version) == ReplayStatus::Abandoned);

    QVERIFY(!mine.document().findFamilyById(TestWard::jonesId()).has_value());
    QCOMPARE(mine.toRedo().size(), qsizetype(1));
    QCOMPARE(mine.toRedo()[0].clashed, 0);
    QCOMPARE(mine.toRedo()[0].descriptions[0].heading(), QString("Jones family"));
}

void WardSessionTest::toRedoDescriptionsStayAsWrittenAfterARename()
{
    WardSession mine(nullptr);
    mine.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    Person bob = TestWard::bob();
    bob.setName(Name("Smyth", "Bob"));
    bob.setPhone(Phone("555-9999"));
    Person alice = TestWard::alice();
    alice.setName(Name("Smyth", "Alice"));
    Family renamed = TestWard::smith();
    renamed.setMembers({bob, alice});

    QVERIFY(mine.replayOnto(TestWard::withFamily(TestWard::document(), renamed)) == ReplayStatus::Abandoned);

    QCOMPARE(familyIn(mine, TestWard::smithId()).surname(), QString("Smyth"));
    const Description& description = mine.toRedo()[0].descriptions[0];
    QCOMPARE(description.heading(), QString("Smith family"));
    QCOMPARE(description.lines(), QStringList({"Bob, phone: 555-2222"}));
}

void WardSessionTest::laterAbandonmentAddsItsOwnGroup()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    commitPhone(mine, TestWard::jonesId(), TestWard::carolId(), "555-6666");
    Document version = mine.document();
    version.removeFamily(TestWard::jonesId());

    QVERIFY(mine.replayOnto(version) == ReplayStatus::Abandoned);

    QCOMPARE(mine.toRedo().size(), qsizetype(2));
    QCOMPARE(mine.toRedo()[0].descriptions.size(), qsizetype(3));
    QCOMPARE(mine.toRedo()[1].descriptions.size(), qsizetype(1));
    QCOMPARE(mine.toRedo()[1].descriptions[0].lines(), QStringList({"Carol, phone: 555-6666"}));
    QCOMPARE(mine.toRedo()[1].clashed, 0);
}

void WardSessionTest::newCommandAfterAnAbandonedReplayJournalsAfresh()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    QVERIFY(mine.journal().isEmpty());

    QVERIFY(commitAliceEmail(mine, "alice@new.example.com").status == CommitStatus::Applied);

    QCOMPARE(mine.journal().entries().size(), qsizetype(1));
    QCOMPARE(mine.journal().entries()[0].sequence, qint64(4));
    QVERIFY(mine.canUndo());
}

void WardSessionTest::undoRunsTheInverseThroughTheCommitStep()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");

    CommitResult undone = session.undo();

    QVERIFY(undone.status == CommitStatus::Applied);
    QVERIFY(session.document() == TestWard::document());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
    Description inverse = CommandRegistry::descriptionOf(session.journal().entries()[1].command);
    QCOMPARE(inverse.heading(), QString("Undo: Smith family"));
    QCOMPARE(inverse.lines(), QStringList({"Bob, phone: 555-2222"}));
    QVERIFY(!session.canUndo());
    QVERIFY(session.canRedo());
    QCOMPARE(session.redoHeading(), QString("Smith family"));

    CommitResult redone = session.redo();

    QVERIFY(redone.status == CommitStatus::Applied);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-2222"));
    QCOMPARE(session.journal().entries().size(), qsizetype(3));
    QVERIFY(session.journal().entries()[2].command == session.journal().entries()[0].command);
    QCOMPARE(session.journal().entries()[2].sequence, qint64(3));
    QVERIFY(session.canUndo());
    QVERIFY(!session.canRedo());
}

void WardSessionTest::undoAfterARemoteEditToAnotherFamilyApplies()
{
    WardSession mine(nullptr);
    WardSession theirs(nullptr);
    mine.open(TestWard::document());
    theirs.open(TestWard::document());
    commitPhone(mine, TestWard::smithId(), TestWard::bobId(), "555-2222");
    commitAddress(theirs, TestWard::jonesId(), TestWard::address("6 Oak Ave", "Provo, UT"));
    QVERIFY(mine.replayOnto(theirs.document()) == ReplayStatus::Replayed);

    QVERIFY(mine.undo().status == CommitStatus::Applied);

    QCOMPARE(phoneOf(mine.document(), TestWard::bobId()), QString("555-1111"));
    QCOMPARE(familyIn(mine, TestWard::jonesId()).address().full(), QString("6 Oak Ave, Provo, UT"));
}

void WardSessionTest::undoWhoseInverseConflictsLeavesTheCommandOnTheUndoStack()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");
    QVERIFY(session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);
    QCOMPARE(session.journal().entries().size(), qsizetype(2));

    CommitResult result = session.undo();

    QVERIFY(result.status == CommitStatus::Conflicted);
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-9999"));
    QVERIFY(session.canUndo());
    QCOMPARE(session.undoHeading(), QString("Smith family"));
    QVERIFY(!session.canRedo());
    QCOMPARE(session.journal().entries().size(), qsizetype(2));
}

void WardSessionTest::commandWithoutAnInverseNeverEntersTheUndoStack()
{
    WardSession session(nullptr);
    session.open(TestWard::document());

    QVERIFY(session.commit(std::make_unique<ForcePhoneCommand>(TestWard::bobId(), "555-9999", QString())).status
            == CommitStatus::Applied);

    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-9999"));
    QCOMPARE(session.journal().entries().size(), qsizetype(1));
    QVERIFY(!session.canUndo());
}

void WardSessionTest::newCommandClearsTheRedoSide()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    commitPhone(session, TestWard::smithId(), TestWard::bobId(), "555-2222");
    session.undo();
    QVERIFY(session.canRedo());

    commitPhone(session, TestWard::jonesId(), TestWard::carolId(), "555-5555");

    QVERIFY(!session.canRedo());
    QCOMPARE(session.undoHeading(), QString("Jones family"));
}

void WardSessionTest::undoStackHoldsAtMostOneHundred()
{
    WardSession session(nullptr);
    session.open(TestWard::document());
    for (int i = 1; i <= 101; ++i)
    {
        QVERIFY(commitPhone(session, TestWard::smithId(), TestWard::bobId(), QString("555-%1").arg(1000 + i)).status
                == CommitStatus::Applied);
    }

    for (int i = 0; i < 100; ++i)
    {
        QVERIFY(session.canUndo());
        QVERIFY(session.undo().status == CommitStatus::Applied);
    }

    QVERIFY(!session.canUndo());
    QCOMPARE(phoneOf(session.document(), TestWard::bobId()), QString("555-1001"));
}

void WardSessionTest::dismissingTheToRedoListEmptiesIt()
{
    WardSession mine(nullptr);
    abandonThreeEdits(mine);
    QCOMPARE(mine.toRedo().size(), qsizetype(1));

    mine.dismissToRedo();

    QVERIFY(mine.toRedo().isEmpty());
}
```

### What task 1 leaves for later plans and tasks

- A replay refused by a rule runs the same `attempt` as a first-run refusal, which
  `refusedCommandChangesNothingAndJournalsNothing` covers. No registered command has a rule
  until plan 3's access-list commands, so the replay-side refusal is tested there.
- Entries stay in the journal after a successful replay; plan 3's landed marks remove the
  ones a version carries. The design's test of an undo after a remote edit to the same field
  needs that trimming, so it is plan 3's.

### Commit

```bash
git branch --show-current
git add src/commands src/services/Journal.h src/services/Journal.cpp src/services/WardSession.h src/services/WardSession.cpp CMakeLists.txt tests
git commit -m "Commands answer Applied or Conflicted, and WardSession commits, journals, replays and undoes them"
```

---

## STOP

**Stop here for the user's own review of the core. Do not start task 2 until the user says so.**

After the user's review, dispatch the read-only reviewer on task 1 as executing-with-review
describes, then continue with task 2 when the user says so.

---

## Task 2: The app runs every edit through the session

**Files:**
- Modify: `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/widgets/FamilyEditPanel.h`
- Modify: `src/widgets/WardListDialog.h`, `src/widgets/WardListDialog.cpp`
- Modify: `src/widgets/NeedsSubView.cpp`
- Modify: `docs/ARCHITECTURE.md`, `CLAUDE.md`

**DocumentManager becomes a thin wrapper over one `WardSession`.**
- Members `Document m_document` and `CommandHistory m_commandHistory` become
  `WardSession m_session`. `document()` returns `m_session.document()` (the archive overlay
  stays until task 5 deletes it).
- `CommitResult executeCommand(CommandPtr command)`: captures `command->description()`, calls
  `m_session.commit`, and on `Applied` runs `checkForIncompleteWards()` and `autoSave()` as
  today; otherwise emits `commandNotApplied(const Description& description, const CommitResult& result)`.
- `undo()` and `redo()` call `m_session.undo()` / `redo()` the same way, emitting
  `commandNotApplied` with the undo stack's heading when the result is not `Applied`.
- `undoDescription()` / `redoDescription()` become `undoHeading()` / `redoHeading()`.
- Signals: `canUndoChanged` and `canRedoChanged` become one `undoStackChanged`, forwarded from
  the session; `documentChanged` is forwarded from the session's.
- `newDocument()` calls `m_session.open(Document::empty())`; `openDocument()` calls
  `m_session.open(result.document)`. `setDocument` is deleted.
- Deleted: `isDirty()`, the `isDirty` `Q_PROPERTY`, `dirtyChanged`, and the `markSaved` call in
  `saveDocumentAs` (every command is saved as it is made).
- The ward lookup and geocoding slots are rewritten in task 3.

**MainWindow.**
- `updateUndoRedoActions()` reads `undoHeading()` / `redoHeading()` and connects to
  `undoStackChanged`.
- New slot `onCommandNotApplied(const Description& description, const CommitResult& result)`:
  for `Conflicted`, a message box with
  `tr("Your change could not be applied, because the ward changed first. This needs doing again:")`
  followed by the heading and its lines; for `Refused`, the rule's wording.
- `onSaveFamily()` builds `UpdateFamilyCommand::fromEdit(m_editPanel->openedFamily(), m_editPanel->family())`
  and commits it when it is not empty, then closes the panel. `FamilyEditPanel` gains
  `const Family& openedFamily() const { return m_originalFamily; }`, the copy it opened with.

**The special-need dialog becomes one "Special Need" dialog** (*The journal*, last bullet of the
work list).
- `WardListDialog::selectPersonWithName` has one caller, `NeedsSubView::showNeedDialog`; it is
  replaced by
  `static std::optional<SpecialNeedEdit> editSpecialNeed(const std::optional<PersonId>& lockedPerson, QWidget* parent)`
  with `struct SpecialNeedEdit { Family opened; Family saved; };` in `WardListDialog.h`.
- Title `tr("Special Need")`. Opened on an existing need (`lockedPerson` set), the person is
  preselected and the tree is disabled. Opened from empty space, `onSelectionChanged()` in this
  mode reads the picked person's family from the document at that moment into `m_openedFamily`
  and fills the note field with that person's current note: that is when the start condition
  is taken.
- On accept, `saved` is `m_openedFamily` with that member's note set to the field's text.
- `NeedsSubView::showNeedDialog` commits `UpdateFamilyCommand::fromEdit(edit.opened, edit.saved)`.
  `NeedsSubView::deleteNeed` captures the family before its confirmation box and commits the
  edit that clears the note. The helper `updatePersonInFamily` is deleted.

**Docs.**
- `docs/ARCHITECTURE.md`: the *Command Interface* section shows `execute` returning `Outcome`,
  `inverse`, `refusal`, `toJson` and the registry; *Command History* becomes *WardSession* (the
  commit step, the journal, replay and the to-redo list, undo as inverse); the
  `AddFamilyCommand` example becomes `UpdateFamilyCommand` with `FamilyChange`; *DocumentManager*
  describes the wrapper; *Modifying Data* steps follow the commit step.
- `CLAUDE.md`: the *Active Tasks* row and the *NeedsSubView Dialog UX (adopted)* section are
  removed, since the dialog is done.

**Tests:** the session behaviour is covered by task 1; this task is wiring and dialogs.

**Commit:** `git commit -m "The app runs every edit through WardSession; edits take their start condition when opened"`

---

## Task 3: Coordinates belong to addresses, and automatic updates run in the session

*Saving* (coordinates, automatic batches), *The journal* (automatic updates stay out of it).

**Files:**
- Create: `src/models/Coordinates.h`, `src/models/Coordinates.cpp`
- Modify: `src/models/Document.h`, `src/models/Document.cpp`, `src/models/Family.h`,
  `src/models/Family.cpp`
- Create: `src/commands/AddressCoordinatesCommands.h`, `src/commands/AddressCoordinatesCommands.cpp`
- Modify: `src/commands/Command.h`, `src/commands/Command.cpp`, `src/commands/FamilyChange.h`,
  `src/commands/FamilyChange.cpp`, `src/commands/FamilyCommands.h`, `src/commands/FamilyCommands.cpp`,
  `src/commands/CommandRegistry.cpp`
- Create: `src/services/Geocoder.h`, `src/services/Geocoder.cpp`,
  `src/services/NominatimGeocoder.h`, `src/services/NominatimGeocoder.cpp`,
  `src/services/UnitLookup.h`, `src/services/UnitLookup.cpp`
- Modify: `src/services/UnitLookupService.h`, `src/services/UnitLookupService.cpp`
- Delete: `src/services/BackgroundGeocodingService.h`, `src/services/BackgroundGeocodingService.cpp`
- Modify: `src/services/WardSession.h`, `src/services/WardSession.cpp`,
  `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`,
  `src/services/WardDirectoryImportService.cpp`
- Modify: `src/listmodels/Filter.cpp`, `src/viewmodels/MapViewModel.cpp`, `src/widgets/MapWidget.cpp`,
  `src/widgets/FamilyEditPanel.h`, `src/widgets/FamilyEditPanel.cpp`, `src/widgets/MainWindow.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/main.cpp`, `tests/services/TestWard.cpp`,
  `tests/services/CommandSamples.cpp`
- Create: `tests/services/FakeGeocoder.h`, `tests/services/FakeGeocoder.cpp`,
  `tests/services/FakeUnitLookup.h`, `tests/services/FakeUnitLookup.cpp`,
  `tests/services/AutomaticUpdatesTest.h`, `tests/services/AutomaticUpdatesTest.cpp`

**Model.**
- `class Coordinates { Coordinates(double latitude, double longitude); latitude(); longitude(); toJson(); fromJson(); operator==; }`.
- `Document` holds `QHash<QString, Coordinates> m_addressCoordinates`, keyed by `Address::full()`:
  `addressCoordinates()`, `std::optional<Coordinates> coordinatesFor(const Address& address) const`
  (none for an empty address), `setAddressCoordinates(const QString& address, const std::optional<Coordinates>& coordinates)`.
  JSON key `addressCoordinates`, an array of `{address, latitude, longitude}`; `operator==`
  includes it.
- `Family` loses `m_latitude`, `m_longitude`, `latitude()`, `longitude()`, `setLatitude`,
  `setLongitude`, `setLocation` and `isMapped()`; `create` and `createWithId` lose their two
  coordinate parameters, and `toJson` / `fromJson` lose the keys. Every reader asks
  `document.coordinatesFor(family.address())`: `Filter.cpp:219,223,506,510`,
  `MapViewModel.cpp:38,55,56,61`, `MapWidget.cpp:202,895,955`.
- `WardDirectoryImportService::preserveIds` and the parsers stop passing coordinates to
  `Family`. Where the directory PDF's coordinates go is open question 10.

**Typed coordinates are a journalled command on the address's entry.**
- `SetAddressCoordinatesCommand(const QString& address, const std::optional<Coordinates>& before, const Coordinates& typed, const Description& description)`,
  `TypeName = "setAddressCoordinates"`: sets the entry to `typed` whatever it holds, so it never
  conflicts; its result already holds when the entry equals `typed`.
- Its inverse is `RevertAddressCoordinatesCommand` (`TypeName = "revertAddressCoordinates"`),
  which puts back `before` when the entry still holds `typed` (result holds when it holds
  `before`) and otherwise conflicts. Its own inverse is the forward command.
- Description: heading `tr("Map position for %1").arg(address)`, one line
  `tr("latitude %1, longitude %2")`.
- `FamilyEditPanel` shows `coordinatesFor` the address it displays, disables the coordinate
  fields while that address is empty, and exposes `std::optional<Coordinates> typedCoordinates() const`
  when the user typed or looked up a position. `MainWindow::onSaveFamily` commits the family
  edit, then a `SetAddressCoordinatesCommand` against the saved address when typed coordinates
  differ from its entry (open question 9).

**Automatic updates live in `WardSession`, behind fakeable interfaces.**
- `class Geocoder : public QObject` with `virtual void geocode(const QString& address) = 0;` and
  signals `resolved(const QString& address, const Coordinates& coordinates)` and
  `failed(const QString& address)`. `NominatimGeocoder` implements it over `GeocodingService`.
- `class UnitLookup : public QObject` with `lookupWard`, `lookupStake` and signals `wardFound`,
  `stakeFound`, `lookupFailed`. `UnitLookupService` derives from it.
- `WardSession(Geocoder* geocoder, UnitLookup* unitLookup, QObject* parent)`.
- `Command` gains `virtual QStringList addressesGiven() const;` (base returns empty): the
  addresses this command gives a family, known at construction. `UpdateFamilyCommand` returns the
  address after when its change sets a non-empty address (`FamilyChange` gains
  `const std::optional<Change<Address>>& address() const`).
- `WardSession::run` starts `runAutomaticUpdates()` after an applied command when any of its
  `addressesGiven()` lacks an entry and is not on the failed list. `DocumentManager` calls
  `runAutomaticUpdates()` when a ward opens (`newDocument`, `openDocument`). Replay starts
  nothing, so a device that takes another device's work runs no batch for it. Nothing else
  starts one.
- `runAutomaticUpdates()`: requests, once each, every address some family has that the table
  lacks, is not on the failed list and is not already pending (addresses with no street are
  skipped, as today); and looks up, one at a time, each ward lacking its details whose unit is
  not on the failed list. Results are held by address as they arrive; a failure adds the text
  to the failed list (`QSet<QString> m_failed`, device-local, addresses and unit numbers).
- When the last request answers, the batch applies: each result adds the entry for its address
  and never replaces one already there, then every entry no family's address uses is dropped.
  A batch with nothing to request finishes at once. Only when something changed does the
  session emit `documentChanged(DocumentChange::full())` and `automaticUpdateApplied()`;
  `DocumentManager` saves on `automaticUpdateApplied`. Nothing enters the journal.
- A completed ward or stake lookup applies its details as `onWardLookupComplete` does today and
  emits `automaticUpdateApplied()` only when they changed something. A failed lookup puts the
  unit on the failed list and continues to the next ward (today it stops the chain,
  `DocumentManager.cpp:292-298`).
- `DocumentManager` loses `startBatchGeocoding`, `stopGeocoding`, `isGeocoding`,
  `onFamilyGeocoded`, `onGeocodingFinished`, `onWardLookupComplete`, `onStakeLookupComplete`,
  `onLookupFailed`, `checkForIncompleteWards`, `m_pendingWardLookups`, `m_pendingStakeLookups`
  and `m_geocodingService`; `geocodingProgressChanged` is forwarded from the session's
  `geocodingProgress(int completed, int total)`. `MainWindow::onImportPdf` drops its
  `startBatchGeocoding()` call.
- Rerunning a batch whose push lost is plan 3, since it depends on the push.

**Tests** (`AutomaticUpdatesTest`, with `FakeGeocoder` recording requests in order and
answering when the test says, and `FakeUnitLookup` likewise):
- `geocodeResultAppliesOutsideTheJournal`: after resolving, `coordinatesFor` the Smith address
  equals the answered coordinates and the journal is empty; a replay onto a version without the
  entry leaves it absent and the journal untouched.
- `geocodeBetweenAnOfflinePhoneEditAndItsReplayKeepsBoth`
- `addressEditPlacesTheFamilyAtTheNewAddressEntry`: unmapped until the geocode for the new
  address lands; a geocode for the old address landing before the edit, or after it, leaves the
  family at the new address's entry and none other.
- `typedCoordinatesSetTheEntryAndAGeocodeNeverReplacesThem`; `undoOfTypedCoordinatesRestoresThePreviousEntry`;
  `typedCoordinatesReplayedAfterAGeocodeOfThatAddressApply` (journal not abandoned).
- `batchDropsEntriesNoFamilyUsesAndKeepsEveryEntryInUse`
- `batchOfSeveralAddressesAppliesOnce`: three addresses resolved, `automaticUpdateApplied`
  emitted exactly once (`QSignalSpy` count 1).
- `failedAddressIsNotRequestedAgainUntilItsTextChanges`: not after a replay, not after another
  family is given a new address; requested again once the text changes. (After reopening the
  app is plan 2.)
- `twoFamiliesAtOneAddressShareOneRequestAndOneEntry`: exactly one request, both families
  mapped, batch finished.
- `commandThatGivesNoAddressStartsNoGeocoding`: a phone edit leaves the fake with zero requests;
  a replay of another session's address change leaves it with zero requests.
- `batchThatChangesNothingEmitsNothing`; `lookupThatChangesNothingEmitsNothing`
- `failedWardLookupContinuesToTheNextWard`: the fake records both wards' requests, in order.
- Samples for `setAddressCoordinates` and `revertAddressCoordinates`.

**Commit:** `git commit -m "Coordinates belong to addresses; geocoding and ward lookup run as automatic updates in the session"`

---

## Task 4: Teams, emergency assets and tags take the update shape

*The journal* (every update command takes this shape; *References stay whole*, first and
third bullets).

**Files:**
- Rewrite: `src/commands/TeamCommands.h/.cpp`, `src/commands/EmergencyAssetCommands.h/.cpp`,
  `src/commands/TagCommands.h/.cpp`
- Modify: `src/commands/ChangeJson.h/.cpp`, `src/commands/CommandRegistry.cpp`
- Modify: `src/widgets/TeamsView.h/.cpp`, `src/widgets/EmergencyAssetView.h/.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`, `tests/services/TestWard.h/.cpp`,
  `tests/services/CommandSamples.cpp`
- Create: `tests/services/TeamCommandsTest.h/.cpp`, `tests/services/AssetAndTagCommandsTest.h/.cpp`

**JSON.** `ChangeJson.h` gains `toJsonValue` / `fromJsonValue` for `QColor`
(`name(QColor::HexArgb)`), `std::optional<PersonId>`, `ResponseArea` (through
`responseAreaToString` / `responseAreaFromString`) and `TagLevel`, and
`template<typename Id> QJsonObject setChangeToJson(const SetChange<Id>&)` /
`setChangeFromJson<Id>(const QJsonObject&)` writing `added` and `removed` as string arrays.

**Teams.**
- `AddTeamCommand(team)`: result holds when the id is present; start condition includes that
  every member and the leader exist. Inverse `DeleteTeamCommand`.
- `UpdateTeamCommand(teamId, std::optional<Change<QString>> name, std::optional<Change<QColor>> color, std::optional<Change<std::optional<PersonId>>> leader, SetChange<PersonId> members, Description)`
  with `static fromEdit(const Team& opened, const Team& saved)`. Execute conflicts when the team
  is gone or a field moved; applies the set change; then requires the leader after, when set, to
  be on the team; requires every added id to exist as a person; and for every removed id that was
  not the leader at construction (no leader change whose before is that id), requires that it is
  not the leader now.
- `fromEdit` worked out at construction: removing the member who is leader also clears the leader,
  holding the leader's before value.
- `DeleteTeamCommand(team as last seen)`: conflicts when the team differs, result holds when it
  is gone; inverse `AddTeamCommand`.
- Deleted: `AddTeamMemberCommand` (no callers) and `RemoveTeamMemberCommand` (its one caller,
  `TeamsView::removeMemberFromTeam`, commits `UpdateTeamCommand::fromEdit` with that member
  removed).
- `TeamsView::showTeamDialog` captures the team when the dialog opens; `setLeader`, `clearLeader`
  and `removeMemberFromTeam` use the team captured when the context menu or button chose it
  (`m_contextTeam`), so a version landing under the menu's event loop makes the confirm conflict.

**Emergency assets.** `AddEmergencyAssetCommand`, `UpdateEmergencyAssetCommand` (name, response
area, people as a `SetChange<PersonId>`, added people must exist), `DeleteEmergencyAssetCommand`
as last seen. `AssignEmergencyAssetToPersonCommand` (no callers) and
`UnassignEmergencyAssetFromPersonCommand` (its caller `EmergencyAssetView::removePersonFromAsset`
moves to `UpdateEmergencyAssetCommand::fromEdit`) are deleted. `showAssetDialog` captures the
asset at open.

**Tags.** `AddTagCommand`, `UpdateTagCommand` (name, color, level, people and families as set
changes, added ids must exist), `DeleteTagCommand` as last seen. `AssignTagToPersonCommand`,
`AssignTagToFamilyCommand`, `UnassignTagFromPersonCommand` and `UnassignTagFromFamilyCommand`
are deleted.

**Tests:**
- `twoUsersAddingDifferentMembersToOneTeamBothLand`
- `memberRemovalReplayedAfterTheyBecameLeaderConflictsAndLeavesThemMemberAndLeader`
- `removingTheLeaderClearsTheLeaderAndUndoRestoresBoth`
- `leaderChangeNeedsTheNewLeaderOnTheTeam`
- `additionOfAPersonReplayedAfterTheyWereRemovedConflicts` (removal through `UpdateFamilyCommand`;
  afterwards no team holds an id the document lacks)
- `teamDeletionReplayedAfterSomeoneAddedAMemberConflictsAndLeavesTheTeam`
- `assetEditsAddingDifferentPeopleBothLand`; `tagRemovalOfAnAbsentPersonIsDone`
- Samples for every new type.

**Commit:** `git commit -m "Teams, assets and tags hold their changed fields and set changes only"`

---

## Task 5: Emergency responses are a keyed collection changed only by commands

*The journal* (the emergency bullets and command table), *Replay* (*An emergency needs no
special case*, *Ending an emergency turns it into its report*), *The record of an emergency*,
*Saving* (undo rules for `StartEmergency`, `AddTaskCategory`, `EndEmergency`), *Testing* (the
clock).

**Files:**
- Create: `src/services/Clock.h`, `src/services/Clock.cpp` (`class Clock { virtual QDateTime now() const = 0; }`, `SystemClock`)
- Modify: `src/models/Id.h` (`EmergencyResponseId`), `src/models/EmergencyResponse.h/.cpp`,
  `src/models/Document.h/.cpp`
- Create: `src/commands/EmergencyCommands.h`, `src/commands/EmergencyCommands.cpp`
- Modify: `src/commands/CommandRegistry.cpp`, `src/commands/ChangeJson.h/.cpp`
- Modify: `src/services/WardSession.h/.cpp`, `src/services/EmergencyManager.h/.cpp`,
  `src/services/DocumentManager.h/.cpp`, `src/services/ReportGenerator.h/.cpp`
- Delete: `src/widgets/ArchiveBrowserDialog.h/.cpp`
- Modify: `src/widgets/MainWindow.h/.cpp`, `src/widgets/EmergencyBanner.h/.cpp`,
  `src/widgets/EmergencyProgressBar.cpp`, `src/widgets/ActionButtonsWidget.cpp`,
  `src/widgets/ContactAttemptDialog.cpp`, `src/widgets/TaskDialog.cpp`,
  `src/widgets/TaskListView.cpp`, `src/widgets/WardListView.cpp`, `src/widgets/TeamsView.cpp`,
  `src/listmodels/TaskListModel.cpp`, `src/listmodels/FamilyTreeModel.cpp`,
  `src/listmodels/MinisteringModel.cpp`, `src/listmodels/TeamsTreeModel.cpp`
- Modify: `docs/USER_GUIDE.md` (the emergency sections: several emergencies in progress, ending
  saves the report, no archives)
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/main.cpp`, `tests/services/CommandSamples.cpp`
- Create: `tests/services/FakeClock.h/.cpp`, `tests/services/EmergencyCommandsTest.h/.cpp`

**Model.**
- `Document` holds `QHash<EmergencyResponseId, EmergencyResponse>` with `emergencyResponses()`,
  `findEmergencyResponse`, `addEmergencyResponse`, `updateEmergencyResponse`,
  `removeEmergencyResponse`; `emergencyResponse()` and `setEmergencyResponse` are deleted. JSON
  key `emergencyResponses`.
- `EmergencyResponse` gains `m_id`; `create(const QString& name, const QDateTime& startedAt)`;
  `m_endedAt`, `endedAt`, `setEndedAt`, `clearEndedAt` are deleted.
- `ContactAttempt`: `std::optional<ContactMethod> method`, `std::optional<PersonId> who`, and
  `std::optional<ContactOutcome> outcome` with `enum class ContactOutcome { OK, UnableToReach }`;
  `create(...)` takes the timestamp. `ContactStatus`, `m_contactStatus` and `setContactStatus`
  are deleted. `effectiveStatus()`: Needs Help while a task is unresolved, else the outcome of the
  first attempt that has one, else Not Contacted.
- `FamilyResponseRecord::insertContactAttempt` places an attempt at its place, greatest timestamp
  first, ties by `ContactAttemptId`; `insertTask` likewise by `createdAt`, ties by `TaskId`.
  `addContactAttempt` and `addTask` (`prepend`) are deleted.
- `ResponseTask` holds `TaskAssignment` (assignee team or person, notes, notification) and
  `TaskResolution` (resolved, notes, time) as values with `operator==`; `assignToTeam`,
  `assignToPerson`, `clearAssignment`, `setNotification`, `resolve`, `reopen` and `mutableTask`
  are replaced by `setAssignment` / `setResolution`. `TaskNotification::create`,
  `ResponseTask::create` take the time.
- Statistics: `EmergencyResponse::totalFamilies(const QHash<FamilyId, Family>& families)` and
  `countByStatus(EffectiveContactStatus status, const QHash<FamilyId, Family>& families)` count
  the records plus every directory family without one as Not Contacted.

**Commands** (`EmergencyCommands.h`), each with `TypeName`, the outcome rules of the design's
table, and a missing response making every command but `EndEmergency` conflict:
- `StartEmergencyCommand(response)`: an addition; not undoable.
- `EndEmergencyCommand(response as the report was written from)`: conflicts when the response
  differs; result holds when it is gone; inverse `RestoreEmergencyCommand`.
- `RestoreEmergencyCommand(response)`: result holds when a response with that id is present.
- `RecordContactAttemptCommand(responseId, familyId, displayName, address, attempt)`: creates the
  record from its snapshot when missing; inserts in order; result holds when the attempt id is
  present; inverse `DeleteContactAttemptCommand`.
- `DeleteContactAttemptCommand(responseId, familyId, attempt as last seen)`.
- `AddTaskCommand(responseId, familyId, displayName, address, task)`, inverse `DeleteTaskCommand`.
- `UpdateTaskCommand(responseId, familyId, taskId, category, description, assignment, resolution)`,
  each an `std::optional<Change<...>>`; a new assignee must exist (person or team); a change of
  assignee holds the notification as none.
- `DeleteTaskCommand(responseId, familyId, task as last seen)`.
- `AddTaskCategoryCommand(responseId, category)`: result holds when present; not undoable.

**Session and managers.**
- `WardSession(Geocoder*, UnitLookup*, Clock* clock, QObject* parent)`; `const Clock& clock() const`.
  `DocumentManager` passes a `SystemClock`; command builders read `now()` at construction.
- `WardSession::endEmergency(const EmergencyResponseId& id, const QString& reportPath, const QString& wardName)`
  returns `enum class EndResult { Ended, AlreadyEnded, ReportFailed, Conflicted }`: with no event
  loop between, reads the response by id (`AlreadyEnded` when gone), writes the report through
  `ReportGenerator::generateReport(response, families, wardName, clock().now(), reportPath)`
  (`ReportFailed`, nothing journalled, when it cannot be written), then commits
  `EndEmergencyCommand`.
- `EmergencyManager` keeps `std::optional<EmergencyResponseId> m_shownId`, reads the response
  from the document, and turns each of today's mutators into building and committing the command;
  `m_response`, `persistResponseData`, `syncFromDocument` and `syncFamilies` are deleted.
- All archive code is deleted, as listed in the notes: `EmergencyManager`'s `m_savedResponse`,
  `loadArchive`, `closeArchive`, `reopenArchive`, `saveArchive`, `isViewingArchive`,
  `archiveViewOpened`, `archiveViewClosed` and `endEmergency`'s `archive` parameter;
  `DocumentManager`'s `m_archiveDocument`, `setArchiveDocument`, `clearArchiveDocument`,
  `isViewingArchive` and `setEmergencyResponse` / `saveDocumentOnly`; `ArchiveBrowserDialog`;
  `EmergencyBanner::setArchiveName`; `MainWindow::onOpenArchive`, `onArchiveViewOpened`,
  `onArchiveViewClosed`, `m_openArchiveAction`, `m_closeArchiveAction` and their menu items; the
  archive connections in `FamilyTreeModel`, `MinisteringModel`, `TeamsTreeModel` and
  `WardListView`; the `isViewingArchive` checks in `ActionButtonsWidget`, `TeamsView` and
  `WardListView`.
- `MainWindow::onEndEmergency`: one End confirmation, then the report's save dialog (cancelling
  ends nothing), then `endEmergency`; `AlreadyEnded` tells the user someone else ended it.
  `generateEmergencyReportWithConfirm` reads the response by id after its dialog rather than
  holding a reference across it.
- The interface lists the emergencies in progress and switches between them (open question 12).
- UI sites build their commands from what was captured when the target was chosen: the OK and
  Unable to Reach buttons (`ActionButtonsWidget::onOkClicked` / `onUnableToReachClicked`) record
  an attempt carrying only the outcome; `ContactAttemptDialog` fills method and who; the task
  dialog, resolve, reopen, assign, unassign and notify in `WardListView`, `TaskListView`,
  `TeamsView`, `TaskDialog` and `TaskListModel::setData` build `UpdateTaskCommand` from the task
  captured at open.
- `ReportGenerator`: tallies an attempt with no method on its own line, *Marked OK* or *Marked
  Unable to Reach*, and draws its overview from `totalFamilies(families)` / `countByStatus`.

**Tests** (`EmergencyCommandsTest`, with `FakeClock` returning times the test sets):
- `twoRespondersOfflineLandEveryAttempt`: exact attempt counts per family and each attempt's notes.
- `twoRespondersOnAFamilyWithNoRecordLandInOneRecord`
- `differentOutcomesOnOneFamilyShowTheLaterTimestampWhicheverReplaysFirst`: neither journal
  abandoned; both orders give equal documents with the attempts newest first.
- `twoOfflineStartsMakeTwoEvents`, each holding its own responder's attempts.
- `endingWritesAReportHoldingEveryAttemptAndTaskThenRemovesTheResponse`: the report's text, read
  back through `PdfExtractor`, contains each attempt's notes and each task's description.
- `attemptsReplayedOntoAnEndedResponseAbandonToTheToRedoList`: the group holds each attempt's
  description.
- `endReplayedAfterAnotherAttemptConflictsAndLeavesTheResponse`
- `undoingAnEndRestoresAnEqualResponseAndLeavesTheReportFile`
- `endWhoseReportCannotBeWrittenEndsNothing`: `ReportFailed`, response present, journal unchanged.
- `taskDeletionReplayedAfterAnotherResolvedItConflicts`: the task stays resolved with its notes.
- `taskEditsToDifferentValuesBothApply`; `oneTaskValueSetDifferentlyConflicts`
- `assignmentToAPersonReplayedAfterTheyWereRemovedConflicts`
- `changingTheAssigneeClearsTheNotification`
- `statisticsCountFamiliesWithoutARecordAsNotContacted`
- Samples for every emergency command type.

**Commit:** `git commit -m "Emergency responses are a keyed collection, changed only by commands; ending saves the report and removes the response"`

---

## Task 6: References stay whole

*The journal* (*References stay whole*, removals as last seen), *Deleting a family*.

**Files:**
- Create: `src/commands/ReferenceCleanup.h`, `src/commands/ReferenceCleanup.cpp`
- Modify: `src/commands/FamilyChange.h/.cpp`, `src/commands/FamilyCommands.h/.cpp`,
  `src/commands/TeamCommands.h/.cpp`, `src/commands/CommandRegistry.cpp`
- Modify: `src/models/Document.h/.cpp` (`cleanupPersonReferences` has no callers and is deleted)
- Modify: `src/widgets/MainWindow.cpp` (`onDeleteFamilyRequested`, `onSaveFamily`)
- Modify: `tests/...` (CMake, main, samples)
- Create: `tests/services/ReferenceCleanupTest.h/.cpp`

**`ReferenceCleanup`** holds, for a set of removed people and families, every place that holds
them, as seen at construction: team memberships and leaders, person-level tags, assets, ministering
groups (ministers, ministered people, presidency) and districts (presidency), and task assignments
(response, family, task, and the assignment before); for families, family-level tags and groups'
families. API:
- `static ReferenceCleanup of(const Document& document, const QSet<PersonId>& people, const QSet<FamilyId>& families)`
- `static ReferenceCleanup ofTeam(const Document& document, const TeamId& team)` (task assignments
  to that team)
- `bool holds(const Document& current) const`: the places holding them now equal these, or none do
- `void apply(Document& document) const`; `bool restoreHolds(const Document& current) const`;
  `void restore(Document& document) const`
- `toJson`, `fromJson`, `operator==`

**Users.**
- `FamilyChange` holds a `ReferenceCleanup` for its removed members; `FamilyChange::between` and
  `UpdateFamilyCommand::fromEdit` take the current `const Document&` to work it out at
  construction; `holds` includes it; the inverse restores it.
- `DeleteFamilyCommand(family as last seen, cleanup)` (`TypeName = "deleteFamily"`) and its
  inverse `RestoreFamilyCommand` (result holds when the family is present). The Delete Family
  action (`MainWindow::onDeleteFamilyRequested`) captures the family before its confirmation box
  and commits `DeleteFamilyCommand`; its "Delete not yet implemented" message goes.
- `DeleteTeamCommand` holds `ReferenceCleanup::ofTeam`, unassigning tasks assigned to the team;
  its inverse assigns them again.

**Tests:**
- `deletingAFamilyRemovesItAndItsPeopleFromEveryPlaceAndUndoRestoresEach` (family-level tag,
  group families; its people in teams, person-level tags, assets, ministering, task assignments)
- `removingAMemberThroughTheEditPanelCleansUpThatPerson`
- `deletingATeamUnassignsItsTasksAndUndoAssignsThemAgain`
- `removalReplayedAfterThePersonJoinedAnotherTeamConflicts` and
  `removalReplayedAfterThePersonWasAssignedATaskConflicts`; in each, no team or task is left
  holding a missing id
- Samples for `deleteFamily`, `restoreFamily`, and the changed `updateFamily` and `deleteTeam`.

**Commit:** `git commit -m "Removals carry their cleanup as a start condition, and Delete Family is a command"`

---

## Task 7: Imports work out their changes at construction

*Imports*.

**Files:**
- Rewrite: `src/commands/ImportWardDirectoryCommand.h/.cpp`, `src/commands/ImportMinisteringCommand.h/.cpp`
- Modify: `src/services/WardDirectoryImportService.h/.cpp`, `src/services/MinisteringImportService.h/.cpp`,
  `src/services/WardDirectoryPdfParser.h/.cpp` (coordinates, per open question 10)
- Modify: `src/models/Document.h/.cpp` (`setFamilies` and `addFamilies` lose their last callers
  and are deleted)
- Modify: `src/widgets/MainWindow.cpp` (`onImportPdf`, `onImportMinisteringPdf`)
- Modify: `docs/IMPORT_DESIGN.md`
- Modify: `tests/...`; Create: `tests/services/ImportCommandsTest.h/.cpp`

**Directory import.**
- `WardDirectoryImportService` gains a public `merge(parsed, existing, pdfDate, ministeringPdfDate)`
  that `importFromPdf` calls after parsing, so tests reach it without a PDF. `preserveIds` copies
  the existing person's special-need note; each existing family is matched at most once.
- `ImportWardDirectoryCommand::fromImport(const Document& document, const WardDirectoryImportResult& result, const QString& fileName)`
  holds the families added, a `FamilyChange` per family that differs (none for the rest), the
  families removed as last seen with their `ReferenceCleanup`, the directory PDF date as a
  `Change<std::optional<QDate>>`, and the ward record created if missing (result holds when one is
  there). An import never changes a special-need note.
- Execute: every change's start condition or result must hold, or it conflicts touching nothing;
  a family someone else added meanwhile is not among its removals.
- Inverse: the same changes swapped (open question 11 for the ward record).
- `addressesGiven()` returns the addresses of the families it adds or re-addresses.
- Description: heading `tr("Import the ward directory from %1.").arg(fileName)`, no lines.

**Ministering import.**
- `ImportMinisteringCommand::fromImport(document, result, fileName)` holds the organisation, its
  districts and groups as one `Change<MinisteringCollection>` (the whole collection before and
  after, with `operator==` and JSON), the families added, a `FamilyChange` per family that
  differs, and the ministering PDF date. Groups and districts may only name people and families
  that exist after it applies.
- `MinisteringImportService::mergeFamilies`, when not authoritative, adds no unmatched family or
  person and leaves them out of the groups and districts it builds
  (`MinisteringImportService.cpp:156-164`, `:330-334`).
- Description: `tr("Import the ministering assignments from %1.").arg(fileName)`.

**Docs.** `docs/IMPORT_DESIGN.md`: the special-need note and the coordinates are the app's own and
an import keeps them; a ministering import adds someone it cannot match only when it is the newer
source.

**Tests:**
- `importReplayedAfterSomeoneAddedAFamilyKeepsThatFamily` (asserted by id)
- `importReplayedAfterAnEditToAFamilyItChangesConflicts`
- `undoingADirectoryImportRestoresTheRemovedPeoplesMemberships` (team, tag, asset, ministering)
- `reimportingTheSameDirectoryKeepsNotesAndCoordinatesAndHoldsNoChange`
- `olderMinisteringPdfAddsNoUnknownFamilyOrMemberAndNoGroupNamesAMissingId`
- `ministeringImportReplayedAfterACleanupChangedAGroupConflicts` (the group keeps the cleanup's
  result)
- `attemptOnAFamilyWithNoRecordLandsAfterAnImportRemovedIt` (with its construction-time snapshot)
- `importDescriptionIsOneLineNamingTheFile`
- Samples for both import types.

**Commit:** `git commit -m "Imports work out every change at construction and replay like any command"`

---

## Open questions for the user

The design is silent on these; each states the choice this plan makes.

1. **Command samples live in the test tree** (`tests/services/CommandSamples.cpp`), keyed by type
   name, and the round-trip test fails for a registered type with no sample. The design says each
   type "registers a sample"; this keeps test documents out of the app's library.
2. **A first-run conflict** is shown once as a message with its description and does not go on
   the to-redo list, which holds abandoned replays.
3. **Description wording:** an emptied value reads `(none)`; member additions and removals read
   `added: Smith, Dan` / `removed: Smith, Alice`; the parent flag reads `Bob, parent` /
   `Bob, not a parent`.
4. **Member field granularity:** phone and other phone, and ward and stake unit numbers, are
   each judged on their own (the design lists "phones" and "units").
5. **The undo stack pairs each command with its inverse, built at commit.** `inverse()` reads no
   document, so this equals building it at undo.
6. **An abandoned replay clears the redo side too**, alongside the undo stack.
7. **The to-redo list's interface** (*These need doing again*, with Dismiss): nothing in plan 1's
   app abandons a replay, so this plan builds the list and leaves its window to plan 3, where
   fetching first abandons one. Or build it now?
8. **Landed marks are plan 3**, so after a successful replay the entries stay in the journal and
   a later replay runs them again, where their result already holds.
9. **Typed coordinates in the family edit panel** commit as a second command after the family
   edit, so one save can make two undo entries.
10. **The directory PDF's own coordinates:** seed the table for addresses it lacks through the
    import command (never replacing an entry), or drop them and let the geocoder fill the table?
11. **Undoing a directory import that created the ward record:** the lookup fills that record's
    details afterwards, so an inverse that removes it "as last seen" would conflict after every
    lookup. Remove it whatever details it holds, or leave it in place?
12. **Switching between emergencies in progress:** a combo box in `EmergencyBanner`?
13. **The test target is `WardPlanningTests`**, linked against `EmergencyPlanLib`, which plan 2's
    rename turns into `WardPlanningLib`.
14. **`docs/SHARED_WARD_PLAN_NOTES.md` stays** for plans 2 and 3, though it says it is deleted
    once the plan is written.
