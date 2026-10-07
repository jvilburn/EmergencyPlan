#pragma once

#include <QString>

class DocumentLibrary;
struct DeviceIdentity;

/// Where a .wardplanning file opened by path goes - a double-click, a file given at
/// startup, or one a later launch handed over. Among .wardplanning files, the
/// header decides where one goes, not its name or folder.
namespace FileOpenRouter
{

enum class Action
{
    OpenWard,               ///< a ward in the library: open it by id
    OpenCopy,               ///< an exported copy: open it read-only
    AddWard,                ///< a ward written for this device but not in its library
    LinkDevice,             ///< a ward of this person, written for another of their devices
    RequestAccess,          ///< a ward no device of this person has
    NeedsNewerVersion,      ///< written by a later version of Ward Planning
    NotAWardPlanningFile,   ///< no readable Ward Planning header
    Unreadable              ///< missing, or could not be read
};

struct Route
{
    Action action;
    QString documentId;     ///< the ward, for every action that names one
};

Route route(const QString& path, const DocumentLibrary& library, const DeviceIdentity& identity);

} // namespace FileOpenRouter
