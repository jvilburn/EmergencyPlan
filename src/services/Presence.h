#pragma once

#include "AccessList.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QSet>
#include <QString>
#include <QStringList>

/// Who else has the ward open, as the window says it.
class Presence
{
    Q_DECLARE_TR_FUNCTIONS(Presence)

public:
    /// One name for each of present, the other devices here, as access names it, sorted: for
    /// a device, its user's name; for an open invite, "someone with Bob's invitation" by its
    /// label, or "a device invitation"; and for a fingerprint access does not know, "a device
    /// that just joined". A name is given once however many of present it stands for, so a
    /// user here on two devices is named once.
    static QStringList others(const AccessList& access, const QSet<QByteArray>& present);

    /// "Alice Smith is also here", "Alice Smith and Bob Jones are also here", or with three or
    /// more, "Alice Smith, Bob Jones and Carol Lee are also here"; empty for no one.
    static QString line(const QStringList& others);
};
