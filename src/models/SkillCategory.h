#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>

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

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }

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
};
