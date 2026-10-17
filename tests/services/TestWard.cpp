#include "TestWard.h"

Person TestWard::bob()
{
    return Person::createWithId(bobId(), Name("Smith", "Bob"), Gender::Male, Birthday(),
                                Phone("555-1111"), Phone(), "bob@example.com", QStringList(),
                                true, QString(), QString());
}

Person TestWard::alice()
{
    return Person::createWithId(aliceId(), Name("Smith", "Alice"), Gender::Female, Birthday(),
                                Phone("555-3333"), Phone(), "alice@example.com", QStringList(),
                                true, QString(), QString());
}

Person TestWard::carol()
{
    return Person::createWithId(carolId(), Name("Jones", "Carol"), Gender::Female, Birthday(),
                                Phone("555-4444"), Phone(), "carol@example.com", QStringList(),
                                true, QString(), QString());
}

Address TestWard::address(const QString& street, const QString& city)
{
    Address address;
    address.addLine(street);
    address.addLine(city);
    return address;
}

Family TestWard::smith()
{
    return Family::createWithId(smithId(), std::nullopt, std::nullopt,
                                address("12 Elm St", "Provo, UT"), {bob(), alice()});
}

Family TestWard::jones()
{
    return Family::createWithId(jonesId(), std::nullopt, std::nullopt,
                                address("4 Oak Ave", "Provo, UT"), {carol()});
}

Document TestWard::document()
{
    Document document;
    document.addFamily(smith());
    document.addFamily(jones());
    document.onDocumentChanged(DocumentChange::full());
    return document;
}

Family TestWard::withMember(const Family& family, const Person& member)
{
    QList<Person> members = family.members();
    for (Person& existing : members)
    {
        if (existing.id() == member.id())
        {
            existing = member;
        }
    }
    Family updated = family;
    updated.setMembers(members);
    return updated;
}

Family TestWard::withPhone(const Family& family, const PersonId& id, const QString& phone)
{
    for (const Person& member : family.members())
    {
        if (member.id() == id)
        {
            Person updated = member;
            updated.setPhone(Phone(phone));
            return withMember(family, updated);
        }
    }
    Q_ASSERT_X(false, "TestWard::withPhone", "the family has no member with that id");
    return family;
}

Document TestWard::withFamily(const Document& document, const Family& family)
{
    Document updated = document;
    updated.updateFamily(family);
    updated.onDocumentChanged(DocumentChange::full());
    return updated;
}

Family TestWard::family(const Document& document, const FamilyId& id)
{
    return document.findFamilyById(id).value();
}

Person TestWard::person(const Document& document, const PersonId& id)
{
    return document.findPersonById(id).value();
}
