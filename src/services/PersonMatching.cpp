#include "PersonMatching.h"

#include <QSet>

namespace
{

/// A household's contact keys, each prefixed by its kind so a street never equals a phone.
QSet<QString> keysOf(const Family& family)
{
    QSet<QString> keys;
    if (!family.address().lines().isEmpty())
    {
        QString street = family.address().lines().first().simplified().toLower();
        if (!street.isEmpty())
        {
            keys.insert("street:" + street);
        }
    }
    for (const Person& member : family.members())
    {
        for (const Phone& phone : {member.phone(), member.altPhone()})
        {
            // The last ten digits, so "+13366080008" and "336-608-0008" agree.
            QString digits = phone.digits().right(10);
            if (digits.size() >= 7)
            {
                keys.insert("phone:" + digits);
            }
        }
        QString email = member.email().trimmed().toLower();
        if (!email.isEmpty())
        {
            keys.insert("email:" + email);
        }
    }
    return keys;
}

bool sameGivenNames(const Person& a, const Person& b)
{
    return a.givenNames().compare(b.givenNames(), Qt::CaseInsensitive) == 0;
}

bool sameFirstGivenName(const Person& a, const Person& b)
{
    return a.name().firstName().compare(b.name().firstName(), Qt::CaseInsensitive) == 0;
}

/// The index in target of the member person is, among those not yet used that names accepts;
/// none when there is none.
std::optional<qsizetype> takeMatch(const Person& person, const QList<Person>& target, QSet<qsizetype>& used,
                                   bool (*names)(const Person&, const Person&))
{
    std::optional<qsizetype> chosen;
    for (qsizetype i = 0; i < target.size(); ++i)
    {
        const Person& member = target[i];
        if (used.contains(i) || !names(person, member))
        {
            continue;
        }
        if (person.gender().has_value() && member.gender().has_value() && person.gender() != member.gender())
        {
            continue;
        }
        bool sameBirthday = person.birthday().hasDate() && member.birthday().hasDate()
            && person.birthday().month() == member.birthday().month()
            && person.birthday().day() == member.birthday().day();
        if (sameBirthday)
        {
            chosen = i;
            break;
        }
        if (!chosen.has_value())
        {
            chosen = i;
        }
    }
    if (chosen.has_value())
    {
        used.insert(*chosen);
    }
    return chosen;
}

}  // namespace

namespace PersonMatching
{

std::optional<FamilyId> findFamily(const Family& source, const QHash<FamilyId, Family>& families)
{
    QSet<QString> sourceKeys = keysOf(source);
    QList<FamilyId> candidates;
    for (const Family& family : families)
    {
        if (keysOf(family).intersects(sourceKeys))
        {
            candidates.append(family.id());
        }
    }
    bool keyed = !candidates.isEmpty();
    if (!keyed)
    {
        for (const Family& family : families)
        {
            if (family.surname().compare(source.surname(), Qt::CaseInsensitive) == 0)
            {
                candidates.append(family.id());
            }
        }
    }

    std::optional<FamilyId> best;
    qsizetype bestMatched = -1;
    for (const FamilyId& id : candidates)
    {
        qsizetype matched = matchMembers(source, families.value(id)).size();
        if (!keyed && (matched == 0 || matched < (source.members().size() + 1) / 2))
        {
            continue;
        }
        if (matched > bestMatched || (matched == bestMatched && id < *best))
        {
            best = id;
            bestMatched = matched;
        }
    }
    return best;
}

QHash<PersonId, PersonId> matchMembers(const Family& source, const Family& target)
{
    const QList<Person>& members = target.members();
    QSet<qsizetype> used;
    QHash<PersonId, PersonId> mapping;
    for (const Person& person : source.members())
    {
        if (person.name().firstName().isEmpty())
        {
            continue;
        }
        std::optional<qsizetype> match = takeMatch(person, members, used, sameGivenNames);
        if (match.has_value())
        {
            mapping.insert(person.id(), members[*match].id());
        }
    }
    for (const Person& person : source.members())
    {
        if (mapping.contains(person.id()) || person.name().firstName().isEmpty())
        {
            continue;
        }
        std::optional<qsizetype> match = takeMatch(person, members, used, sameFirstGivenName);
        if (match.has_value())
        {
            mapping.insert(person.id(), members[*match].id());
        }
    }
    return mapping;
}

}  // namespace PersonMatching
