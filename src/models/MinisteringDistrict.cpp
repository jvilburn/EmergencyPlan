#include "MinisteringDistrict.h"

MinisteringDistrict MinisteringDistrict::create(
    const QString& name,
    const std::optional<QString>& presidencyMemberId,
    const QSet<QString>& groupIds)
{
    MinisteringDistrict district;
    district.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    district.m_name = name;
    district.m_presidencyMemberId = presidencyMemberId;
    district.m_groupIds = groupIds;
    return district;
}

QJsonObject MinisteringDistrict::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;

    if (m_presidencyMemberId.has_value())
    {
        json["presidencyMemberId"] = m_presidencyMemberId.value();
    }

    if (!m_groupIds.isEmpty())
    {
        QJsonArray groupIdsArray;
        for (const QString& id : m_groupIds)
        {
            groupIdsArray.append(id);
        }
        json["groupIds"] = groupIdsArray;
    }

    return json;
}

MinisteringDistrict MinisteringDistrict::fromJson(const QJsonObject& json)
{
    MinisteringDistrict district;
    district.m_id = json["id"].toString();
    district.m_name = json["name"].toString();

    if (json.contains("presidencyMemberId") && !json["presidencyMemberId"].isNull())
    {
        district.m_presidencyMemberId = json["presidencyMemberId"].toString();
    }

    if (json.contains("groupIds"))
    {
        QJsonArray groupIdsArray = json["groupIds"].toArray();
        for (const QJsonValue& value : groupIdsArray)
        {
            district.m_groupIds.insert(value.toString());
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
