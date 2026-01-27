#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>
#include "ResponseArea.h"

class EquipmentCategory
{
public:
    EquipmentCategory() = default;

    // Factory methods for creating new categories
    static EquipmentCategory create(const QString& name, int sortOrder);
    static EquipmentCategory create(const QString& name, int sortOrder, ResponseArea decorationType);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    int sortOrder() const { return m_sortOrder; }
    ResponseArea decorationType() const { return m_decorationType; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setSortOrder(int sortOrder) { m_sortOrder = sortOrder; }
    void setDecorationType(ResponseArea type) { m_decorationType = type; }

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
    ResponseArea m_decorationType = ResponseArea::None;
};
