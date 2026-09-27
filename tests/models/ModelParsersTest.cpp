#include "ModelParsersTest.h"

#include "Birthday.h"
#include "DocumentCrypto.h"
#include "DocumentMetadata.h"
#include "EmergencyAsset.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "ModelSamples.h"
#include "Person.h"
#include "Stake.h"
#include "Tag.h"
#include "Team.h"
#include "Ward.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

namespace
{

/// A value of any JSON type but the one given.
QJsonValue wrongTypeFor(const QJsonValue& value)
{
    return value.isString() ? QJsonValue(42) : QJsonValue(QString("not this type"));
}

/// The checks every strict reader meets. optionalKeys are the fields the writer
/// omits when empty; every other field of the sample is required. The sample
/// holds one element in each set, so the writer's output has a fixed order.
template<typename T>
void checkReader(const QJsonObject& sample, const QStringList& optionalKeys)
{
    // The sample carries every field, so every check below has something to test.
    for (const QString& key : optionalKeys)
    {
        QVERIFY2(sample.contains(key), qPrintable("the sample lacks " + key));
    }

    // Written back, it is the sample exactly: every field in its place with its
    // value, so a reader that swapped two same-typed fields fails here.
    const std::optional<T> read = T::fromJson(sample);
    QVERIFY(read.has_value());
    const QJsonObject written = read.value().toJson();
    QVERIFY2(written == sample, QJsonDocument(written).toJson().constData());

    for (const QString& key : sample.keys())
    {
        QJsonObject without = sample;
        without.remove(key);
        QVERIFY2(T::fromJson(without).has_value() == optionalKeys.contains(key),
                 qPrintable("removing " + key));

        QJsonObject wrong = sample;
        wrong[key] = wrongTypeFor(sample.value(key));
        QVERIFY2(!T::fromJson(wrong).has_value(), qPrintable("wrong type for " + key));
    }

    QJsonObject extra = sample;
    extra["unexpected"] = true;
    QVERIFY(!T::fromJson(extra).has_value());
}

template<typename T>
bool reads(const QJsonObject& json)
{
    return T::fromJson(json).has_value();
}

/// The sample with one field replaced.
QJsonObject with(QJsonObject json, const char* key, const QJsonValue& value)
{
    json[QLatin1String(key)] = value;
    return json;
}

} // namespace

void ModelParsersTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Every field of every type ------------------------------------------------

void ModelParsersTest::birthday()
{
    checkReader<Birthday>(QJsonObject{{"year", 1980}, {"month", 4}, {"day", 12}}, {"year", "month", "day"});

    // Never written empty.
    QVERIFY(!reads<Birthday>(QJsonObject()));
}

void ModelParsersTest::person()
{
    checkReader<Person>(ModelSamples::person(),
                        {"phone",
                         "altPhone",
                         "email",
                         "gender",
                         "birthday",
                         "callings",
                         "stakeUnitNumber",
                         "wardUnitNumber",
                         "isParent",
                         "specialNeedNote"});
}

void ModelParsersTest::family()
{
    checkReader<Family>(ModelSamples::family(), {"latitude", "longitude", "address", "members"});

    // A damaged member refuses the family.
    QJsonObject damaged = ModelSamples::family();
    QJsonArray members = damaged["members"].toArray();
    members.replace(1, with(ModelSamples::secondPerson(), "name", 42));
    damaged["members"] = members;
    QVERIFY(!reads<Family>(damaged));
}

void ModelParsersTest::team()
{
    checkReader<Team>(ModelSamples::team(), {"color", "leaderId", "memberIds"});
}

void ModelParsersTest::tag()
{
    checkReader<Tag>(ModelSamples::tag(), {"color", "personIds", "familyIds"});
}

void ModelParsersTest::emergencyAsset()
{
    checkReader<EmergencyAsset>(ModelSamples::emergencyAsset(), {});

    // Always written, empty or not.
    QVERIFY(reads<EmergencyAsset>(with(ModelSamples::emergencyAsset(), "personIds", QJsonArray())));
}

void ModelParsersTest::ministeringDistrict()
{
    checkReader<MinisteringDistrict>(ModelSamples::district("d1", "m1"), {"presidencyMemberId", "groupIds"});
}

void ModelParsersTest::ministeringGroup()
{
    checkReader<MinisteringGroup>(ModelSamples::group("m1", false),
                                  {"ministerIds",
                                   "familyIds",
                                   "ministeredPersonIds",
                                   "interviewedDate",
                                   "presidencyMemberId"});

    QVERIFY(!reads<MinisteringGroup>(with(ModelSamples::group("m1", false), "interviewedDate", "2026-9-1")));
}

void ModelParsersTest::ward()
{
    checkReader<Ward>(ModelSamples::ward(),
                      {"name",
                       "unitNumber",
                       "stakeUnitNumber",
                       "chapelAddress",
                       "chapelLat",
                       "chapelLng",
                       "chapelPhone",
                       "meetingTime"});

    // The chapel fields are written whenever set, even when empty.
    QVERIFY(reads<Ward>(with(ModelSamples::ward(), "chapelPhone", "")));
}

