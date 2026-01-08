#include "PersonMatching.h"
#include "Name.h"

namespace PersonMatching
{

FamilyMatchResult findBestFamilyMatch(
    const QString& familyName,
    const QString& address,
    const QString& phone,
    const QList<Family>& existingFamilies)
{
    Q_UNUSED(familyName)
    Q_UNUSED(address)
    Q_UNUSED(phone)
    Q_UNUSED(existingFamilies)

    // TODO: Implement multi-factor scoring
    return {};
}

std::optional<Person> findPersonInFamilies(
    const QString& personName,
    const QList<Family>& families)
{
    // First try exact match
    for (const Family& family : families)
    {
        for (const Person& person : family.members())
        {
            if (Name::match(person.displayName(), personName))
            {
                return person;
            }
        }
    }

    // If no exact match, try component-based matching (ignores middle names)
    // e.g., "Smith, John" matches "Smith, John Michael"
    for (const Family& family : families)
    {
        for (const Person& person : family.members())
        {
            if (Name::matchComponents(person.displayName(), personName))
            {
                return person;
            }
        }
    }

    return std::nullopt;
}

int scorePersonMatch(const Person& source, const Person& target)
{
    int score = 0;

    QString sourceFirst = source.givenNames().split(' ').first().toLower();
    QString targetFirst = target.givenNames().split(' ').first().toLower();

    // First name matching
    if (sourceFirst == targetFirst)
    {
        score += 100;
    }
    else if (sourceFirst.startsWith(targetFirst) || targetFirst.startsWith(sourceFirst))
    {
        // Partial match (Mike/Michael, Bob/Robert won't match this way, but Tom/Thomas might)
        score += 50;
    }
    else
    {
        // No first name match at all - probably not the same person
        return 0;
    }

    // Birth month+day matching (not year - year is often missing for adults)
    if (source.birthday().hasDate() && target.birthday().hasDate()
        && source.birthday().month() == target.birthday().month()
        && source.birthday().day() == target.birthday().day())
    {
        score += 50;
    }

    // Both are parents
    if (source.isParent() && target.isParent())
    {
        score += 20;
    }

    return score;
}

PersonMatchScore findBestPersonMatch(const Person& source, const Family& family)
{
    PersonMatchScore best;

    for (const Person& member : family.members())
    {
        int score = scorePersonMatch(source, member);
        if (score > best.score)
        {
            best.personId = member.id();
            best.score = score;
        }
    }

    return best;
}

}  // namespace PersonMatching
