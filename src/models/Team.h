#pragma once

#include <QString>
#include <QSet>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>

#include "Id.h"

class Team
{
public:
    Team() = default;

    // Factory method for creating new teams
    static Team create(const QString& name,
                       const QColor& color,
                       std::optional<PersonId> leaderId);

    // Getters
    const TeamId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QColor& color() const { return m_color; }
    std::optional<PersonId> leaderId() const { return m_leaderId; }
    const QSet<PersonId>& memberIds() const { return m_memberIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setColor(const QColor& color) { m_color = color; }
    void setLeaderId(std::optional<PersonId> leaderId) { m_leaderId = leaderId; }
    void setMemberIds(const QSet<PersonId>& memberIds) { m_memberIds = memberIds; }
    void addMember(const PersonId& memberId) { m_memberIds.insert(memberId); }
    void removeMember(const PersonId& memberId);

    // Computed properties
    bool hasLeader() const { return m_leaderId.has_value(); }
    bool hasMember(const PersonId& memberId) const { return m_memberIds.contains(memberId); }
    bool isEmpty() const { return m_memberIds.isEmpty(); }
    int memberCount() const { return m_memberIds.size(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Team fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Team& other) const;
    bool operator!=(const Team& other) const { return !(*this == other); }

private:
    TeamId m_id;
    QString m_name;
    QColor m_color;
    std::optional<PersonId> m_leaderId;
    QSet<PersonId> m_memberIds;
};
