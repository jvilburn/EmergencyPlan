#include "Team.h"

Team Team::create(const QString& name)
{
    Team team;
    team.m_id = TeamId::generate();
    team.m_name = name;
    return team;
}

void Team::setMemberIds(const QSet<PersonId>& memberIds)
{
    m_memberIds = memberIds;
    if (m_leaderId.has_value() && !m_memberIds.contains(*m_leaderId))
    {
        m_leaderId = std::nullopt;
    }
}

QJsonObject Team::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;

    if (m_leaderId.has_value())
    {
        json["leaderId"] = m_leaderId->toString();
    }

    if (!m_memberIds.isEmpty())
    {
        json["memberIds"] = sortedIdArray(m_memberIds);
    }

    return json;
}

Team Team::fromJson(JsonReader json)
{
    Team team;
    team.m_id = json.id<TeamId>("id");
    team.m_name = json.string("name");
    team.m_leaderId = json.optionalId<PersonId>("leaderId");
    team.m_memberIds = json.optionalIds<PersonId>("memberIds");
    json.finish();
    return team;
}

bool Team::operator==(const Team& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_leaderId == other.m_leaderId
        && m_memberIds == other.m_memberIds;
}
