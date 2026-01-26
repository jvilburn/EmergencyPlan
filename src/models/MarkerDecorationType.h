#pragma once

#include <QString>
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
// Note: None and SpecialNeeds are intentionally not serialized. None means "no decoration",
// and SpecialNeeds is derived from Person.specialNeedNote rather than stored on categories.
// fromString("") returns nullopt (not None) because empty strings indicate absence of the field.
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
        case MarkerDecorationType::None:
        case MarkerDecorationType::SpecialNeeds:
            return QString();
    }
    return QString();  // Unreachable, silences compiler warnings
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
