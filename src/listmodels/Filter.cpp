#include "Filter.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"
#include "Tag.h"
#include "Team.h"

Filter::Filter(QObject* parent)
    : QObject(parent)
{
}

void Filter::setSearchText(const QString& text)
{
    if (m_searchText != text)
    {
        m_searchText = text;
        emit changed();
    }
}

void Filter::setTagIds(const QSet<QString>& ids)
{
    if (m_tagIds != ids)
    {
        m_tagIds = ids;
        emit changed();
    }
}

void Filter::setTeamIds(const QSet<QString>& ids)
{
    if (m_teamIds != ids)
    {
        m_teamIds = ids;
        emit changed();
    }
}

void Filter::setResourceTypeIds(const QSet<QString>& ids)
{
    if (m_resourceTypeIds != ids)
    {
        m_resourceTypeIds = ids;
        emit changed();
    }
}

void Filter::setCallings(const QSet<QString>& callings)
{
    if (m_callings != callings)
    {
        m_callings = callings;
        emit changed();
    }
}

void Filter::setGender(std::optional<Gender> gender)
{
    if (m_gender != gender)
    {
        m_gender = gender;
        emit changed();
    }
}

void Filter::setAgeFilter(AgeFilter filter)
{
    if (m_ageFilter != filter)
    {
        m_ageFilter = filter;
        emit changed();
    }
}

void Filter::setSpecificAge(std::optional<int> age)
{
    if (m_specificAge != age)
    {
        m_specificAge = age;
        emit changed();
    }
}

void Filter::setMappedFilter(MappedFilter filter)
{
    if (m_mappedFilter != filter)
    {
        m_mappedFilter = filter;
        emit changed();
    }
}

void Filter::setOnlyWithContact(bool value)
{
    if (m_onlyWithContact != value)
    {
        m_onlyWithContact = value;
        emit changed();
    }
}

void Filter::clear()
{
    bool wasEmpty = isEmpty();

    m_searchText.clear();
    m_tagIds.clear();
    m_teamIds.clear();
    m_resourceTypeIds.clear();
    m_callings.clear();
    m_gender = std::nullopt;
    m_ageFilter = AgeFilter::All;
    m_specificAge = std::nullopt;
    m_mappedFilter = MappedFilter::All;
    m_onlyWithContact = false;

    if (!wasEmpty)
    {
        emit changed();
    }
}

bool Filter::isEmpty() const
{
    return m_searchText.isEmpty()
        && m_tagIds.isEmpty()
        && m_teamIds.isEmpty()
        && m_resourceTypeIds.isEmpty()
        && m_callings.isEmpty()
        && !m_gender.has_value()
        && m_ageFilter == AgeFilter::All
        && !m_specificAge.has_value()
        && m_mappedFilter == MappedFilter::All
        && !m_onlyWithContact;
}

bool Filter::passesFamilyCriteria(const Family& family) const
{
    if (m_mappedFilter == MappedFilter::Mapped && !family.isMapped())
    {
        return false;
    }
    if (m_mappedFilter == MappedFilter::Unmapped && family.isMapped())
    {
        return false;
    }
    if (m_onlyWithContact && !family.hasContact())
    {
        return false;
    }
    return true;
}

