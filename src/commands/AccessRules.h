#pragma once

#include "AccessList.h"

#include <QByteArray>
#include <QString>

/// The rules that decide which access-list changes a device's user may make. Each answers the
/// wording the app gives when the rule refuses, or an empty string when it allows. A command
/// asks its rule of the version in front of it, the first time and on replay, about the user
/// of the device that made it, named by that device's fingerprint (actor); the interface asks
/// the same rule before it offers the action.
class AccessRules
{
public:
    /// Making an invite: a user invite is for admins only; anyone adds their own devices.
    static QString invite(const AccessList& access, const QByteArray& actor, bool forUser);

    /// Removing a user: admins only, and never yourself.
    static QString removeUser(const AccessList& access, const QByteArray& actor, const UserId& user);

    /// Removing a device: only one of your own, and never your last.
    static QString removeDevice(const AccessList& access, const QByteArray& actor, const QByteArray& device);

    /// Withdrawing an invite: its maker, or an admin.
    static QString withdrawInvite(const AccessList& access, const QByteArray& actor, const QByteArray& invite);

    /// Setting a role: admins only, and the only admin cannot step down.
    static QString setRole(const AccessList& access, const QByteArray& actor, const UserId& user, AccessRole role);

    /// Joining device through invite: only while the list holds the invite, or already holds
    /// the device, which is the join's result.
    static QString join(const AccessList& access, const QByteArray& invite, const QByteArray& device);
};
