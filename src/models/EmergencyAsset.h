#pragma once

#include "Id.h"
#include "ResponseArea.h"

#include <QString>
#include <QSet>
#include <QJsonObject>

class EmergencyAsset
{
public:
    EmergencyAsset() = default;

    // Factory method for creating new assets
    static EmergencyAsset create(const QString& name, ResponseArea area);

    // Getters
    const EmergencyAssetId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    ResponseArea responseArea() const { return m_responseArea; }
    const QSet<PersonId>& personIds() const { return m_personIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setResponseArea(ResponseArea area) { m_responseArea = area; }
    void setPersonIds(const QSet<PersonId>& ids) { m_personIds = ids; }

    // Person assignment helpers
    void addPerson(const PersonId& personId);
    void removePerson(const PersonId& personId);
    bool hasPerson(const PersonId& personId) const;

    // JSON serialization
    QJsonObject toJson() const;
    static EmergencyAsset fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const EmergencyAsset& other) const;
    bool operator!=(const EmergencyAsset& other) const { return !(*this == other); }

private:
    EmergencyAssetId m_id;
    QString m_name;
    ResponseArea m_responseArea = ResponseArea::None;
    QSet<PersonId> m_personIds;
};
