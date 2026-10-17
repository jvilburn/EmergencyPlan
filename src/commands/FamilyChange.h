#pragma once

#include "FieldChange.h"
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

    std::optional<FieldChange<Name>> m_name;
    std::optional<FieldChange<std::optional<Gender>>> m_gender;
    std::optional<FieldChange<Phone>> m_phone;
    std::optional<FieldChange<Phone>> m_altPhone;
    std::optional<FieldChange<QString>> m_email;
    std::optional<FieldChange<Birthday>> m_birthday;
    std::optional<FieldChange<QStringList>> m_callings;
    std::optional<FieldChange<bool>> m_isParent;
    std::optional<FieldChange<QString>> m_wardUnitNumber;
    std::optional<FieldChange<QString>> m_stakeUnitNumber;
    std::optional<FieldChange<QString>> m_specialNeedNote;
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

    FamilyChange(const std::optional<FieldChange<Address>>& address,
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
    std::optional<FieldChange<Address>> m_address;
    QHash<PersonId, MemberChange> m_members;
    QList<Person> m_added;
    QList<Person> m_removed;
};
