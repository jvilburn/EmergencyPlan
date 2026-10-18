#include "Phone.h"

QString Phone::digits() const
{
    QString result;
    for (const QChar& ch : *this)
    {
        if (ch.isDigit())
        {
            result.append(ch);
        }
    }
    return result;
}

bool Phone::matches(const Phone& other) const
{
    QString d1 = digits();
    QString d2 = other.digits();
    return !d1.isEmpty() && d1 == d2;
}

// ============================================================================
// Static functions (for raw QString classification/matching)
// ============================================================================

bool Phone::isPhone(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    QChar first = trimmed[0];
    if (!first.isDigit() && first != '(' && first != '+')
    {
        return false;
    }

    int digitCount = 0;
    for (const QChar& ch : trimmed)
    {
        if (ch.isDigit())
        {
            ++digitCount;
        }
    }
    return digitCount >= 7;
}
