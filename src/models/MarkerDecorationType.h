#pragma once

#include <QString>
#include <QJsonValue>
#include <optional>

enum class MarkerDecorationType
{
    None,
    Medical,        // Blue marker with medical icon
    Recovery,       // Green marker with wrench icon
    Communications, // Antenna overlay on any marker
    SpecialNeeds    // Red marker - derived from Person.specialNeedNote, not stored on categories
};

// JSON serialization helpers
inline QString markerDecorationTypeToString(MarkerDecorationType type)
{
    switch (type)
    {
        case MarkerDecorationType::Medical:
            return "med";
        case MarkerDecorationType::Recovery:
            return "rec";
        case MarkerDecorationType::Communications:
            return "com";
        default:
            return QString();
    }
}

inline std::optional<MarkerDecorationType> markerDecorationTypeFromString(const QString& str)
{
    if (str == "med")
    {
        return MarkerDecorationType::Medical;
    }
    if (str == "rec")
    {
        return MarkerDecorationType::Recovery;
    }
    if (str == "com")
    {
        return MarkerDecorationType::Communications;
    }
    return std::nullopt;
}
