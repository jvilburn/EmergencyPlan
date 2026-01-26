#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>
#include "MarkerDecorationType.h"
#include <optional>

class SkillCategory
{
public:
    SkillCategory() = default;

    // Factory method for creating new categories
    static SkillCategory create(const QString& name, int sortOrder);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    int sortOrder() const { return m_sortOrder; }
    std::optional<MarkerDecorationType> decorationType() const { return m_decorationType; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }
    void setDecorationType(std::optional<MarkerDecorationType> type) { m_decorationType = type; }

    // JSON serialization
    QJsonObject toJson() const;
    static SkillCategory fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const SkillCategory& other) const;
    bool operator!=(const SkillCategory& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    int m_sortOrder = 0;
    std::optional<MarkerDecorationType> m_decorationType;
};
