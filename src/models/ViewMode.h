#pragma once

#include <QString>

enum class ViewMode
{
    EmergencyResponse,
    EldersQuorum,
    ReliefSociety,
    Tags
};

inline QString viewModeToString(ViewMode mode)
{
    switch (mode)
    {
        case ViewMode::EmergencyResponse:
            return "emergencyResponse";
        case ViewMode::EldersQuorum:
            return "eldersQuorum";
        case ViewMode::ReliefSociety:
            return "reliefSociety";
        case ViewMode::Tags:
            return "tags";
    }
    return QString();
}

inline ViewMode viewModeFromString(const QString& str)
{
    if (str == "emergencyResponse")
    {
        return ViewMode::EmergencyResponse;
    }
    if (str == "eldersQuorum")
    {
        return ViewMode::EldersQuorum;
    }
    if (str == "reliefSociety")
    {
        return ViewMode::ReliefSociety;
    }
    if (str == "tags")
    {
        return ViewMode::Tags;
    }
    return ViewMode::EmergencyResponse;  // Default
}

inline QString viewModeLabel(ViewMode mode)
{
    switch (mode)
    {
        case ViewMode::EmergencyResponse:
            return "Emergency Response";
        case ViewMode::EldersQuorum:
            return "Elders Quorum";
        case ViewMode::ReliefSociety:
            return "Relief Society";
        case ViewMode::Tags:
            return "Tags";
    }
    return QString();
}

inline QString viewModeShortcut(ViewMode mode)
{
    switch (mode)
    {
        case ViewMode::EmergencyResponse:
            return "Ctrl+1";
        case ViewMode::EldersQuorum:
            return "Ctrl+2";
        case ViewMode::ReliefSociety:
            return "Ctrl+3";
        case ViewMode::Tags:
            return "Ctrl+4";
    }
    return QString();
}
