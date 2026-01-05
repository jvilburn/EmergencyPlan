#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <optional>

class MinisteringDistrict
{
public:
    MinisteringDistrict() = default;

    // Factory method for creating new districts
    static MinisteringDistrict create(
        const QString& name,
        const std::optional<QString>& presidencyMemberId = std::nullopt,
        const QSet<QString>& groupIds = {});

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    std::optional<QString> presidencyMemberId() const { return m_presidencyMemberId; }
    const QSet<QString>& groupIds() const { return m_groupIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setPresidencyMemberId(std::optional<QString> presidencyMemberId) { m_presidencyMemberId = presidencyMemberId; }
    void setGroupIds(const QSet<QString>& groupIds) { m_groupIds = groupIds; }
    void addGroup(const QString& groupId) { m_groupIds.insert(groupId); }
    void removeGroup(const QString& groupId) { m_groupIds.remove(groupId); }

    // Computed properties
    bool hasPresidencyMember() const { return m_presidencyMemberId.has_value() && !m_presidencyMemberId->isEmpty(); }
    bool hasGroups() const { return !m_groupIds.isEmpty(); }
    int groupCount() const { return m_groupIds.size(); }

    // JSON serialization
    QJsonObject toJson() const;
    static MinisteringDistrict fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const MinisteringDistrict& other) const;
    bool operator!=(const MinisteringDistrict& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    std::optional<QString> m_presidencyMemberId;
    QSet<QString> m_groupIds;
};
