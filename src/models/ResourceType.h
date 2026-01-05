#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class ResourceType
{
public:
    ResourceType() = default;

    // Factory method for creating new resource types
    static ResourceType create(const QString& name, const QString& categoryId);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& categoryId() const { return m_categoryId; }
    const QSet<QString>& personIds() const { return m_personIds; }
    const QSet<QString>& familyIds() const { return m_familyIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setCategoryId(const QString& categoryId) { m_categoryId = categoryId; }
    void setPersonIds(const QSet<QString>& personIds) { m_personIds = personIds; }
    void setFamilyIds(const QSet<QString>& familyIds) { m_familyIds = familyIds; }
    void addPerson(const QString& personId) { m_personIds.insert(personId); }
    void removePerson(const QString& personId) { m_personIds.remove(personId); }
    void addFamily(const QString& familyId) { m_familyIds.insert(familyId); }
    void removeFamily(const QString& familyId) { m_familyIds.remove(familyId); }

    // Computed properties
    bool hasPerson(const QString& personId) const { return m_personIds.contains(personId); }
    bool hasFamily(const QString& familyId) const { return m_familyIds.contains(familyId); }
    bool isEmpty() const { return m_personIds.isEmpty() && m_familyIds.isEmpty(); }

    // JSON serialization
    QJsonObject toJson() const;
    static ResourceType fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const ResourceType& other) const;
    bool operator!=(const ResourceType& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_categoryId;
    QSet<QString> m_personIds;
    QSet<QString> m_familyIds;
};
