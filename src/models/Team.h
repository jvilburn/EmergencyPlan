#pragma once

#include <QString>
#include <QSet>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <optional>

class Team
{
public:
    Team() = default;

    // Factory method for creating new teams
    static Team create(const QString& name,
                       const QColor& color = QColor(),
                       const QString& leaderId = QString());

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QColor& color() const { return m_color; }
    const QString& leaderId() const { return m_leaderId; }
    const QSet<QString>& memberIds() const { return m_memberIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setColor(const QColor& color) { m_color = color; }
    void setLeaderId(const QString& leaderId) { m_leaderId = leaderId; }
    void setMemberIds(const QSet<QString>& memberIds) { m_memberIds = memberIds; }
    void addMember(const QString& memberId) { m_memberIds.insert(memberId); }
    void removeMember(const QString& memberId) { m_memberIds.remove(memberId); }

    // Computed properties
    bool hasLeader() const { return !m_leaderId.isEmpty(); }
    bool hasMember(const QString& memberId) const { return m_memberIds.contains(memberId); }
    bool isEmpty() const { return m_memberIds.isEmpty(); }
    int memberCount() const { return m_memberIds.size(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Team fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Team& other) const;
    bool operator!=(const Team& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QColor m_color;
    QString m_leaderId;
    QSet<QString> m_memberIds;
};
