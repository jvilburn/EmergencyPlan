#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class Equipment
{
public:
    Equipment() = default;

    // Factory method for creating new equipment
    static Equipment create(const QString& name, const QString& categoryId);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& categoryId() const { return m_categoryId; }
    const QSet<QString>& familyIds() const { return m_familyIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setCategoryId(const QString& categoryId) { m_categoryId = categoryId; }
    void setFamilyIds(const QSet<QString>& familyIds) { m_familyIds = familyIds; }
    void addFamily(const QString& familyId) { m_familyIds.insert(familyId); }
    void removeFamily(const QString& familyId) { m_familyIds.remove(familyId); }

    // Computed properties
    bool hasFamily(const QString& familyId) const { return m_familyIds.contains(familyId); }
    bool isEmpty() const { return m_familyIds.isEmpty(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Equipment fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Equipment& other) const;
    bool operator!=(const Equipment& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_categoryId;
    QSet<QString> m_familyIds;
};
