#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include "ResourceLevel.h"

class Tag
{
public:
    Tag() = default;

    // Factory method for creating new tags
    static Tag create(const QString& name, ResourceLevel level,
                      const QString& color = QString());

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& color() const { return m_color; }
    ResourceLevel level() const { return m_level; }
    const QSet<QString>& entityIds() const { return m_entityIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setColor(const QString& color) { m_color = color; }
    void setLevel(ResourceLevel level) { m_level = level; }
    void setEntityIds(const QSet<QString>& entityIds) { m_entityIds = entityIds; }
    void addEntity(const QString& entityId) { m_entityIds.insert(entityId); }
    void removeEntity(const QString& entityId) { m_entityIds.remove(entityId); }

    // Computed properties
    bool hasEntity(const QString& entityId) const { return m_entityIds.contains(entityId); }
    bool isEmpty() const { return m_entityIds.isEmpty(); }

    // Convenience for checking level
    bool isPersonLevel() const { return m_level == ResourceLevel::Person; }
    bool isFamilyLevel() const { return m_level == ResourceLevel::Family; }

    // JSON serialization
    QJsonObject toJson() const;
    static Tag fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Tag& other) const;
    bool operator!=(const Tag& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_color;
    ResourceLevel m_level = ResourceLevel::Person;
    QSet<QString> m_entityIds;
};
