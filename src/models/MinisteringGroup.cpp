#include "MinisteringGroup.h"

MinisteringGroup MinisteringGroup::createEQ(
    const QSet<PersonId>& ministerIds,
    const QSet<FamilyId>& familyIds)
{
    MinisteringGroup group;
    group.m_id = MinisteringGroupId::generate();
    group.m_isRSFormat = false;
    group.m_ministerIds = ministerIds;
    group.m_familyIds = familyIds;
    return group;
}

MinisteringGroup MinisteringGroup::createRS(
    const QSet<PersonId>& ministerIds,
    const QSet<PersonId>& ministeredPersonIds)
{
    MinisteringGroup group;
    group.m_id = MinisteringGroupId::generate();
    group.m_isRSFormat = true;
    group.m_ministerIds = ministerIds;
    group.m_ministeredPersonIds = ministeredPersonIds;
    return group;
}

QJsonObject MinisteringGroup::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["isRSFormat"] = m_isRSFormat;

    if (!m_ministerIds.isEmpty())
    {
        json["ministerIds"] = sortedIdArray(m_ministerIds);
    }

    if (!m_familyIds.isEmpty())
    {
        json["familyIds"] = sortedIdArray(m_familyIds);
    }

    if (!m_ministeredPersonIds.isEmpty())
    {
        json["ministeredPersonIds"] = sortedIdArray(m_ministeredPersonIds);
    }

    if (m_presidencyMemberId.has_value())
    {
        json["presidencyMemberId"] = m_presidencyMemberId->toString();
    }

    return json;
}

MinisteringGroup MinisteringGroup::fromJson(JsonReader json)
{
    MinisteringGroup group;
    group.m_id = json.id<MinisteringGroupId>("id");
    group.m_isRSFormat = json.boolean("isRSFormat");
    group.m_ministerIds = json.optionalIds<PersonId>("ministerIds");
    group.m_familyIds = json.optionalIds<FamilyId>("familyIds");
    group.m_ministeredPersonIds = json.optionalIds<PersonId>("ministeredPersonIds");
    group.m_presidencyMemberId = json.optionalId<PersonId>("presidencyMemberId");
    json.finish();
    return group;
}

bool MinisteringGroup::operator==(const MinisteringGroup& other) const
{
    return m_id == other.m_id
        && m_isRSFormat == other.m_isRSFormat
        && m_ministerIds == other.m_ministerIds
        && m_familyIds == other.m_familyIds
        && m_ministeredPersonIds == other.m_ministeredPersonIds
        && m_presidencyMemberId == other.m_presidencyMemberId;
}
