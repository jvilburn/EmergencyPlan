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

bool Name::matches(const Name& other) const
{
    return trimmed().compare(other.trimmed(), Qt::CaseInsensitive) == 0;
}

bool Name::matchesComponents(const Name& other) const
{
    if (matches(other))
    {
        return true;
    }

    // Compare surname + first name only (ignore middle names)
    if (surname().compare(other.surname(), Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    return firstName().compare(other.firstName(), Qt::CaseInsensitive) == 0;
}

bool Name::firstNameMatches(const QString& targetFirstName) const
{
    QString myFirst = firstName();

    if (myFirst.compare(targetFirstName, Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    // Handle case where targetFirstName might include middle name
    int spaceIndex = targetFirstName.indexOf(' ');
    if (spaceIndex > 0)
    {
        QString targetFirst = targetFirstName.left(spaceIndex);
        return myFirst.compare(targetFirst, Qt::CaseInsensitive) == 0;
    }

    return false;
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

bool Name::match(const QString& name1, const QString& name2)
{
    return name1.trimmed().compare(name2.trimmed(), Qt::CaseInsensitive) == 0;
}

bool Name::matchComponents(const QString& name1, const QString& name2)
{
    // First try exact match
    if (match(name1, name2))
    {
        return true;
    }

    // Try matching without middle names
    // "Smith, John" should match "Smith, John Michael"
    int comma1 = name1.indexOf(',');
    int comma2 = name2.indexOf(',');

    if (comma1 > 0 && comma2 > 0)
    {
        QString surname1 = name1.left(comma1).trimmed();
        QString surname2 = name2.left(comma2).trimmed();

        if (surname1.compare(surname2, Qt::CaseInsensitive) == 0)
        {
            // Surnames match, check first names (first word only)
            QString given1 = name1.mid(comma1 + 1).trimmed();
            QString given2 = name2.mid(comma2 + 1).trimmed();

            QString firstName1 = given1.split(' ', Qt::SkipEmptyParts).value(0);
            QString firstName2 = given2.split(' ', Qt::SkipEmptyParts).value(0);

            if (firstName1.compare(firstName2, Qt::CaseInsensitive) == 0)
            {
                return true;
            }
        }
    }

    return false;
}

bool Name::firstNameMatches(const QString& fullName, const QString& targetFirstName)
{
    // Full name is in "Surname, First Middle" format
    int commaPos = fullName.indexOf(',');
    if (commaPos < 0)
    {
        return false;
    }

    QString givenPart = fullName.mid(commaPos + 1).trimmed();
    QString personFirstName = givenPart.split(' ', Qt::SkipEmptyParts).value(0);

    if (personFirstName.compare(targetFirstName, Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    // Also handle case where targetFirstName might have middle name
    QString targetFirstOnly = targetFirstName.split(' ', Qt::SkipEmptyParts).value(0);
    if (personFirstName.compare(targetFirstOnly, Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    return false;
}