void ModelParsersTest::stake()
{
    checkReader<Stake>(ModelSamples::stake(),
                       {"name",
                        "unitNumber",
                        "phone",
                        "email",
                        "stakeCenterAddress",
                        "stakeCenterLat",
                        "stakeCenterLng",
                        "wardUnitNumbers"});

    QVERIFY(!reads<Stake>(with(ModelSamples::stake(), "wardUnitNumbers", QJsonArray{"123456", "123456"})));
}

void ModelParsersTest::taskNotification()
{
    checkReader<TaskNotification>(ModelSamples::notification(), {"notes"});
}

void ModelParsersTest::contactAttempt()
{
    checkReader<ContactAttempt>(ModelSamples::attempt(), {"notes"});
}

void ModelParsersTest::responseTask()
{
    checkReader<ResponseTask>(ModelSamples::task(), {"assignment", "resolution"});

    const QJsonObject assignment = ModelSamples::task()["assignment"].toObject();
    const QJsonObject resolution = ModelSamples::task()["resolution"].toObject();

    // Inside each value, the notes and the notification are optional; the
    // assignee and the time are not.
    QJsonObject bare = assignment;
    bare.remove("notes");
    bare.remove("notification");
    QVERIFY(reads<ResponseTask>(with(ModelSamples::task(), "assignment", bare)));
    QJsonObject noNotes = resolution;
    noNotes.remove("notes");
    QVERIFY(reads<ResponseTask>(with(ModelSamples::task(), "resolution", noNotes)));
    QJsonObject noTime = resolution;
    noTime.remove("at");
    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "resolution", noTime)));

    // A team instead of a person is equally one assignee.
    QJsonObject toTeam = assignment;
    toTeam.remove("personId");
    toTeam["teamId"] = "t1";
    QVERIFY(reads<ResponseTask>(with(ModelSamples::task(), "assignment", toTeam)));

    // A damaged notification refuses the task.
    QJsonObject damaged = assignment;
    damaged["notification"] = QJsonObject{{"method", "text"}};
    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "assignment", damaged)));

    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "assignment", with(assignment, "unexpected", true))));
    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "resolution", with(resolution, "unexpected", true))));
}

void ModelParsersTest::familyResponseRecord()
{
    checkReader<FamilyResponseRecord>(ModelSamples::familyRecord(), {"address", "contactAttempts", "tasks"});
}

void ModelParsersTest::emergencyResponse()
{
    checkReader<EmergencyResponse>(ModelSamples::emergencyResponse(), {"endedAt"});

    // Always written, empty or not.
    QVERIFY(reads<EmergencyResponse>(with(ModelSamples::emergencyResponse(), "familyRecords", QJsonArray())));
    QVERIFY(reads<EmergencyResponse>(with(ModelSamples::emergencyResponse(), "taskCategories", QJsonArray())));

    // A set: addTaskCategory never repeats one.
    QVERIFY(!reads<EmergencyResponse>(
        with(ModelSamples::emergencyResponse(), "taskCategories", QJsonArray{"Flooding", "Flooding"})));
}

// --- Values in exactly the writer's form --------------------------------------

void ModelParsersTest::enumsAreReadInTheWritersSpellingOnly()
{
    // Nothing case-insensitive, nothing defaulted.
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "gender", "Female")));
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "gender", "unknown")));
    QVERIFY(reads<Person>(with(ModelSamples::person(), "gender", "male")));

    QVERIFY(!reads<Tag>(with(ModelSamples::tag(), "level", "Person")));
    QVERIFY(reads<Tag>(with(ModelSamples::tag(), "level", "family")));

    QVERIFY(!reads<EmergencyAsset>(with(ModelSamples::emergencyAsset(), "responseArea", "medical")));
    QVERIFY(reads<EmergencyAsset>(with(ModelSamples::emergencyAsset(), "responseArea", "")));

    QVERIFY(!reads<ContactAttempt>(with(ModelSamples::attempt(), "method", "Visit")));
    QVERIFY(reads<ContactAttempt>(with(ModelSamples::attempt(), "method", "other")));

    QVERIFY(!reads<FamilyResponseRecord>(with(ModelSamples::familyRecord(), "contactStatus", "OK")));
    QVERIFY(reads<FamilyResponseRecord>(with(ModelSamples::familyRecord(), "contactStatus", "unableToReach")));
}

void ModelParsersTest::timesMustCarryTheirMillisecondsInUtc()
{
    QVERIFY(!reads<ContactAttempt>(with(ModelSamples::attempt(), "timestamp", "2026-09-27T12:34:56Z")));
    QVERIFY(!reads<ContactAttempt>(with(ModelSamples::attempt(), "timestamp", "2026-09-27T07:34:56.789-05:00")));
    QVERIFY(!reads<ContactAttempt>(with(ModelSamples::attempt(), "timestamp", "yesterday")));
    QVERIFY(reads<ContactAttempt>(with(ModelSamples::attempt(), "timestamp", "2026-09-27T12:34:56.000Z")));
}

