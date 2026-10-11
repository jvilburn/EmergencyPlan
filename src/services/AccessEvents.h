#pragma once

#include "AccessRoster.h"
#include "CryptoTypes.h"
#include "DocumentHeader.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

/// Changes to who has access, read from headers alone, so every member sees what
/// any admin did within one sync - which is what makes a hostile admin's actions
/// visible.
namespace AccessEvents
{

struct Event
{
    enum class Kind
    {
        Added,
        Removed,
        Left,
        MadeAdmin,
        MadeMember
    };

    Kind kind;
    QString actor;      ///< who wrote the header that made it
    QString subject;    ///< whom it is about
    QDateTime when;

    /// One line for the list: "Bob Wilson added Tom Hale", "Tom Hale left".
    QString describe() const;
};

/// The people-level changes from previousBase through laterHeaders, in order.
/// Linking and unlinking a person's own devices is not among them. Names come from
/// the current roster, then the base copy's, then "a former member".
QList<Event> between(const DocumentHeader& previousBase,
                     const QList<DocumentHeader>& laterHeaders,
                     const AccessRoster& current,
                     const AccessRoster& base);

/// Which device added a device, and when: the writer of the first header that
/// lists it.
struct Origin
{
    Fingerprint addedBy;
    QDateTime addedAt;
};

/// Each device the chain lists, by fingerprint hex, with its origin. chain is the
/// kept headers in order, ending with the current one.
QHash<QString, Origin> deviceOrigins(const QList<DocumentHeader>& chain);

} // namespace AccessEvents
