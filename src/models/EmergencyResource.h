#pragma once

#include "ResponseArea.h"

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QUuid>

class EmergencyResource
{
public:
    EmergencyResource() = default;

    // Factory method for creating new resources
    static EmergencyResource create(const QString& name, ResponseArea area);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    ResponseArea responseArea() const { return m_responseArea; }
    const QSet<QString>& personIds() const { return m_personIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setResponseArea(ResponseArea area) { m_responseArea = area; }
    void setPersonIds(const QSet<QString>& ids) { m_personIds = ids; }

    // Person assignment helpers
    void addPerson(const QString& personId);
    void removePerson(const QString& personId);
    bool hasPerson(const QString& personId) const;

    // JSON serialization
    QJsonObject toJson() const;
    static EmergencyResource fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const EmergencyResource& other) const;
    bool operator!=(const EmergencyResource& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    ResponseArea m_responseArea = ResponseArea::None;
    QSet<QString> m_personIds;
};
