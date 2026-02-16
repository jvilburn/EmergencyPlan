#include "Tag.h"

Tag Tag::create(const QString& name, TagLevel level, const QString& color)
{
    Tag tag;
    tag.m_id = TagId::generate();
    tag.m_name = name;
    tag.m_level = level;
    tag.m_color = color;
    return tag;
}

QJsonObject Tag::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;
    json["level"] = tagLevelToString(m_level);

    if (!m_color.isEmpty())
    {
        json["color"] = m_color;
    }

    if (!m_personIds.isEmpty())
    {
        QJsonArray personIdsArray;
        for (const PersonId& id : m_personIds)
        {
            personIdsArray.append(id.toString());
        }
        json["personIds"] = personIdsArray;
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

    return json;
}

Tag Tag::fromJson(const QJsonObject& json)
{
    Tag tag;
    tag.m_id = TagId::fromString(json["id"].toString());
    tag.m_name = json["name"].toString();
    tag.m_color = json["color"].toString();
    tag.m_level = tagLevelFromJson(json["level"]);

    // Read new format (personIds/familyIds)
    if (json.contains("personIds"))
    {
        QJsonArray personIdsArray = json["personIds"].toArray();
        for (const QJsonValue& value : personIdsArray)
        {
            tag.m_personIds.insert(PersonId::fromString(value.toString()));
        }
    }
    if (json.contains("familyIds"))
    {
        QJsonArray familyIdsArray = json["familyIds"].toArray();
        for (const QJsonValue& value : familyIdsArray)
        {
            tag.m_familyIds.insert(FamilyId::fromString(value.toString()));
        }
    }

    // Backward compatibility: read old "entityIds" format
    if (json.contains("entityIds"))
    {
        QJsonArray entityIdsArray = json["entityIds"].toArray();
        for (const QJsonValue& value : entityIdsArray)
        {
            if (tag.m_level == TagLevel::Person)
            {
                tag.m_personIds.insert(PersonId::fromString(value.toString()));
            }
            else
            {
                tag.m_familyIds.insert(FamilyId::fromString(value.toString()));
            }
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
        && m_personIds == other.m_personIds
        && m_familyIds == other.m_familyIds;
}