void ModelParsersTest::optionalFieldsAreNeverPresentButEmpty()
{
    // The writer omits these when empty, so present and empty is not its output.
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "phone", "")));
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "callings", QJsonArray())));
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "memberIds", QJsonArray())));
    QVERIFY(!reads<Family>(with(ModelSamples::family(), "members", QJsonArray())));
    QVERIFY(!reads<ContactAttempt>(with(ModelSamples::attempt(), "notes", "")));
    QVERIFY(!reads<Stake>(with(ModelSamples::stake(), "name", "")));
}

void ModelParsersTest::idSetsRefuseRepeats()
{
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "memberIds", QJsonArray{"p1", "p1"})));
    QVERIFY(!reads<Tag>(with(ModelSamples::tag(), "familyIds", QJsonArray{"f1", "f1"})));
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "memberIds", QJsonArray{"p1", ""})));
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "leaderId", "")));
    QVERIFY(!reads<Tag>(with(ModelSamples::tag(), "personIds", QJsonArray{"p1", 42})));
}

void ModelParsersTest::aPersonsFlagsAndBirthdayAreAsWritten()
{
    // isParent is written only when true.
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "isParent", false)));

    // A birthday is written only with a month and a day.
    QVERIFY(!reads<Person>(with(ModelSamples::person(), "birthday", QJsonObject{{"year", 1980}})));
    QVERIFY(reads<Person>(with(ModelSamples::person(), "birthday", QJsonObject{{"month", 4}, {"day", 12}})));

    // A birthday field that is not a whole number, or not one an int holds.
    QVERIFY(!reads<Birthday>(QJsonObject{{"month", 4.5}, {"day", 12}}));
    QVERIFY(!reads<Birthday>(QJsonObject{{"year", 1e10}, {"month", 4}, {"day", 12}}));
}

void ModelParsersTest::anAddressLineIsAsTheAddressKeepsIt()
{
    // Address trims and collapses whitespace, so a line it would change was not
    // written by it.
    QVERIFY(!reads<Family>(with(ModelSamples::family(), "address", QJsonArray{"  1 Oak St"})));
    QVERIFY(!reads<Family>(with(ModelSamples::family(), "address", QJsonArray{"1 Oak St", ""})));
    QVERIFY(reads<Family>(with(ModelSamples::family(), "address", QJsonArray{"1 Oak St"})));
}

void ModelParsersTest::aTeamColourIsTheNameQColorWrites()
{
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "color", "orange")));
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "color", "#FF8800")));
    QVERIFY(!reads<Team>(with(ModelSamples::team(), "color", "#f80")));
    QVERIFY(reads<Team>(with(ModelSamples::team(), "color", "#123abc")));
}

void ModelParsersTest::aTaskAssignmentHasExactlyOneAssignee()
{
    const QJsonObject assignment = ModelSamples::task()["assignment"].toObject();
    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "assignment", with(assignment, "teamId", "t1"))));

    QJsonObject nobody = assignment;
    nobody.remove("personId");
    QVERIFY(!reads<ResponseTask>(with(ModelSamples::task(), "assignment", nobody)));
}

void ModelParsersTest::aCollectionRefusesARepeatedId()
{
    // Otherwise the second would overwrite the first unseen.
    QJsonObject family = ModelSamples::family();
    family["members"] = QJsonArray{ModelSamples::person(), ModelSamples::person()};
    QVERIFY(!reads<Family>(family));

    QJsonObject record = ModelSamples::familyRecord();
    record["contactAttempts"] = QJsonArray{ModelSamples::attempt(), ModelSamples::attempt()};
    QVERIFY(!reads<FamilyResponseRecord>(record));

    QJsonObject response = ModelSamples::emergencyResponse();
    response["familyRecords"] = QJsonArray{ModelSamples::familyRecord(), ModelSamples::familyRecord()};
    QVERIFY(!reads<EmergencyResponse>(response));

    QJsonObject metadata;
    metadata["wards"] = QJsonArray{ModelSamples::ward(), ModelSamples::ward()};
    QVERIFY(!DocumentMetadata::fromJson(metadata).has_value());
    metadata["wards"] = QJsonArray{ModelSamples::ward()};
    QVERIFY(DocumentMetadata::fromJson(metadata).has_value());
}

void ModelParsersTest::timesSurviveTheRoundTripExactly()
{
    // Milliseconds included: a record read back from disk equals the one in
    // memory, which the three-way merge relies on when it compares against a base
    // copy.
    const ContactAttempt original = ContactAttempt::create(ContactMethod::Phone, PersonId::generate(), "Called");
    const std::optional<ContactAttempt> back = ContactAttempt::fromJson(original.toJson());
    QVERIFY(back.has_value());
    QVERIFY(back.value() == original);

    EmergencyResponse response = EmergencyResponse::create("Storm");
    response.setStartedAt(QDateTime::fromString("2026-09-27T12:34:56.789Z", Qt::ISODateWithMs));
    const std::optional<EmergencyResponse> responseBack = EmergencyResponse::fromJson(response.toJson());
    QVERIFY(responseBack.has_value());
    QCOMPARE(responseBack.value().startedAt().time().msec(), 789);
    QVERIFY(responseBack.value() == response);
}
