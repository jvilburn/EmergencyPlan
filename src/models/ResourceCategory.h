#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>
#include <optional>

#include "ResourceLevel.h"
#include "MarkerDecorationType.h"

class ResourceCategory
{
public:
    ResourceCategory() = default;

    // Factory method for creating new categories
    static ResourceCategory create(
        const QString& name,
        ResourceLevel level,
        int sortOrder,
        std::optional<MarkerDecorationType> decorationType = std::nullopt);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    ResourceLevel level() const { return m_level; }
    int sortOrder() const { return m_sortOrder; }
    std::optional<MarkerDecorationType> decorationType() const { return m_decorationType; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setLevel(ResourceLevel level) { m_level = level; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }
    void setDecorationType(std::optional<MarkerDecorationType> decorationType) { m_decorationType = decorationType; }

    // JSON serialization
    QJsonObject toJson() const;
    static ResourceCategory fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const ResourceCategory& other) const;
    bool operator!=(const ResourceCategory& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    ResourceLevel m_level = ResourceLevel::Person;
    int m_sortOrder = 0;
    std::optional<MarkerDecorationType> m_decorationType;
};
