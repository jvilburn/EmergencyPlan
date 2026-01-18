#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>

class EquipmentCategory
{
public:
    EquipmentCategory() = default;

    // Factory method for creating new categories
    static EquipmentCategory create(const QString& name, int sortOrder);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    int sortOrder() const { return m_sortOrder; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }

    // JSON serialization
    QJsonObject toJson() const;
    static EquipmentCategory fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const EquipmentCategory& other) const;
    bool operator!=(const EquipmentCategory& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    int m_sortOrder = 0;
};
