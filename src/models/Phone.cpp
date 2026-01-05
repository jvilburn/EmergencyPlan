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

bool Phone::isValid() const
{
    return digits().length() >= 7;
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

bool Phone::match(const QString& phone1, const QString& phone2)
{
    QString digits1 = extractDigits(phone1);
    QString digits2 = extractDigits(phone2);
    return !digits1.isEmpty() && digits1 == digits2;
}

QString Phone::extractDigits(const QString& phone)
{
    QString result;
    for (const QChar& ch : phone)
    {
        if (ch.isDigit())
        {
            result.append(ch);
        }
    }
    return result;
}
