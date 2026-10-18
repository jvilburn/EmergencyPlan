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

Person TestWard::dan()
{
    return Person::createWithId(danId(), Name("Smith", "Dan"), Gender::Male, Birthday(),
                                Phone(), Phone(), QString(), QStringList(),
                                false, QString(), QString());
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

Family TestWard::smithWithEveryFieldChanged()
{
    Person bob = TestWard::bob();
    bob.setName(Name("Smith", "Robert"));
    bob.setGender(std::nullopt);
    bob.setPhone(Phone("555-2222"));
    bob.setAltPhone(Phone("555-2223"));
    bob.setEmail("robert@example.com");
    bob.setBirthday(Birthday::create(1980, 7, 4));
    bob.setCallings({"Clerk"});
    bob.setIsParent(false);
    bob.setWardUnitNumber("123456");
    bob.setStakeUnitNumber("654321");
    bob.setSpecialNeedNote("Uses a wheelchair; the ramp is at the back door.");

    Family saved = smith();
    saved.setAddress(address("14 Elm St", "Provo, UT"));
    saved.setMembers({bob, dan()});
    return saved;
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
