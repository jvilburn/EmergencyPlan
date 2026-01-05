#pragma once

#include <QString>
#include <QJsonValue>
#include <optional>

enum class ResourceLevel
{
    Person,
    Family
};

inline QString resourceLevelToString(ResourceLevel level)
{
    switch (level)
    {
        case ResourceLevel::Person:
            return "person";
        case ResourceLevel::Family:
            return "family";
    }
    return QString();
}

inline std::optional<ResourceLevel> resourceLevelFromString(const QString& str)
{
    QString lower = str.toLower();
    if (lower == "person")
    {
        return ResourceLevel::Person;
    }
    if (lower == "family")
    {
        return ResourceLevel::Family;
    }
    return std::nullopt;
}

inline QString resourceLevelDisplayName(ResourceLevel level)
{
    switch (level)
    {
        case ResourceLevel::Person:
            return "Person";
        case ResourceLevel::Family:
            return "Family";
    }
    return QString();
}

inline QJsonValue resourceLevelToJson(ResourceLevel level)
{
    return QJsonValue(resourceLevelToString(level));
}

inline ResourceLevel resourceLevelFromJson(const QJsonValue& value)
{
    if (value.isNull() || !value.isString())
    {
        return ResourceLevel::Person;  // Default fallback
    }
    return resourceLevelFromString(value.toString()).value_or(ResourceLevel::Person);
}
