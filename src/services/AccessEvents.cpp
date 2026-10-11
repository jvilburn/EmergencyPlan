#include "AccessEvents.h"

#include <QObject>
#include <QSet>

namespace
{

QSet<QString> membersOf(const DocumentHeader& header)
{
    QSet<QString> members;
    for (const RecipientEntry& entry : header.recipients)
    {
        members.insert(entry.member.toHex());
    }
    return members;
}

QSet<QString> adminsOf(const DocumentHeader& header)
{
    QSet<QString> admins;
    for (const MemberId& admin : header.admins)
    {
        admins.insert(admin.toHex());
    }
    return admins;
}

/// The person whose device wrote header: listed in it, or - for a leave - in the
/// header before.
QString writerOf(const DocumentHeader& header, const DocumentHeader& previous)
{
    for (const DocumentHeader* candidate : {&header, &previous})
    {
        for (const RecipientEntry& entry : candidate->recipients)
        {
            if (entry.fingerprint == header.lastWriter)
            {
                return entry.member.toHex();
            }
        }
    }
    return QString();
}

QString nameOf(const QString& memberHex, const AccessRoster& current, const AccessRoster& base)
{
    const MemberId member = MemberId::fromHex(memberHex);
    for (const AccessRoster* roster : {&current, &base})
    {
        const std::optional<RosterPerson> person = roster->person(member);
        if (person.has_value())
        {
            return person.value().displayName();
        }
    }
    return QObject::tr("a former member");
}

/// Sorted, so the events of one header come in the same order on every device.
QStringList sorted(const QSet<QString>& members)
{
    QStringList list(members.cbegin(), members.cend());
    list.sort();
    return list;
}

void appendStep(QList<AccessEvents::Event>& events,
                const DocumentHeader& previous,
                const DocumentHeader& header,
                const AccessRoster& current,
                const AccessRoster& base)
{
    using Kind = AccessEvents::Event::Kind;

    const QSet<QString> before = membersOf(previous);
    const QSet<QString> after = membersOf(header);
    const QSet<QString> adminsBefore = adminsOf(previous);
    const QSet<QString> adminsAfter = adminsOf(header);
    const QString writer = writerOf(header, previous);
    const QString actor = writer.isEmpty() ? QObject::tr("a former member") : nameOf(writer, current, base);

    for (const QString& member : sorted(after - before))
    {
        events.append({Kind::Added, actor, nameOf(member, current, base), header.lastWriteTime});
    }
    for (const QString& member : sorted(before - after))
    {
        events.append({member == writer ? Kind::Left : Kind::Removed, actor, nameOf(member, current, base),
                       header.lastWriteTime});
    }

    const QSet<QString> stayed = before & after;
    for (const QString& member : sorted((adminsAfter - adminsBefore) & stayed))
    {
        events.append({Kind::MadeAdmin, actor, nameOf(member, current, base), header.lastWriteTime});
    }
    for (const QString& member : sorted((adminsBefore - adminsAfter) & stayed))
    {
        events.append({Kind::MadeMember, actor, nameOf(member, current, base), header.lastWriteTime});
    }
}

} // namespace

namespace AccessEvents
{

QString Event::describe() const
{
    switch (kind)
    {
    case Kind::Added:
        return QObject::tr("%1 added %2").arg(actor, subject);
    case Kind::Removed:
        return QObject::tr("%1 removed %2").arg(actor, subject);
    case Kind::Left:
        return QObject::tr("%1 left").arg(subject);
    case Kind::MadeAdmin:
        return QObject::tr("%1 made %2 an admin").arg(actor, subject);
    case Kind::MadeMember:
        return actor == subject ? QObject::tr("%1 stepped down as admin").arg(subject)
                                : QObject::tr("%1 made %2 a member").arg(actor, subject);
    }
    return QString();
}

QList<Event> between(const DocumentHeader& previousBase,
                     const QList<DocumentHeader>& laterHeaders,
                     const AccessRoster& current,
                     const AccessRoster& base)
{
    QList<Event> events;
    const DocumentHeader* previous = &previousBase;
    for (const DocumentHeader& header : laterHeaders)
    {
        appendStep(events, *previous, header, current, base);
        previous = &header;
    }
    return events;
}

QHash<QString, Origin> deviceOrigins(const QList<DocumentHeader>& chain)
{
    QHash<QString, Origin> origins;
    for (const DocumentHeader& header : chain)
    {
        // A device unlinked and later linked again was added by the later link.
        QSet<QString> listed;
        for (const RecipientEntry& entry : header.recipients)
        {
            const QString key = entry.fingerprint.toHex();
            listed.insert(key);
            if (!origins.contains(key))
            {
                origins.insert(key, Origin{header.lastWriter, header.lastWriteTime});
            }
        }
        for (const QString& key : origins.keys())
        {
            if (!listed.contains(key))
            {
                origins.remove(key);
            }
        }
    }
    return origins;
}

} // namespace AccessEvents
