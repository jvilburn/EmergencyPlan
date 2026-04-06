#pragma once

#include "EmergencyResponse.h"

#include <QIcon>
#include <QString>

/// Shared status icon utilities for tree models.
namespace StatusIcons
{
    // Named Unicode constants for status display
    inline const QString Checkmark = QString::fromUtf8("\xe2\x9c\x93");  // ✓
    inline const QString Bullet = QString::fromUtf8("\xe2\x80\xa2");     // •
    inline const QString Flag = QString::fromUtf8("\xe2\x9a\x91");       // ⚑
    inline const QString Circle = QString::fromUtf8("\xe2\x97\x8b");     // ○

    /// Returns a cached QIcon for the given contact status.
    /// Returns an empty QIcon for NotContacted.
    QIcon iconForStatus(EffectiveContactStatus status);
}
