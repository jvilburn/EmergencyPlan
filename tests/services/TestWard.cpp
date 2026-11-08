#include "TestWard.h"
#include "DocumentChange.h"
#include "JsonReader.h"

#include <QTimeZone>

namespace
{

/// The T json holds, which the test ward builds well formed.
template<typename T>
T readWellFormed(const QJsonObject& json)
{
    JsonReader reader(json);
    T value = T::fromJson(reader);
    Q_ASSERT_X(!reader.failed(), "TestWard", qPrintable(reader.problem()));
    return value;
}

}  // namespace

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
    return Family::createWithId(smithId(), address("12 Elm St", "Provo, UT"), {bob(), alice()});
}

Family TestWard::jones()
{
    return Family::createWithId(jonesId(), address("4 Oak Ave", "Provo, UT"), {carol()});
}

Person TestWard::leeMember(const QString& id, const QString& givenName, std::optional<Gender> gender,
                           std::optional<int> birthYear, bool isParent)
{
    return Person::createWithId(PersonId::fromString(id), Name("Lee", givenName), gender,
                                Birthday::create(birthYear, 1, 1), Phone(), Phone(), QString(),
                                QStringList(), isParent, QString(), QString());
}

Family TestWard::lee(std::optional<int> fayBirthYear)
{
    return Family::createWithId(FamilyId::fromString("family-lee"), address("8 Pine St", "Provo, UT"),
                                {leeMember("person-ken", "Ken", Gender::Male, 1975, true),
                                 leeMember("person-mia", "Mia", Gender::Female, 1977, true),
                                 leeMember("person-ed", "Ed", Gender::Male, 2005, false),
                                 leeMember("person-fay", "Fay", Gender::Female, fayBirthYear, false)});
}

Person TestWard::gus()
{
    return leeMember("person-gus", "Gus", Gender::Male, 2008, false);
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
    bob.setNote("Drives the ward van");

    Family saved = smith();
    saved.setAddress(address("14 Elm St", "Provo, UT"));
    saved.setNote("Gate code is 1234");
    saved.setMembers({bob, dan()});
    return saved;
}

Document TestWard::document()
{
    Document document;
    document.setFamily(smith());
    document.setFamily(jones());
    document.onDocumentChanged(DocumentChange::full());
    return document;
}

Team TestWard::setupTeam()
{
    QJsonObject json;
    json["id"] = setupTeamId().toString();
    json["name"] = "Setup";
    json["memberIds"] = QJsonArray({bobId().toString(), aliceId().toString()});
    return readWellFormed<Team>(json);
}

EmergencyAsset TestWard::generator()
{
    QJsonObject json;
    json["id"] = generatorId().toString();
    json["name"] = "Generator";
    json["responseArea"] = responseAreaToString(ResponseArea::Recovery);
    json["personIds"] = QJsonArray({bobId().toString(), aliceId().toString()});
    return readWellFormed<EmergencyAsset>(json);
}

MinisteringGroup TestWard::eqGroup()
{
    QJsonObject json;
    json["id"] = eqGroupId().toString();
    json["isRSFormat"] = false;
    json["ministerIds"] = QJsonArray({bobId().toString()});
    json["familyIds"] = QJsonArray({jonesId().toString(), smithId().toString()});
    json["presidencyMemberId"] = aliceId().toString();
    return readWellFormed<MinisteringGroup>(json);
}

MinisteringGroup TestWard::rsGroup()
{
    QJsonObject json;
    json["id"] = rsGroupId().toString();
    json["isRSFormat"] = true;
    json["ministerIds"] = QJsonArray({carolId().toString()});
    json["ministeredPersonIds"] = QJsonArray({aliceId().toString()});
    return readWellFormed<MinisteringGroup>(json);
}

MinisteringDistrict TestWard::district()
{
    QJsonObject json;
    json["id"] = districtId().toString();
    json["name"] = "District 1";
    json["presidencyMemberId"] = bobId().toString();
    json["groupIds"] = QJsonArray({eqGroupId().toString()});
    return readWellFormed<MinisteringDistrict>(json);
}

