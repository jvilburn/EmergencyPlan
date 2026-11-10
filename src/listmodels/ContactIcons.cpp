#include "ContactIcons.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"

#include <QObject>

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

QStringList personContactLines(const PersonId& personId, const Document& document)
{
    std::optional<Person> person = document.findPersonById(personId);
    if (!person.has_value())
    {
        return QStringList();
    }
    QStringList lines;
    if (!person->phone().isEmpty())
    {
        lines.append(ContactIcons::Phone + person->phone());
    }
    if (!person->altPhone().isEmpty())
    {
        lines.append(ContactIcons::Phone + person->altPhone() + QObject::tr(" (alt)"));
    }
    if (!person->email().isEmpty())
    {
        lines.append(ContactIcons::Email + person->email());
    }
    std::optional<FamilyId> familyId = document.familyIdForPerson(personId);
    std::optional<Family> family = familyId.has_value() ? document.findFamilyById(*familyId) : std::nullopt;
    if (family.has_value() && !family->address().isEmpty())
    {
        lines.append(ContactIcons::Address + family->address().full());
    }
    return lines;
}

QStringList familyContactLines(const FamilyId& familyId, const Document& document)
{
    std::optional<Family> family = document.findFamilyById(familyId);
    if (!family.has_value())
    {
        return QStringList();
    }
    QStringList lines;
    if (!family->displayPhone().isEmpty())
    {
        lines.append(ContactIcons::Phone + family->displayPhone());
    }
    if (!family->address().isEmpty())
    {
        lines.append(ContactIcons::Address + family->address().full());
    }
    return lines;
}
