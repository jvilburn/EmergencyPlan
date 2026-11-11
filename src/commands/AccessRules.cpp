#include "AccessRules.h"

#include <QObject>

namespace
{

/// What a member is told when they ask for what only admins do.
QString askAnAdmin(const AccessList& access)
{
    return QObject::tr("To add or remove users, ask an admin: %1").arg(access.adminNames().join(", "));
}

QString useLeave()
{
    return QObject::tr("To stop using this ward, use Leave.");
}

bool isAdmin(const AccessList& access, const QByteArray& actor)
{
    std::optional<AccessUser> user = access.userOfDevice(actor);
    return user.has_value() && user->role() == AccessRole::Admin;
}

}  // namespace

QString AccessRules::invite(const AccessList& access, const QByteArray& actor, bool forUser)
{
    if (forUser && !isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    return QString();
}

QString AccessRules::removeUser(const AccessList& access, const QByteArray& actor, const UserId& user)
{
    if (!isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    if (access.userOfDevice(actor)->id() == user)
    {
        return useLeave();
    }
    return QString();
}

QString AccessRules::removeDevice(const AccessList& access, const QByteArray& actor, const QByteArray& device)
{
    std::optional<AccessUser> owner = access.userOfDevice(device);
    if (!owner.has_value())
    {
        return QString();
    }
    if (owner->id() != access.userOfDevice(actor)->id())
    {
        return QObject::tr("You can remove only your own devices.");
    }
    if (owner->devices().size() == 1)
    {
        return useLeave();
    }
    return QString();
}

QString AccessRules::withdrawInvite(const AccessList& access, const QByteArray& actor, const QByteArray& invite)
{
    std::optional<AccessInvite> entry = access.findInvite(invite);
    if (!entry.has_value()
        || isAdmin(access, actor)
        || entry->maker() == access.userOfDevice(actor)->id())
    {
        return QString();
    }
    return askAnAdmin(access);
}

QString AccessRules::setRole(const AccessList& access, const QByteArray& actor, const UserId& user, AccessRole role)
{
    if (!isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    std::optional<AccessUser> target = access.findUser(user);
    if (target.has_value()
        && target->role() == AccessRole::Admin
        && role == AccessRole::Member
        && access.adminNames().size() == 1)
    {
        return QObject::tr("You are the only admin. Make someone else an admin first.");
    }
    return QString();
}

QString AccessRules::join(const AccessList& access, const QByteArray& invite, const QByteArray& device)
{
    if (!access.findInvite(invite).has_value() && !access.findDevice(device).has_value())
    {
        return QObject::tr("This invite has been used or withdrawn.");
    }
    return QString();
}
