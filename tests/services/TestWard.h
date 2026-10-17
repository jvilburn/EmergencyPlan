#pragma once

#include "Document.h"
#include "Family.h"
#include "Person.h"

/// A small ward with fixed ids, and helpers for building versions of it.
///
/// The Smith family at 12 Elm St, Provo, UT: Bob (555-1111, bob@example.com) and Alice
/// (555-3333, alice@example.com), both parents. The Jones family at 4 Oak Ave, Provo, UT:
/// Carol (555-4444, carol@example.com), a parent.
class TestWard
{
public:
    static FamilyId smithId() { return FamilyId::fromString("family-smith"); }
    static FamilyId jonesId() { return FamilyId::fromString("family-jones"); }
    static PersonId bobId() { return PersonId::fromString("person-bob"); }
    static PersonId aliceId() { return PersonId::fromString("person-alice"); }
    static PersonId carolId() { return PersonId::fromString("person-carol"); }

    static Person bob();
    static Person alice();
    static Person carol();
    static Family smith();
    static Family jones();
    static Address address(const QString& street, const QString& city);

    /// The ward holding the Smith and Jones families, with its lookup caches built.
    static Document document();

    /// family with the member of the same id replaced by member.
    static Family withMember(const Family& family, const Person& member);

    /// family with that member's phone set to phone.
    static Family withPhone(const Family& family, const PersonId& id, const QString& phone);

    /// document with the family of the same id replaced by family, caches rebuilt.
    static Document withFamily(const Document& document, const Family& family);

    /// The family with that id; throws std::bad_optional_access when it is absent.
    static Family family(const Document& document, const FamilyId& id);

    /// The person with that id; throws std::bad_optional_access when it is absent.
    static Person person(const Document& document, const PersonId& id);
};
