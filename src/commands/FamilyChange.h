#pragma once

#include "FieldChange.h"
#include "Family.h"
#include "ReferenceCleanup.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QtGlobal>
#include <optional>

class Document;

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

    /// The one list of a member's fields. Calls visitor once per field, in this order, with
    /// the field's JSON key, its description line (untranslated, %1 the first name and %2
    /// the value set), its Person getter and setter, and change's value for it. Change is
    /// MemberChange or const MemberChange, so a visitor can read or set the values.
    template<typename Change, typename Visitor>
    static void forEachField(Change& change, Visitor& visitor)
    {
        visitor("name", QT_TRANSLATE_NOOP("QObject", "%1, name: %2"),
                &Person::name, &Person::setName, change.m_name);
        visitor("gender", QT_TRANSLATE_NOOP("QObject", "%1, gender: %2"),
                &Person::gender, &Person::setGender, change.m_gender);
        visitor("phone", QT_TRANSLATE_NOOP("QObject", "%1, phone: %2"),
                &Person::phone, &Person::setPhone, change.m_phone);
        visitor("altPhone", QT_TRANSLATE_NOOP("QObject", "%1, other phone: %2"),
                &Person::altPhone, &Person::setAltPhone, change.m_altPhone);
        visitor("email", QT_TRANSLATE_NOOP("QObject", "%1, email: %2"),
                &Person::email, &Person::setEmail, change.m_email);
        visitor("birthday", QT_TRANSLATE_NOOP("QObject", "%1, birthday: %2"),
                &Person::birthday, &Person::setBirthday, change.m_birthday);
        visitor("callings", QT_TRANSLATE_NOOP("QObject", "%1, callings: %2"),
                &Person::callings, &Person::setCallings, change.m_callings);
        visitor("isParent", QT_TRANSLATE_NOOP("QObject", "%1, %2"),
                &Person::isParent, &Person::setIsParent, change.m_isParent);
        visitor("wardUnitNumber", QT_TRANSLATE_NOOP("QObject", "%1, ward unit: %2"),
                &Person::wardUnitNumber, &Person::setWardUnitNumber, change.m_wardUnitNumber);
        visitor("stakeUnitNumber", QT_TRANSLATE_NOOP("QObject", "%1, stake unit: %2"),
                &Person::stakeUnitNumber, &Person::setStakeUnitNumber, change.m_stakeUnitNumber);
        visitor("specialNeedNote", QT_TRANSLATE_NOOP("QObject", "%1, special need: %2"),
                &Person::specialNeedNote, &Person::setSpecialNeedNote, change.m_specialNeedNote);
        visitor("note", QT_TRANSLATE_NOOP("QObject", "%1, note: %2"),
                &Person::note, &Person::setNote, change.m_note);
    }

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
    std::optional<FieldChange<QString>> m_note;
};

/// A member a family change adds or removes, with its place in the family's member order:
/// for an addition, the index it is inserted at; for a removal, the index it was seen at,
/// where the removal's inverse inserts it again.
struct PlacedMember
{
    Person person;
    qsizetype index;
};

/// What an edit changes in one family: its address, its note, a change per member keyed by
/// PersonId, the members it adds, and the members it removes as last seen. Members are
/// matched by PersonId. A family's member order is the order it was given, and a change
/// keeps it: it inserts each addition at its index and moves no one else. Order is not part
/// of any start condition.
///
/// It also holds a ReferenceCleanup for the members it removes, which it applies, and one
/// for the members it adds, whose held places it restores; swapping exchanges the two, so
/// the change that takes a removal back restores the places the removal held. Neither is
/// part of its start condition.
class FamilyChange
{
public:
    /// The difference between the family as opened and as saved, or none when nothing
    /// differs. A member the saved family has and the opened one lacks is added at its index
    /// in the saved family. Each cleanup holds the places in document that refer to the
    /// members it names.
    static std::optional<FamilyChange> between(const Family& opened, const Family& saved, const Document& document);

    FamilyChange(const std::optional<FieldChange<Address>>& address,
                 const std::optional<FieldChange<QString>>& note,
                 const QHash<PersonId, MemberChange>& members,
                 const QList<PlacedMember>& added,
                 const QList<PlacedMember>& removed,
                 const ReferenceCleanup& addedCleanup,
                 const ReferenceCleanup& removedCleanup);

