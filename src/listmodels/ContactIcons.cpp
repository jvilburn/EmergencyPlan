#include "ContactIcons.h"
#include "Person.h"

QString formatContactSuffix(const Person& person)
{
    QStringList parts;
    if (!person.phone().isEmpty())
    {
        parts.append(person.phone());
    }
    if (!person.email().isEmpty())
    {
        parts.append(person.email());
    }
    if (parts.isEmpty())
    {
        return QString();
    }
    return QString::fromUtf8(" \u2014 ") + parts.join(QString::fromUtf8(" \u2014 "));
}
