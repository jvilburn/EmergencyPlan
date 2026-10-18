#include "Name.h"

#include <QStringList>

Name::Name(const QString& surname, const QString& givenNames)
    : QString(surname + ", " + givenNames)
{
}

QString Name::surname() const
{
    int commaIndex = indexOf(',');
    if (commaIndex < 0)
    {
        return *this;
    }
    return left(commaIndex).trimmed();
}

QString Name::givenNames() const
{
    int commaIndex = indexOf(',');
    if (commaIndex < 0)
    {
        return QString();
    }
    return mid(commaIndex + 1).trimmed();
}

QString Name::firstName() const
{
    QString given = givenNames();
    int spaceIndex = given.indexOf(' ');
    if (spaceIndex < 0)
    {
        return given;
    }
    return given.left(spaceIndex);
}

// ============================================================================
// Static functions (for raw QString classification/matching)
// ============================================================================

bool Name::isSurnameOnly(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(','))
    {
        return false;
    }
    for (const QChar& ch : trimmed)
    {
        if (!ch.isLetter() && !ch.isSpace())
        {
            return false;
        }
    }
    return true;
}

bool Name::looksLikeFullName(const QString& text)
{
    // Check for ", " with non-empty text on both sides
    int commaPos = text.indexOf(", ");
    if (commaPos <= 0)
    {
        return false;
    }
    return commaPos < text.trimmed().length() - 2;  // At least one char after ", "
}
