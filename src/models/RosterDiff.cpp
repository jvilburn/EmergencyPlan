#include "RosterDiff.h"

#include <QObject>

namespace
{

QString roleName(Role role)
{
    return role == Role::Admin ? QObject::tr("an admin") : QObject::tr("a member");
}

} // namespace

namespace RosterDiff
{

QStringList describe(const SealingSet& previous, const SealingSet& current)
{
    QStringList lines;

    for (const Holder& was : previous.people())
    {
        const std::optional<Holder> now = current.person(was.id);
        if (!now.has_value())
        {
            lines.append(QObject::tr("%1 no longer has access").arg(was.name));
            continue;
        }
        if (now->role != was.role)
        {
            lines.append(QObject::tr("%1 is now %2").arg(now->name, roleName(now->role)));
        }
        for (const NamedDevice& device : was.devices)
        {
            if (!now->devices.contains(device))
            {
                lines.append(QObject::tr("%1 unlinked a device, %2").arg(now->name, device.name));
            }
        }
        for (const NamedDevice& device : now->devices)
        {
            if (!was.devices.contains(device))
            {
                lines.append(QObject::tr("%1 added a device, %2").arg(now->name, device.name));
            }
        }
    }

    for (const Holder& now : current.people())
    {
        if (!previous.person(now.id).has_value())
        {
            lines.append(QObject::tr("%1 joined, as %2").arg(now.name, roleName(now.role)));
        }
    }

    for (const Invite& was : previous.invites())
    {
        if (!current.invite(was.fingerprint()).has_value())
        {
            lines.append(QObject::tr("The invite %1 was used or withdrawn").arg(was.label));
        }
    }
    for (const Invite& now : current.invites())
    {
        if (!previous.invite(now.fingerprint()).has_value())
        {
            const std::optional<Holder> maker = current.person(now.person);
            lines.append(QObject::tr("%1 made an invite, %2").arg(maker.has_value() ? maker->name : QObject::tr("Someone"), now.label));
        }
    }

    return lines;
}

} // namespace RosterDiff
