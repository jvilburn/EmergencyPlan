#include "Presence.h"

namespace
{
/// What the window calls the device or invite with this fingerprint.
QString nameOf(const AccessList& access, const QByteArray& fingerprint)
{
    std::optional<AccessUser> user = access.userOfDevice(fingerprint);
    if (user.has_value())
    {
        return user->name();
    }
    std::optional<AccessInvite> invite = access.findInvite(fingerprint);
    if (!invite.has_value())
    {
        return Presence::tr("a device that just joined");
    }
    if (invite->invitedUser().has_value())
    {
        return Presence::tr("someone with %1's invitation").arg(invite->invitedUser()->label);
    }
    return Presence::tr("a device invitation");
}
}  // namespace

QStringList Presence::others(const AccessList& access, const QSet<QByteArray>& present)
{
    QStringList names;
    for (const QByteArray& fingerprint : present)
    {
        names.append(nameOf(access, fingerprint));
    }
    names.removeDuplicates();
    names.sort();
    return names;
}

QString Presence::line(const QStringList& others)
{
    if (others.isEmpty())
    {
        return QString();
    }
    if (others.size() == 1)
    {
        return tr("%1 is also here").arg(others.first());
    }
    QStringList allButLast = others.mid(0, others.size() - 1);
    return tr("%1 and %2 are also here").arg(allButLast.join(tr(", ")), others.last());
}
