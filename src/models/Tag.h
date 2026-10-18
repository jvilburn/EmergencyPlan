#pragma once

#include <QString>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <optional>

#include "Id.h"

// Tag level - whether the tag applies to persons or families
enum class TagLevel
{
    Person,
    Family
};

inline QString tagLevelToString(TagLevel level)
{
    switch (level)
    {
        case TagLevel::Person:
            return "person";
        case TagLevel::Family:
            return "family";
    }
    return QString();
}

inline std::optional<TagLevel> tagLevelFromString(const QString& str)
{
    QString lower = str.toLower();
    if (lower == "person")
    {
        return TagLevel::Person;
    }
    if (lower == "family")
    {
        return TagLevel::Family;
    }
    return std::nullopt;
}

inline TagLevel tagLevelFromJson(const QJsonValue& value)
{
    if (value.isNull() || !value.isString())
    {
        return TagLevel::Person;  // Default fallback
    }
    return tagLevelFromString(value.toString()).value_or(TagLevel::Person);
}

class Tag
{
public:
    Tag() = default;

    // Getters
    const TagId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QString& color() const { return m_color; }
    TagLevel level() const { return m_level; }
    const QSet<PersonId>& personIds() const { return m_personIds; }
    const QSet<FamilyId>& familyIds() const { return m_familyIds; }

    // Computed properties
    bool hasPerson(const PersonId& personId) const { return m_personIds.contains(personId); }
    bool hasFamily(const FamilyId& familyId) const { return m_familyIds.contains(familyId); }
    bool isEmpty() const { return m_personIds.isEmpty() && m_familyIds.isEmpty(); }

    // Convenience for checking level
    bool isPersonLevel() const { return m_level == TagLevel::Person; }
    bool isFamilyLevel() const { return m_level == TagLevel::Family; }

    // JSON serialization
    QJsonObject toJson() const;
    static Tag fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Tag& other) const;
    bool operator!=(const Tag& other) const { return !(*this == other); }

private:
    TagId m_id;
    QString m_name;
    QString m_color;
    TagLevel m_level = TagLevel::Person;
    QSet<PersonId> m_personIds;
    QSet<FamilyId> m_familyIds;
};
