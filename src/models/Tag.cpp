#include "Tag.h"

Tag Tag::create(const QString& name, TagLevel level, const QString& color)
{
    Tag tag;
    tag.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tag.m_name = name;
    tag.m_level = level;
    tag.m_color = color;
    return tag;
}

QJsonObject Tag::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["level"] = tagLevelToString(m_level);

    if (!m_color.isEmpty())
    {
        json["color"] = m_color;
    }

    if (!m_entityIds.isEmpty())
    {
        QJsonArray entityIdsArray;
        for (const QString& id : m_entityIds)
        {
            entityIdsArray.append(id);
        }
        json["entityIds"] = entityIdsArray;
    }

    return json;
}

Tag Tag::fromJson(const QJsonObject& json)
{
    Tag tag;
    tag.m_id = json["id"].toString();
    tag.m_name = json["name"].toString();
    tag.m_color = json["color"].toString();
    tag.m_level = tagLevelFromJson(json["level"]);

    if (json.contains("entityIds"))
    {
        QJsonArray entityIdsArray = json["entityIds"].toArray();
        for (const QJsonValue& value : entityIdsArray)
        {
            tag.m_entityIds.insert(value.toString());
        }
    }

    return tag;
}

bool Tag::operator==(const Tag& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_color == other.m_color
        && m_level == other.m_level
        && m_entityIds == other.m_entityIds;
}
