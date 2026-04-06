#pragma once

#include <QString>

/// Emoji prefixes for contact detail display text.
namespace ContactIcons
{
    inline const QString Phone = QString::fromUtf8("\xf0\x9f\x93\x9e ");    // 📞
    inline const QString Email = QString::fromUtf8("\xf0\x9f\x93\xa7 ");    // 📧
    inline const QString Address = QString::fromUtf8("\xf0\x9f\x93\x8d ");  // 📍
}
