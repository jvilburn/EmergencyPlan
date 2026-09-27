#include "MinisteringGroup.h"
#include "StrictJson.h"

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
        QJsonArray ministerIdsArray;
        for (const PersonId& id : m_ministerIds)
        {
            ministerIdsArray.append(id.toString());
        }
        json["ministerIds"] = ministerIdsArray;
    }

    if (!m_familyIds.isEmpty())
    {
        QJsonArray familyIdsArray;
        for (const FamilyId& id : m_familyIds)
        {
            familyIdsArray.append(id.toString());
        }
        json["familyIds"] = familyIdsArray;
    }

    if (!m_ministeredPersonIds.isEmpty())
    {
        QJsonArray ministeredPersonIdsArray;
        for (const PersonId& id : m_ministeredPersonIds)
        {
            ministeredPersonIdsArray.append(id.toString());
        }
        json["ministeredPersonIds"] = ministeredPersonIdsArray;
    }

    if (m_interviewedDate.has_value())
    {
        json["interviewedDate"] = StrictJson::writeDate(m_interviewedDate.value());
    }

    if (m_presidencyMemberId.has_value())
    {
        json["presidencyMemberId"] = m_presidencyMemberId->toString();
    }

    return json;
}

std::optional<MinisteringGroup> MinisteringGroup::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("isRSFormat"),
                                  QLatin1String("ministerIds"),
                                  QLatin1String("familyIds"),
                                  QLatin1String("ministeredPersonIds"),
                                  QLatin1String("interviewedDate"),
                                  QLatin1String("presidencyMemberId")}))
    {
        return std::nullopt;
    }

    MinisteringGroup group;
    if (!StrictJson::readId(json, QLatin1String("id"), group.m_id)
        || !StrictJson::readBool(json, QLatin1String("isRSFormat"), group.m_isRSFormat))
    {
        return std::nullopt;
    }

    if (json.contains("ministerIds")
        && (!StrictJson::readIdSet(json, QLatin1String("ministerIds"), group.m_ministerIds)
            || group.m_ministerIds.isEmpty()))
    {
        return std::nullopt;
    }

    if (json.contains("familyIds")
        && (!StrictJson::readIdSet(json, QLatin1String("familyIds"), group.m_familyIds)
            || group.m_familyIds.isEmpty()))
    {
        return std::nullopt;
    }

    if (json.contains("ministeredPersonIds")
        && (!StrictJson::readIdSet(json, QLatin1String("ministeredPersonIds"), group.m_ministeredPersonIds)
            || group.m_ministeredPersonIds.isEmpty()))
    {
        return std::nullopt;
    }

    if (json.contains("interviewedDate"))
    {
        QDate date;
        if (!StrictJson::readDate(json, QLatin1String("interviewedDate"), date))
        {
            return std::nullopt;
        }
        group.m_interviewedDate = date;
    }

    if (json.contains("presidencyMemberId"))
    {
        PersonId member;
        if (!StrictJson::readId(json, QLatin1String("presidencyMemberId"), member))
        {
            return std::nullopt;
        }
        group.m_presidencyMemberId = member;
    }

    return group;
}

bool MinisteringGroup::operator==(const MinisteringGroup& other) const
{
    return m_id == other.m_id
        && m_isRSFormat == other.m_isRSFormat
        && m_ministerIds == other.m_ministerIds
        && m_familyIds == other.m_familyIds
        && m_ministeredPersonIds == other.m_ministeredPersonIds
        && m_interviewedDate == other.m_interviewedDate
        && m_presidencyMemberId == other.m_presidencyMemberId;
}
