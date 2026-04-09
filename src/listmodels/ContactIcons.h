#pragma once

#include <QString>
#include <QStringList>

class Person;

/// Emoji prefixes for contact detail display text.
namespace ContactIcons
{
    inline const QString Phone = QString::fromUtf8("\xf0\x9f\x93\x9e ");    // 📞
    inline const QString Email = QString::fromUtf8("\xf0\x9f\x93\xa7 ");    // 📧
    inline const QString Address = QString::fromUtf8("\xf0\x9f\x93\x8d ");  // 📍
}

/// Returns " — phone — email" suffix for display, or empty string if no contact info.
QString formatContactSuffix(const Person& person);
