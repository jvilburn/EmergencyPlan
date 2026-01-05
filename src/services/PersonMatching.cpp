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

}  // namespace PersonMatching
