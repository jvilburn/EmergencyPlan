#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>

#include "Id.h"

class MinisteringDistrict
{
public:
    MinisteringDistrict() = default;

    // Factory method for creating new districts
    static MinisteringDistrict create(
        const QString& name,
        std::optional<PersonId> presidencyMemberId = std::nullopt,
        const QSet<MinisteringGroupId>& groupIds = {});

    // Getters
    const MinisteringDistrictId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    std::optional<PersonId> presidencyMemberId() const { return m_presidencyMemberId; }
    const QSet<MinisteringGroupId>& groupIds() const { return m_groupIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setPresidencyMemberId(std::optional<PersonId> presidencyMemberId) { m_presidencyMemberId = presidencyMemberId; }
    void setGroupIds(const QSet<MinisteringGroupId>& groupIds) { m_groupIds = groupIds; }
    void addGroup(const MinisteringGroupId& groupId) { m_groupIds.insert(groupId); }
    void removeGroup(const MinisteringGroupId& groupId) { m_groupIds.remove(groupId); }

    // Computed properties
    bool hasPresidencyMember() const { return m_presidencyMemberId.has_value(); }
    bool hasGroups() const { return !m_groupIds.isEmpty(); }
    int groupCount() const { return m_groupIds.size(); }

    // JSON serialization
    QJsonObject toJson() const;
    static MinisteringDistrict fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const MinisteringDistrict& other) const;
    bool operator!=(const MinisteringDistrict& other) const { return !(*this == other); }

private:
    MinisteringDistrictId m_id;
    QString m_name;
    std::optional<PersonId> m_presidencyMemberId;
    QSet<MinisteringGroupId> m_groupIds;
};
