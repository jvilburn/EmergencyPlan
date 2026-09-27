#include "MinisteringDistrict.h"
#include "StrictJson.h"

MinisteringDistrict MinisteringDistrict::create(
    const QString& name,
    std::optional<PersonId> presidencyMemberId,
    const QSet<MinisteringGroupId>& groupIds)
{
    MinisteringDistrict district;
    district.m_id = MinisteringDistrictId::generate();
    district.m_name = name;
    district.m_presidencyMemberId = presidencyMemberId;
    district.m_groupIds = groupIds;
    return district;
}

QJsonObject MinisteringDistrict::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;

    if (m_presidencyMemberId.has_value())
    {
        json["presidencyMemberId"] = m_presidencyMemberId->toString();
    }

    if (!m_groupIds.isEmpty())
    {
        QJsonArray groupIdsArray;
        for (const MinisteringGroupId& id : m_groupIds)
        {
            groupIdsArray.append(id.toString());
        }
        json["groupIds"] = groupIdsArray;
    }

    return json;
}

std::optional<MinisteringDistrict> MinisteringDistrict::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("name"),
                                  QLatin1String("presidencyMemberId"),
                                  QLatin1String("groupIds")}))
    {
        return std::nullopt;
    }

    MinisteringDistrict district;
    if (!StrictJson::readId(json, QLatin1String("id"), district.m_id)
        || !StrictJson::readString(json, QLatin1String("name"), district.m_name))
    {
        return std::nullopt;
    }

    if (json.contains("presidencyMemberId"))
    {
        PersonId member;
        if (!StrictJson::readId(json, QLatin1String("presidencyMemberId"), member))
        {
            return std::nullopt;
        }
        district.m_presidencyMemberId = member;
    }

    if (json.contains("groupIds")
        && (!StrictJson::readIdSet(json, QLatin1String("groupIds"), district.m_groupIds)
            || district.m_groupIds.isEmpty()))
    {
        return std::nullopt;
    }

    return district;
}

bool MinisteringDistrict::operator==(const MinisteringDistrict& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_presidencyMemberId == other.m_presidencyMemberId
        && m_groupIds == other.m_groupIds;
}
