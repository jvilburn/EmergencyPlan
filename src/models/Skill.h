#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class Skill
{
public:
    Skill() = default;

    // Factory method for creating new skills
    static Skill create(const QString& name, const QString& categoryId);

    // Getters
    const QString& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& categoryId() const { return m_categoryId; }
    const QSet<QString>& personIds() const { return m_personIds; }

    // Setters
    void setName(const QString& name) { m_name = name; }
    void setCategoryId(const QString& categoryId) { m_categoryId = categoryId; }
    void setPersonIds(const QSet<QString>& personIds) { m_personIds = personIds; }
    void addPerson(const QString& personId) { m_personIds.insert(personId); }
    void removePerson(const QString& personId) { m_personIds.remove(personId); }

    // Computed properties
    bool hasPerson(const QString& personId) const { return m_personIds.contains(personId); }
    bool isEmpty() const { return m_personIds.isEmpty(); }

    // JSON serialization
    QJsonObject toJson() const;
    static Skill fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Skill& other) const;
    bool operator!=(const Skill& other) const { return !(*this == other); }

private:
    QString m_id;
    QString m_name;
    QString m_categoryId;
    QSet<QString> m_personIds;
};
