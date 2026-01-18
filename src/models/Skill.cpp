#include "Skill.h"

Skill Skill::create(const QString& name, const QString& categoryId)
{
    Skill skill;
    skill.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    skill.m_name = name;
    skill.m_categoryId = categoryId;
    return skill;
}

QJsonObject Skill::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["categoryId"] = m_categoryId;

    QJsonArray personArray;
    for (const QString& personId : m_personIds)
    {
        personArray.append(personId);
    }
    json["personIds"] = personArray;

    return json;
}

Skill Skill::fromJson(const QJsonObject& json)
{
    Skill skill;
    skill.m_id = json["id"].toString();
    skill.m_name = json["name"].toString();
    skill.m_categoryId = json["categoryId"].toString();

    const QJsonArray personArray = json["personIds"].toArray();
    for (const QJsonValue& value : personArray)
    {
        skill.m_personIds.insert(value.toString());
    }

    return skill;
}

bool Skill::operator==(const Skill& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_categoryId == other.m_categoryId
        && m_personIds == other.m_personIds;
}
