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

    // First name matching - disqualify if no match at all
    if (sourceFirst == targetFirst)
    {
        score += 10;
    }
    else if (sourceFirst.startsWith(targetFirst) || targetFirst.startsWith(sourceFirst))
    {
        // Partial match (Mike/Michael, Bob/Robert won't match this way, but Tom/Thomas might)
        score += 5;
    }
    else
    {
        // No first name match at all - disqualify
        return 0;
    }

    // Gender mismatch - disqualify if both have gender and they differ
    if (source.gender().has_value() && target.gender().has_value()
        && source.gender().value() != target.gender().value())
    {
        return 0;
    }

    // Birth month+day matching (not year - year is often missing for adults)
    if (source.birthday().hasDate() && target.birthday().hasDate()
        && source.birthday().month() == target.birthday().month()
        && source.birthday().day() == target.birthday().day())
    {
        score += 7;
    }

    // Both are parents
    if (source.isParent() && target.isParent())
    {
        score += 2;
    }

    // Phone match
    if (!source.phone().isEmpty() && source.phone().matches(target.phone()))
    {
        score += 5;
    }

    // Email match
    if (!source.email().isEmpty() && !target.email().isEmpty()
        && source.email().compare(target.email(), Qt::CaseInsensitive) == 0)
    {
        score += 5;
    }

    return score;
}

PersonMatchScore findBestPersonMatch(const Person& source, const Family& family)
{
    constexpr int minimumScore = 10;
    PersonMatchScore best;

    for (const Person& member : family.members())
    {
        int score = scorePersonMatch(source, member);
        if (score >= minimumScore && score > best.score)
        {
            best.personId = member.id();
            best.score = score;
        }
    }

    return best;
}

FamilyMemberMatchResult findFamilyByMembers(
    const Family& sourceFamily,
    const QHash<QString, Family>& targetFamilies)
{
    FamilyMemberMatchResult bestResult;
    bestResult.totalSourceMembers = sourceFamily.members().size();

    QString sourceSurname = sourceFamily.surname().toLower();

    // Filter by surname first
    for (const Family& targetFamily : targetFamilies)
    {
        if (targetFamily.surname().toLower() != sourceSurname)
        {
            continue;
        }

        // Count matching members using scored matching
        FamilyMemberMatchResult current;
        current.familyId = targetFamily.id();
        current.totalSourceMembers = sourceFamily.members().size();

        for (const Person& sourceMember : sourceFamily.members())
        {
            PersonMatchScore match = findBestPersonMatch(sourceMember, targetFamily);
            if (match.score >= 10)  // Uses the minimum threshold
            {
                current.matchedMembers++;
                current.personIdMapping.insert(sourceMember.id(), match.personId);
            }
        }

        // Keep best match
        if (current.matchedMembers > bestResult.matchedMembers)
        {
            bestResult = current;
        }
    }

    // Check majority rule: more than half must match
    if (bestResult.matchedMembers > 0
        && bestResult.matchedMembers > bestResult.totalSourceMembers / 2)
    {
        return bestResult;
    }

    // No majority match
    bestResult.familyId.clear();
    bestResult.personIdMapping.clear();
    return bestResult;
}

}  // namespace PersonMatching
