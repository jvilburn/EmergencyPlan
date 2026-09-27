#include "Team.h"
#include "StrictJson.h"

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

std::optional<Team> Team::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("name"),
                                  QLatin1String("color"),
                                  QLatin1String("leaderId"),
                                  QLatin1String("memberIds")}))
    {
        return std::nullopt;
    }

    Team team;
    if (!StrictJson::readId(json, QLatin1String("id"), team.m_id)
        || !StrictJson::readString(json, QLatin1String("name"), team.m_name))
    {
        return std::nullopt;
    }

    // Exactly the #rrggbb name QColor writes.
    if (json.contains("color"))
    {
        QString color;
        if (!StrictJson::readNonEmptyString(json, QLatin1String("color"), color))
        {
            return std::nullopt;
        }
        team.m_color = QColor(color);
        if (!team.m_color.isValid() || team.m_color.name() != color)
        {
            return std::nullopt;
        }
    }

    if (json.contains("leaderId"))
    {
        PersonId leader;
        if (!StrictJson::readId(json, QLatin1String("leaderId"), leader))
        {
            return std::nullopt;
        }
        team.m_leaderId = leader;
    }

    if (json.contains("memberIds")
        && (!StrictJson::readIdSet(json, QLatin1String("memberIds"), team.m_memberIds)
            || team.m_memberIds.isEmpty()))
    {
        return std::nullopt;
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
