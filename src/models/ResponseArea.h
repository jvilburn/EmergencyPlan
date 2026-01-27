#pragma once

#include <QString>

enum class ResponseArea
{
    None,           // No decoration (marker rendering only)
    Medical,        // Blue marker with medical icon
    Recovery,       // Green marker with wrench icon
    Communications, // Antenna overlay on any marker
    SpecialNeeds    // Red marker - derived from Person.specialNeedNote (marker rendering only)
};

// JSON serialization helpers
inline QString responseAreaToString(ResponseArea area)
{
    switch (area)
    {
        case ResponseArea::Medical:
            return "med";
        case ResponseArea::Recovery:
            return "rec";
        case ResponseArea::Communications:
            return "com";
        case ResponseArea::None:
        case ResponseArea::SpecialNeeds:
            return QString();
    }
    return QString();
}

inline ResponseArea responseAreaFromString(const QString& str)
{
    if (str == "med")
    {
        return ResponseArea::Medical;
    }
    if (str == "rec")
    {
        return ResponseArea::Recovery;
    }
    if (str == "com")
    {
        return ResponseArea::Communications;
    }
    return ResponseArea::None;
}
