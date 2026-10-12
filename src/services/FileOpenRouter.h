#pragma once

#include <QString>

class DocumentLibrary;
struct DeviceIdentity;

/// Where a .wardplanning file or an invite link goes - a double-click, a link
/// clicked, an argument given at startup, or one a later launch handed over. Among
/// .wardplanning files, the header decides where one goes, not its name or folder.
namespace FileOpenRouter
{

enum class Action
{
    OpenWard,               ///< a ward in the library: open it by id
    OpenCopy,               ///< an exported copy: open it read-only
    AddWard,                ///< a ward written for this device but not in its library
    RequestAccess,          ///< a ward not written for this device
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

/// The invite link a shared ward is sent as. It grants nothing: whoever opens it
/// asks for access.
QString inviteLink(const QString& documentId);

/// The ward an invite link names; empty for anything that is not one.
QString invitedWard(const QString& argument);

/// Where an invite link goes: the ward, when this device has it; otherwise a
/// request for access.
Route routeInvite(const QString& documentId, const DocumentLibrary& library);

} // namespace FileOpenRouter
