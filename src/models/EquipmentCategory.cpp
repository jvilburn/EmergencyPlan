#include "EquipmentCategory.h"

EquipmentCategory EquipmentCategory::create(const QString& name, int sortOrder)
{
    EquipmentCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    return category;
}

EquipmentCategory EquipmentCategory::create(const QString& name, int sortOrder, std::optional<MarkerDecorationType> decorationType)
{
    EquipmentCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    category.m_decorationType = decorationType;
    return category;
}

QJsonObject EquipmentCategory::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["sortOrder"] = m_sortOrder;
    if (m_decorationType.has_value())
    {
        QString typeStr = markerDecorationTypeToString(m_decorationType.value());
        if (!typeStr.isEmpty())
        {
            json["decorationType"] = typeStr;
        }
    }
    return json;
}

EquipmentCategory EquipmentCategory::fromJson(const QJsonObject& json)
{
    EquipmentCategory category;
    category.m_id = json["id"].toString();
    category.m_name = json["name"].toString();
    category.m_sortOrder = json["sortOrder"].toInt(0);
    if (json.contains("decorationType"))
    {
        category.m_decorationType = markerDecorationTypeFromString(json["decorationType"].toString());
    }
    return category;
}

bool EquipmentCategory::operator==(const EquipmentCategory& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_sortOrder == other.m_sortOrder
        && m_decorationType == other.m_decorationType;
}
