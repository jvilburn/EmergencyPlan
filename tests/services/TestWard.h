#pragma once

#include "Document.h"
#include "EmergencyAsset.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Person.h"
#include "Team.h"

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
    static PersonId danId() { return PersonId::fromString("person-dan"); }
    static TeamId setupTeamId() { return TeamId::fromString("team-setup"); }
    static EmergencyAssetId generatorId() { return EmergencyAssetId::fromString("asset-generator"); }

    static Person bob();
    static Person alice();
    static Person carol();

    /// Dan Smith, male, not a parent, with no birthday or contact details.
    static Person dan();

    static Family smith();
    static Family jones();

    /// A Lee family member born on 1 January of birthYear, with no contact details.
    static Person leeMember(const QString& id, const QString& givenName, std::optional<Gender> gender,
                            std::optional<int> birthYear, bool isParent);

    /// The Lee family at 8 Pine St, Provo, UT, which is not in document(): Ken (father, 1975),
    /// Mia (mother, 1977), Ed (child, 2005) and Fay (child, born in fayBirthYear).
    static Family lee(std::optional<int> fayBirthYear);

    /// Gus Lee, a boy born in 2008, not a parent.
    static Person gus();

    static Address address(const QString& street, const QString& city);

    /// The Smith family with every field of Bob changed, Alice removed, Dan added, and the
    /// address and note changed, so an edit to it holds every value type a family edit holds.
    static Family smithWithEveryFieldChanged();

    /// The ward holding the Smith and Jones families, with its lookup caches built.
    static Document document();

    /// The Setup team, which is not in document(): Bob and Alice, with no leader.
    static Team setupTeam();

    /// The Generator asset in the recovery area, which is not in document(): Bob and Alice.
    static EmergencyAsset generator();

    /// document with team added, or put in place of the team with the same id.
    static Document withTeam(const Document& document, const Team& team);

    /// document with asset added, or put in place of the asset with the same id, caches
    /// rebuilt.
    static Document withAsset(const Document& document, const EmergencyAsset& asset);

    /// 1 October 2026 at hour:minute UTC.
    static QDateTime octoberFirst(int hour, int minute);

    /// The Storm emergency, which is not in document(): started on 1 October 2026 at 08:00
    /// UTC, with the default task categories and no attempts or tasks.
    static EmergencyResponse storm();

    /// The Flood emergency, which is not in document(): started on 1 October 2026 at 09:00
    /// UTC, with the default task categories and no attempts or tasks.
    static EmergencyResponse flood();

    /// An attempt with that id made at timestamp, otherwise as ContactAttempt::create makes
    /// it.
    static ContactAttempt attempt(const QString& id, const FamilyId& familyId, const QDateTime& timestamp,
                                  std::optional<ContactMethod> method,
                                  const std::optional<PersonId>& who, std::optional<ContactOutcome> outcome,
                                  const QString& notes);

    /// A task with that id created at createdAt, unassigned and unresolved.
    static ResponseTask task(const QString& id, const FamilyId& familyId, const QDateTime& createdAt,
                             const QString& category, const QString& description);

    /// A resolution with those notes made at resolvedAt.
    static TaskResolution resolution(const QString& notes, const QDateTime& resolvedAt);

    /// A notification by method with those notes made at timestamp.
    static TaskNotification notification(ContactMethod method, const QString& notes, const QDateTime& timestamp);

    /// The Storm emergency holding attempts and tasks.
    static EmergencyResponse stormWith(const QList<ContactAttempt>& attempts, const QList<ResponseTask>& tasks);

    /// document with response as its emergency in progress, caches rebuilt.
    static Document withResponse(const Document& document, const EmergencyResponse& response);

    /// family with the member of the same id replaced by member.
    static Family withMember(const Family& family, const Person& member);

    /// family with that member's phone set to phone.
    static Family withPhone(const Family& family, const PersonId& id, const QString& phone);

    /// document with the family of the same id replaced by family, caches rebuilt.
    static Document withFamily(const Document& document, const Family& family);

    /// document with the entry for address in its table of address coordinates set to
    /// coordinates.
    static Document withCoordinates(const Document& document, const Address& address, const Coordinates& coordinates);

    /// The family with that id; throws std::bad_optional_access when it is absent.
    static Family family(const Document& document, const FamilyId& id);

    /// The person with that id; throws std::bad_optional_access when it is absent.
    static Person person(const Document& document, const PersonId& id);
};