bool Filter::passesPersonCriteria(const Person& person) const
{
    // Callings
    if (!m_callings.isEmpty())
    {
        bool found = false;
        for (const QString& calling : person.callings())
        {
            if (m_callings.contains(calling))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }

    // Gender
    if (m_gender.has_value())
    {
        if (!person.gender().has_value() || person.gender().value() != m_gender.value())
        {
            return false;
        }
    }

    // Age: specific age takes precedence
    if (m_specificAge.has_value())
    {
        std::optional<int> personAge = person.age();
        if (!personAge.has_value() || personAge.value() != m_specificAge.value())
        {
            return false;
        }
    }
    else if (m_ageFilter == AgeFilter::Adults)
    {
        if (person.isChild())
        {
            return false;
        }
    }
    else if (m_ageFilter == AgeFilter::Children)
    {
        if (!person.isChild())
        {
            return false;
        }
    }

    return true;
}

bool Filter::personContainsWord(const Person& person, const QString& word)
{
    return person.displayName().toLower().contains(word)
        || person.phone().toLower().contains(word)
        || person.altPhone().toLower().contains(word)
        || person.email().toLower().contains(word);
}

bool Filter::familyContainsWord(const Family& family, const QString& word)
{
    return family.displayName().toLower().contains(word)
        || family.address().full().toLower().contains(word)
        || family.displayPhone().toLower().contains(word)
        || family.displayEmail().toLower().contains(word);
}

bool Filter::hasFamilyLevelTag(const Document& document, const QString& familyId) const
{
    for (const QString& tagId : m_tagIds)
    {
        std::optional<Tag> tagOpt = document.findTagById(tagId);
        if (tagOpt && tagOpt->isFamilyLevel() && tagOpt->hasEntity(familyId))
        {
            return true;
        }
    }
    return false;
}

bool Filter::hasPersonLevelTag(const Document& document, const QString& personId) const
{
    for (const QString& tagId : m_tagIds)
    {
        std::optional<Tag> tagOpt = document.findTagById(tagId);
        if (tagOpt && !tagOpt->isFamilyLevel() && tagOpt->hasEntity(personId))
        {
            return true;
        }
    }
    return false;
}

bool Filter::hasFamilyLevelResource(const Document& /*document*/, const QString& /*familyId*/) const
{
    // TODO: Implement filtering by EmergencyResource (family-level)
    Q_UNUSED(m_resourceTypeIds);
    return false;
}

bool Filter::hasPersonLevelResource(const Document& /*document*/, const QString& /*personId*/) const
{
    // TODO: Implement filtering by EmergencyResource (person-level)
    Q_UNUSED(m_resourceTypeIds);
    return false;
}

bool Filter::isOnTeam(const Document& document, const QString& personId) const
{
    for (const QString& teamId : m_teamIds)
    {
        std::optional<Team> teamOpt = document.findTeamById(teamId);
        if (teamOpt && teamOpt->hasMember(personId))
        {
            return true;
        }
    }
    return false;
}

bool Filter::passes(const Document& document, const Family& family) const
{
    // 1. Family-only criteria (fail fast)
    if (!passesFamilyCriteria(family))
    {
        return false;
    }

    // 2. Pre-compute search words and family-level matches
    QStringList searchWords;
    QSet<int> famMatchedWordIndices;
    if (!m_searchText.isEmpty())
    {
        searchWords = m_searchText.toLower().split(' ', Qt::SkipEmptyParts);
        for (int i = 0; i < searchWords.size(); ++i)
        {
            if (familyContainsWord(family, searchWords[i]))
            {
                famMatchedWordIndices.insert(i);
            }
        }
    }

    // Pre-compute family-level tag/resource matches
    bool tagsOk = m_tagIds.isEmpty() || hasFamilyLevelTag(document, family.id());
    bool resourcesOk = m_resourceTypeIds.isEmpty()
        || hasFamilyLevelResource(document, family.id());
    bool teamsOk = m_teamIds.isEmpty();
    bool personCriteriaOk = m_callings.isEmpty()
        && !m_gender.has_value()
        && m_ageFilter == AgeFilter::All
        && !m_specificAge.has_value();

    // 3. Single loop through members
    for (const Person& member : family.members())
    {
        // Search: find words not matched by family
        for (int i = 0; i < searchWords.size(); ++i)
        {
            if (!famMatchedWordIndices.contains(i)
                && personContainsWord(member, searchWords[i]))
            {
                famMatchedWordIndices.insert(i);
            }
        }

        // Tags: person-level
        if (!tagsOk && hasPersonLevelTag(document, member.id()))
        {
            tagsOk = true;
        }

        // Resources: person-level
        if (!resourcesOk && hasPersonLevelResource(document, member.id()))
        {
            resourcesOk = true;
        }

        // Teams
        if (!teamsOk && isOnTeam(document, member.id()))
        {
            teamsOk = true;
        }

        // Person-only criteria (callings, gender, age)
        if (!personCriteriaOk && passesPersonCriteria(member))
        {
            personCriteriaOk = true;
        }

        // Early exit if all satisfied
        bool allWordsFound = (famMatchedWordIndices.size() == searchWords.size());
        if (allWordsFound && tagsOk && resourcesOk && teamsOk && personCriteriaOk)
        {
            return true;
        }
    }

    // Final check
    bool allWordsFound = searchWords.isEmpty()
        || (famMatchedWordIndices.size() == searchWords.size());
    return allWordsFound && tagsOk && resourcesOk && teamsOk && personCriteriaOk;
}

bool Filter::passes(const Document& document, const Person& person) const
{
    // Look up person's family
    QString familyId = document.familyIdForPerson(person.id());
    std::optional<Family> familyOpt = document.findFamilyById(familyId);

    // Family-level criteria: mapped
    if (familyOpt.has_value())
    {
        const Family& family = familyOpt.value();
        if (m_mappedFilter == MappedFilter::Mapped && !family.isMapped())
        {
            return false;
        }
        if (m_mappedFilter == MappedFilter::Unmapped && family.isMapped())
        {
            return false;
        }
    }

    // Person-level: contact (person's contact, not family's)
    if (m_onlyWithContact && !person.hasContact())
    {
        return false;
    }

    // Person-only criteria (callings, gender, age)
    if (!passesPersonCriteria(person))
    {
        return false;
    }

    // Search: person fields OR family fields
    if (!m_searchText.isEmpty())
    {
        QStringList words = m_searchText.toLower().split(' ', Qt::SkipEmptyParts);
        for (const QString& word : words)
        {
            bool found = personContainsWord(person, word);
            if (!found && familyOpt.has_value())
            {
                found = familyContainsWord(familyOpt.value(), word);
            }
            if (!found)
            {
                return false;
            }
        }
    }

    // Tags: person-level OR family-level
    if (!m_tagIds.isEmpty())
    {
        bool found = hasPersonLevelTag(document, person.id());
        if (!found && familyOpt.has_value())
        {
            found = hasFamilyLevelTag(document, familyId);
        }
        if (!found)
        {
            return false;
        }
    }

    // Teams
    if (!m_teamIds.isEmpty() && !isOnTeam(document, person.id()))
    {
        return false;
    }

    // Resources: person-level OR family-level
    if (!m_resourceTypeIds.isEmpty())
    {
        bool found = hasPersonLevelResource(document, person.id());
        if (!found && familyOpt.has_value())
        {
            found = hasFamilyLevelResource(document, familyId);
        }
        if (!found)
        {
            return false;
        }
    }

    return true;
}
