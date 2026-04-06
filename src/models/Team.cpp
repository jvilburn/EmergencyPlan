#include "Team.h"

Team Team::create(const QString& name,
                  const QColor& color,
                  std::optional<PersonId> leaderId)
{
    Team team;
    team.m_id = TeamId::generate();
    team.m_name = name;
    team.m_color = color;
    team.m_leaderId = leaderId;
    return team;
}

void Team::removeMember(const PersonId& memberId)
{
    m_memberIds.remove(memberId);
    if (m_leaderId && *m_leaderId == memberId)
    {
        m_leaderId = std::nullopt;
    }
}

QJsonObject Team::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;

    if (m_color.isValid())
    {
        json["color"] = m_color.name();
    }
    if (m_leaderId.has_value())
    {
        json["leaderId"] = m_leaderId->toString();
    }

    if (!m_memberIds.isEmpty())
    {
        QJsonArray memberIdsArray;
        for (const PersonId& id : m_memberIds)
        {
            memberIdsArray.append(id.toString());
        }
        json["memberIds"] = memberIdsArray;
    }

    return json;
}

Team Team::fromJson(const QJsonObject& json)
{
    Team team;
    team.m_id = TeamId::fromString(json["id"].toString());
    team.m_name = json["name"].toString();

    if (json.contains("color"))
    {
        team.m_color = QColor(json["color"].toString());
    }

    QString leaderStr = json["leaderId"].toString();
    if (!leaderStr.isEmpty())
    {
        team.m_leaderId = PersonId::fromString(leaderStr);
    }

    if (json.contains("memberIds"))
    {
        QJsonArray memberIdsArray = json["memberIds"].toArray();
        for (const QJsonValue& value : memberIdsArray)
        {
            team.m_memberIds.insert(PersonId::fromString(value.toString()));
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
