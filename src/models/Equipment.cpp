#include "Equipment.h"

Equipment Equipment::create(const QString& name, const QString& categoryId)
{
    Equipment equipment;
    equipment.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    equipment.m_name = name;
    equipment.m_categoryId = categoryId;
    return equipment;
}

QJsonObject Equipment::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["categoryId"] = m_categoryId;

    QJsonArray familyArray;
    for (const QString& familyId : m_familyIds)
    {
        familyArray.append(familyId);
    }
    json["familyIds"] = familyArray;

    return json;
}

Equipment Equipment::fromJson(const QJsonObject& json)
{
    Equipment equipment;
    equipment.m_id = json["id"].toString();
    equipment.m_name = json["name"].toString();
    equipment.m_categoryId = json["categoryId"].toString();

    const QJsonArray familyArray = json["familyIds"].toArray();
    for (const QJsonValue& value : familyArray)
    {
        equipment.m_familyIds.insert(value.toString());
    }

    return equipment;
}

bool Equipment::operator==(const Equipment& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_categoryId == other.m_categoryId
        && m_familyIds == other.m_familyIds;
}
