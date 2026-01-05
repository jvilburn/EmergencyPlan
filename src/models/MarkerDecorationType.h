#pragma once

#include <QString>
#include <QColor>
#include <QJsonValue>
#include <optional>

enum class MarkerDecorationType
{
    Medical,       // Blue marker - medical skills
    Recovery,      // Green marker - recovery resources
    Communication, // Antenna overlay - communication equipment
    SpecialNeeds   // Red marker - special needs
};

inline QString markerDecorationTypeToString(MarkerDecorationType type)
{
    switch (type)
    {
        case MarkerDecorationType::Medical:
            return "MED";
        case MarkerDecorationType::Recovery:
            return "REC";
        case MarkerDecorationType::Communication:
            return "COM";
        case MarkerDecorationType::SpecialNeeds:
            return "SPEC";
    }
    return QString();
}

inline std::optional<MarkerDecorationType> markerDecorationTypeFromString(const QString& str)
{
    QString upper = str.toUpper();
    if (upper == "MED")
    {
        return MarkerDecorationType::Medical;
    }
    if (upper == "REC")
    {
        return MarkerDecorationType::Recovery;
    }
    if (upper == "COM")
    {
        return MarkerDecorationType::Communication;
    }
    if (upper == "SPEC")
    {
        return MarkerDecorationType::SpecialNeeds;
    }
    return std::nullopt;
}

inline QString markerDecorationDisplayName(MarkerDecorationType type)
{
    switch (type)
    {
        case MarkerDecorationType::Medical:
            return "Medical Skills";
        case MarkerDecorationType::Recovery:
            return "Recovery Resources";
        case MarkerDecorationType::Communication:
            return "Communication";
        case MarkerDecorationType::SpecialNeeds:
            return "Special Needs";
    }
    return QString();
}

inline QColor markerDecorationColor(MarkerDecorationType type)
{
    switch (type)
    {
        case MarkerDecorationType::Medical:
            return QColor(0x44, 0x88, 0xCC);  // Blue
        case MarkerDecorationType::Recovery:
            return QColor(0x44, 0xAA, 0x44);  // Green
        case MarkerDecorationType::Communication:
            return QColor(0xCC, 0x88, 0x44);  // Orange
        case MarkerDecorationType::SpecialNeeds:
            return QColor(0xCC, 0x44, 0x44);  // Red
    }
    return QColor();
}

inline QJsonValue markerDecorationTypeToJson(MarkerDecorationType type)
{
    return QJsonValue(markerDecorationTypeToString(type));
}

inline std::optional<MarkerDecorationType> markerDecorationTypeFromJson(const QJsonValue& value)
{
    if (value.isNull() || !value.isString())
    {
        return std::nullopt;
    }
    return markerDecorationTypeFromString(value.toString());
}
