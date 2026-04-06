#include "MinisteringDistrict.h"

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

MinisteringDistrict MinisteringDistrict::fromJson(const QJsonObject& json)
{
    MinisteringDistrict district;
    district.m_id = MinisteringDistrictId::fromString(json["id"].toString());
    district.m_name = json["name"].toString();

    if (json.contains("presidencyMemberId") && !json["presidencyMemberId"].isNull())
    {
        QString pmStr = json["presidencyMemberId"].toString();
        if (!pmStr.isEmpty())
        {
            district.m_presidencyMemberId = PersonId::fromString(pmStr);
        }
    }

    if (json.contains("groupIds"))
    {
        QJsonArray groupIdsArray = json["groupIds"].toArray();
        for (const QJsonValue& value : groupIdsArray)
        {
            district.m_groupIds.insert(MinisteringGroupId::fromString(value.toString()));
        }
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
