#pragma once

#include <QString>

class DocumentLibrary;

/// Where a .wardplanning file opened by path goes - a double-click, a file given at
/// startup, or one a later launch handed over. Its header decides, not its name or
/// folder.
namespace FileOpenRouter
{

enum class Action
{
    OpenWard,               ///< a ward in the library: open it by id
    OpenCopy,               ///< an exported copy: open it read-only
    RequestAccess,          ///< a ward this device does not have
    NeedsNewerVersion,      ///< written by a later version of Ward Planning
    NotAWardPlanningFile,   ///< no readable Ward Planning header
    Unreadable              ///< missing, or could not be read
};

struct Route
{
    Action action;
    QString documentId;     ///< the ward, for OpenWard and RequestAccess
};

Route route(const QString& path, const DocumentLibrary& library);

} // namespace FileOpenRouter