    /// The start condition or the result holds: the address, the note and every changed
    /// member are at before or after, and every removed member is gone or exactly as last
    /// seen. An added member never conflicts; its result holds when that PersonId is already
    /// there.
    bool holds(const Family& current) const;

    /// current with the removed members taken out, the member changes made, and then each
    /// added member not already there inserted, in order, at its index, clamped to the
    /// member count at that point.
    Family appliedTo(const Family& current) const;

    /// Puts appliedTo(current) in document, takes the removed members out of every place
    /// that holds them, and restores the places the added members' cleanup holds.
    void applyTo(Document& document, const Family& current) const;

    /// The change that takes this one back: the address, note and member changes swapped,
    /// the removed members added again at the indices they were seen at, the added ones
    /// removed, and the two cleanups exchanged.
    FamilyChange swapped() const;

    /// True when running it can change a place outside the family: it removes a member, or
    /// restores a held place.
    bool reachesOtherPlaces() const;

    /// The held places the added members' cleanup would leave on document.
    QStringList skippedOn(const Document& document) const;

    /// The lines of what to do again: member lines in the opened family's member order,
    /// then additions, removals, the address and the note.
    QStringList describe(const Family& opened) const;

    const std::optional<FieldChange<Address>>& address() const { return m_address; }

    QJsonObject toJson() const;
    static FamilyChange fromJson(const QJsonObject& json);

private:
    std::optional<FieldChange<Address>> m_address;
    std::optional<FieldChange<QString>> m_note;
    QHash<PersonId, MemberChange> m_members;
    QList<PlacedMember> m_added;
    QList<PlacedMember> m_removed;
    ReferenceCleanup m_addedCleanup;
    ReferenceCleanup m_removedCleanup;
};

/// What an import changes in the ward's families: the families it adds, a FamilyChange per
/// family that differs, and the families it removes as last seen, with a ReferenceCleanup
/// for the families it removes and their people, which it applies, and one for the families
/// it adds and their people, whose held places it restores.
class FamilyCollectionChange
{
public:
    /// The change from the families document holds to imported, the families as an import
    /// leaves them: each one document lacks is added, and each one that differs gets the
    /// change between, which keeps the imported family's member order. Each family in removed
    /// that document holds is removed as document holds it. Each cleanup holds the places in
    /// document that refer to the families and people it names.
    static FamilyCollectionChange between(const Document& document,
                                          const QHash<FamilyId, Family>& imported,
                                          const QSet<FamilyId>& removed);

    FamilyCollectionChange(const QHash<FamilyId, Family>& added,
                           const QHash<FamilyId, FamilyChange>& changed,
                           const QHash<FamilyId, Family>& removed,
                           const ReferenceCleanup& addedCleanup,
                           const ReferenceCleanup& removedCleanup);

    bool isEmpty() const;

    /// The start condition or the result holds: every changed family is there and its
    /// change holds, and every removed family is gone or exactly as last seen. An added
    /// family never conflicts; its result holds when a family with its id is there.
    bool holds(const Document& current) const;

    /// Adds each added family that is not there, makes each change, removes each removed
    /// family, takes the removed families and their people out of every place that holds
    /// them, and restores the places the added families' cleanup holds. Called when holds is
    /// true.
    void applyTo(Document& document) const;

    /// The change that takes this one back: the added families removed as they were added,
    /// the changes swapped, the removed families added again, and the two cleanups
    /// exchanged.
    FamilyCollectionChange swapped() const;

    /// The held places the added families' cleanup, and each change's, would leave on
    /// document.
    QStringList skippedOn(const Document& document) const;

    /// The non-empty addresses of the families it adds, and of the changes that set one.
    QList<Address> addressesGiven() const;

    QJsonObject toJson() const;
    static FamilyCollectionChange fromJson(const QJsonObject& json);

private:
    QHash<FamilyId, Family> m_added;
    QHash<FamilyId, FamilyChange> m_changed;
    QHash<FamilyId, Family> m_removed;
    ReferenceCleanup m_addedCleanup;
    ReferenceCleanup m_removedCleanup;
};
