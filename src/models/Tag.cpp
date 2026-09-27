#include "Tag.h"
#include "StrictJson.h"

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

std::optional<Tag> Tag::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("name"),
                                  QLatin1String("level"),
                                  QLatin1String("color"),
                                  QLatin1String("personIds"),
                                  QLatin1String("familyIds")}))
    {
        return std::nullopt;
    }

    Tag tag;
    QString level;
    if (!StrictJson::readId(json, QLatin1String("id"), tag.m_id)
        || !StrictJson::readString(json, QLatin1String("name"), tag.m_name)
        || !StrictJson::readString(json, QLatin1String("level"), level))
    {
        return std::nullopt;
    }

    // Exactly as tagLevelToString writes it.
    if (level == tagLevelToString(TagLevel::Person))
    {
        tag.m_level = TagLevel::Person;
    }
    else if (level == tagLevelToString(TagLevel::Family))
    {
        tag.m_level = TagLevel::Family;
    }
    else
    {
        return std::nullopt;
    }

    if (json.contains("color") && !StrictJson::readNonEmptyString(json, QLatin1String("color"), tag.m_color))
    {
        return std::nullopt;
    }

    if (json.contains("personIds")
        && (!StrictJson::readIdSet(json, QLatin1String("personIds"), tag.m_personIds)
            || tag.m_personIds.isEmpty()))
    {
        return std::nullopt;
    }

    if (json.contains("familyIds")
        && (!StrictJson::readIdSet(json, QLatin1String("familyIds"), tag.m_familyIds)
            || tag.m_familyIds.isEmpty()))
    {
        return std::nullopt;
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
