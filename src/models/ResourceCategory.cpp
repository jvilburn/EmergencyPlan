#include "ResourceCategory.h"

ResourceCategory ResourceCategory::create(
    const QString& name,
    ResourceLevel level,
    int sortOrder,
    std::optional<MarkerDecorationType> decorationType)
{
    ResourceCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_level = level;
    category.m_sortOrder = sortOrder;
    category.m_decorationType = decorationType;
    return category;
}

QJsonObject ResourceCategory::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["level"] = resourceLevelToJson(m_level);
    json["sortOrder"] = m_sortOrder;

    if (m_decorationType.has_value())
    {
        json["decorationType"] = markerDecorationTypeToJson(m_decorationType.value());
    }

    return json;
}

ResourceCategory ResourceCategory::fromJson(const QJsonObject& json)
{
    ResourceCategory category;
    category.m_id = json["id"].toString();
    category.m_name = json["name"].toString();
    category.m_level = resourceLevelFromJson(json["level"]);
    category.m_sortOrder = json["sortOrder"].toInt(0);

    if (json.contains("decorationType") && !json["decorationType"].isNull())
    {
        category.m_decorationType = markerDecorationTypeFromJson(json["decorationType"]);
    }

    return category;
}

bool ResourceCategory::operator==(const ResourceCategory& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_level == other.m_level
        && m_sortOrder == other.m_sortOrder
        && m_decorationType == other.m_decorationType;
}
