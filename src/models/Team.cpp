#include "Team.h"

Team Team::create(const QString& name,
                  const QColor& color,
                  const QString& leaderId)
{
    Team team;
    team.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    team.m_name = name;
    team.m_color = color;
    team.m_leaderId = leaderId;
    return team;
}

QJsonObject Team::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;

    if (m_color.isValid())
    {
        json["color"] = m_color.name();
    }
    if (!m_leaderId.isEmpty())
    {
        json["leaderId"] = m_leaderId;
    }

    if (!m_memberIds.isEmpty())
    {
        QJsonArray memberIdsArray;
        for (const QString& id : m_memberIds)
        {
            memberIdsArray.append(id);
        }
        json["memberIds"] = memberIdsArray;
    }

    return json;
}

Team Team::fromJson(const QJsonObject& json)
{
    Team team;
    team.m_id = json["id"].toString();
    team.m_name = json["name"].toString();

    if (json.contains("color"))
    {
        team.m_color = QColor(json["color"].toString());
    }

    team.m_leaderId = json["leaderId"].toString();

    if (json.contains("memberIds"))
    {
        QJsonArray memberIdsArray = json["memberIds"].toArray();
        for (const QJsonValue& value : memberIdsArray)
        {
            team.m_memberIds.insert(value.toString());
        }
    }

    return team;
}

bool Team::operator==(const Team& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_color == other.m_color
        && m_leaderId == other.m_leaderId
        && m_memberIds == other.m_memberIds;
}
