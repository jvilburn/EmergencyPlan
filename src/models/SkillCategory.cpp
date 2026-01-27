#include "SkillCategory.h"

SkillCategory SkillCategory::create(const QString& name, int sortOrder)
{
    SkillCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    return category;
}

SkillCategory SkillCategory::create(const QString& name, int sortOrder, ResponseArea decorationType)
{
    SkillCategory category;
    category.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.m_name = name;
    category.m_sortOrder = sortOrder;
    category.m_decorationType = decorationType;
    return category;
}

QJsonObject SkillCategory::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["sortOrder"] = m_sortOrder;
    QString typeStr = responseAreaToString(m_decorationType);
    if (!typeStr.isEmpty())
    {
        json["decorationType"] = typeStr;
    }
    return json;
}

SkillCategory SkillCategory::fromJson(const QJsonObject& json)
{
    SkillCategory category;
    category.m_id = json["id"].toString();
    category.m_name = json["name"].toString();
    category.m_sortOrder = json["sortOrder"].toInt(0);
    if (json.contains("decorationType"))
    {
        category.m_decorationType = responseAreaFromString(json["decorationType"].toString());
    }
    else
    {
        category.m_decorationType = ResponseArea::None;
    }
    return category;
}

bool SkillCategory::operator==(const SkillCategory& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_sortOrder == other.m_sortOrder
        && m_decorationType == other.m_decorationType;
}