Document TestWard::documentWithPlaces()
{
    Team setup = setupTeam();
    setup.setLeaderId(bobId());

    ResponseTask shingles = task(shinglesTaskId().toString(), jones(), octoberFirst(10, 0), "Repairs",
                                 "Shingles off the roof");
    shingles.setAssignment(TaskAssignment::toPerson(alice()));
    ResponseTask basement = task(basementTaskId().toString(), smith(), octoberFirst(10, 30), "Flooding",
                                 "Water in the basement");
    basement.setAssignment(TaskAssignment::toTeam(setup));

    Document document = withAsset(withTeam(TestWard::document(), setup), generator());
    document.setGroups(MinisteringOrg::EldersQuorum, {{eqGroupId(), eqGroup()}});
    document.setDistricts(MinisteringOrg::EldersQuorum, {{districtId(), district()}});
    document.setGroups(MinisteringOrg::ReliefSociety, {{rsGroupId(), rsGroup()}});
    return withResponse(document, stormWith({}, {shingles, basement}));
}

Document TestWard::withTeam(const Document& document, const Team& team)
{
    Document updated = document;
    updated.setTeam(team);
    return updated;
}

Document TestWard::withAsset(const Document& document, const EmergencyAsset& asset)
{
    Document updated = document;
    updated.setEmergencyAsset(asset);
    updated.onDocumentChanged(DocumentChange::full());
    return updated;
}

QDateTime TestWard::octoberFirst(int hour, int minute)
{
    return QDateTime(QDate(2026, 10, 1), QTime(hour, minute), QTimeZone(QTimeZone::UTC));
}

EmergencyResponse TestWard::storm()
{
    QJsonObject json = EmergencyResponse::create("Storm").toJson();
    json["startedAt"] = octoberFirst(8, 0).toString(Qt::ISODateWithMs);
    return readWellFormed<EmergencyResponse>(json);
}

EmergencyResponse TestWard::flood()
{
    QJsonObject json = EmergencyResponse::create("Flood").toJson();
    json["startedAt"] = octoberFirst(9, 0).toString(Qt::ISODateWithMs);
    return readWellFormed<EmergencyResponse>(json);
}

ContactAttempt TestWard::attempt(const QString& id, const Family& family, const QDateTime& timestamp,
                                 std::optional<ContactMethod> method,
                                 const std::optional<Person>& who, std::optional<ContactOutcome> outcome,
                                 const QString& notes)
{
    QJsonObject json = ContactAttempt::create(family, method, who, outcome, notes).toJson();
    json["id"] = id;
    json["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
    return readWellFormed<ContactAttempt>(json);
}

ResponseTask TestWard::task(const QString& id, const Family& family, const QDateTime& createdAt,
                            const QString& category, const QString& description)
{
    QJsonObject json = ResponseTask::create(family, category, description).toJson();
    json["id"] = id;
    json["createdAt"] = createdAt.toString(Qt::ISODateWithMs);
    return readWellFormed<ResponseTask>(json);
}

TaskResolution TestWard::resolution(const QString& notes, const QDateTime& resolvedAt)
{
    QJsonObject json = TaskResolution::create(notes).toJson();
    json["resolvedAt"] = resolvedAt.toString(Qt::ISODateWithMs);
    return readWellFormed<TaskResolution>(json);
}

TaskNotification TestWard::notification(ContactMethod method, const QString& notes, const QDateTime& timestamp)
{
    QJsonObject json = TaskNotification::create(method, notes).toJson();
    json["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
    return readWellFormed<TaskNotification>(json);
}

EmergencyResponse TestWard::stormWith(const QList<ContactAttempt>& attempts, const QList<ResponseTask>& tasks)
{
    EmergencyResponse response = storm();
    for (const ContactAttempt& attempt : attempts)
    {
        response.insertContactAttempt(attempt);
    }
    for (const ResponseTask& task : tasks)
    {
        response.insertTask(task);
    }
    return response;
}

Document TestWard::withResponse(const Document& document, const EmergencyResponse& response)
{
    Document updated = document;
    updated.setEmergencyResponse(response);
    updated.onDocumentChanged(DocumentChange::full());
    return updated;
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
    updated.setFamily(family);
    updated.onDocumentChanged(DocumentChange::full());
    return updated;
}

Document TestWard::withCoordinates(const Document& document, const Address& address, const Coordinates& coordinates)
{
    Document updated = document;
    updated.setAddressCoordinates(address.full(), coordinates);
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
