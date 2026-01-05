#include "ResourceType.h"

ResourceType ResourceType::create(const QString& name, const QString& categoryId)
{
    ResourceType type;
    type.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    type.m_name = name;
    type.m_categoryId = categoryId;
    return type;
}

QJsonObject ResourceType::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["categoryId"] = m_categoryId;

    if (!m_personIds.isEmpty())
    {
        QJsonArray personIdsArray;
        for (const QString& id : m_personIds)
        {
            personIdsArray.append(id);
        }
        json["personIds"] = personIdsArray;
    }

    if (!m_familyIds.isEmpty())
    {
        QJsonArray familyIdsArray;
        for (const QString& id : m_familyIds)
        {
            familyIdsArray.append(id);
        }
        json["familyIds"] = familyIdsArray;
    }

    return json;
}

ResourceType ResourceType::fromJson(const QJsonObject& json)
{
    ResourceType type;
    type.m_id = json["id"].toString();
    type.m_name = json["name"].toString();
    type.m_categoryId = json["categoryId"].toString();

    if (json.contains("personIds"))
    {
        QJsonArray personIdsArray = json["personIds"].toArray();
        for (const QJsonValue& value : personIdsArray)
        {
            type.m_personIds.insert(value.toString());
        }
    }

    if (json.contains("familyIds"))
    {
        QJsonArray familyIdsArray = json["familyIds"].toArray();
        for (const QJsonValue& value : familyIdsArray)
        {
            type.m_familyIds.insert(value.toString());
        }
    }

    return type;
}

bool ResourceType::operator==(const ResourceType& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_categoryId == other.m_categoryId
        && m_personIds == other.m_personIds
        && m_familyIds == other.m_familyIds;
}
