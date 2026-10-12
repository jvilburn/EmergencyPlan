#include "ModelSamples.h"

#include "Document.h"
#include "DocumentCrypto.h"
#include "SealingSet.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace
{

QJsonObject parsed(const char* text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

} // namespace

namespace ModelSamples
{

const char* const Time = "2026-09-27T12:34:56.789Z";

QJsonObject person()
{
    return parsed(R"({
        "id": "p1",
        "name": "Reed, Alice",
        "phone": "555-0100",
        "altPhone": "555-0101",
        "email": "alice@example.org",
        "gender": "female",
        "birthday": {"year": 1980, "month": 4, "day": 12},
        "callings": ["Primary Teacher"],
        "stakeUnitNumber": "510033",
        "wardUnitNumber": "123456",
        "isParent": true,
        "specialNeedNote": "Uses a wheelchair"
    })");
}

QJsonObject secondPerson()
{
    return parsed(R"({"id": "p2", "name": "Reed, Ben"})");
}

QJsonObject family()
{
    QJsonObject json = parsed(R"({
        "id": "f1",
        "latitude": 40.25,
        "longitude": -111.5,
        "address": ["1 Oak St", "Provo, UT 84601"]
    })");
    json["members"] = QJsonArray{person(), secondPerson()};
    return json;
}

QJsonObject team()
{
    return parsed(R"({
        "id": "t1",
        "name": "Chainsaw crew",
        "color": "#ff8800",
        "leaderId": "p1",
        "memberIds": ["p2"]
    })");
}

QJsonObject tag()
{
    return parsed(R"({
        "id": "g1",
        "name": "Choir",
        "level": "person",
        "color": "#00aa00",
        "personIds": ["p1"],
        "familyIds": ["f1"]
    })");
}

QJsonObject emergencyAsset()
{
    return parsed(R"({"id": "a1", "name": "Generator", "responseArea": "rec", "personIds": ["p1"]})");
}

QJsonObject district(const char* id, const char* groupId)
{
    QJsonObject json = parsed(R"({"name": "District 1", "presidencyMemberId": "p1"})");
    json["id"] = QString(id);
    json["groupIds"] = QJsonArray{QString(groupId)};
    return json;
}

QJsonObject group(const char* id, bool reliefSociety)
{
    QJsonObject json = parsed(R"({
        "ministerIds": ["p1"],
        "familyIds": ["f1"],
        "ministeredPersonIds": ["p2"],
        "interviewedDate": "2026-09-01",
        "presidencyMemberId": "p3"
    })");
    json["id"] = QString(id);
    json["isRSFormat"] = reliefSociety;
    return json;
}

QJsonObject ward()
{
    return parsed(R"({
        "name": "Oak Hills",
        "unitNumber": "123456",
        "stakeUnitNumber": "510033",
        "chapelAddress": "1 Chapel Rd",
        "chapelLat": 40.5,
        "chapelLng": -111.25,
        "chapelPhone": "555-0199",
        "meetingTime": "Sunday 9:00 AM"
    })");
}

QJsonObject stake()
{
    return parsed(R"({
        "name": "Provo",
        "unitNumber": "510033",
        "phone": "555-0150",
        "email": "stake@example.org",
        "stakeCenterAddress": "2 Center St",
        "stakeCenterLat": 40.75,
        "stakeCenterLng": -111.75,
        "wardUnitNumbers": ["123456"]
    })");
}

QJsonObject notification()
{
    QJsonObject json = parsed(R"({"method": "text", "notes": "Sent a reminder"})");
    json["timestamp"] = QString("2026-09-27T13:00:00.001Z");
    return json;
}

QJsonObject attempt()
{
    QJsonObject json = parsed(R"({"id": "c1", "method": "visit", "who": "p1", "notes": "All well"})");
    json["timestamp"] = QString(Time);
    return json;
}

QJsonObject task()
{
    QJsonObject json = parsed(R"({
        "id": "k1",
        "category": "Tree removal",
        "description": "Oak across the drive"
    })");
    json["createdAt"] = QString("2026-09-27T12:40:00.002Z");

    QJsonObject assignment = parsed(R"({"personId": "p1", "notes": "Bring the saw"})");
    assignment["notification"] = notification();
    json["assignment"] = assignment;

    QJsonObject resolution = parsed(R"({"notes": "Cleared"})");
    resolution["at"] = QString("2026-09-27T15:30:00.003Z");
    json["resolution"] = resolution;
    return json;
}

QJsonObject familyRecord()
{
    QJsonObject json = parsed(R"({
        "familyId": "f1",
        "displayName": "Reed",
        "address": "1 Oak St",
        "contactStatus": "ok"
    })");
    json["contactAttempts"] = QJsonArray{attempt()};
    json["tasks"] = QJsonArray{task()};
    return json;
}

QJsonObject emergencyResponse()
{
    QJsonObject json = parsed(R"({"name": "Storm", "taskCategories": ["Flooding", "Shelter"]})");
    json["startedAt"] = QString(Time);
    json["familyRecords"] = QJsonArray{familyRecord()};
    return json;
}

QJsonObject document()
{
    const SealingSet sealingSet = SealingSet::of(
        DeviceKeys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey});

    QJsonObject json;
    json["sealingSet"] = sealingSet.toJson();
    json["stakes"] = QJsonArray{stake()};
    json["wards"] = QJsonArray{ward()};
    json["families"] = QJsonArray{family()};
    json["teams"] = QJsonArray{team()};
    json["tags"] = QJsonArray{tag()};
    json["emergencyResources"] = QJsonArray{emergencyAsset()};
    json["eqDistricts"] = QJsonArray{district("d1", "m1")};
    json["eqGroups"] = QJsonArray{group("m1", false)};
    json["rsDistricts"] = QJsonArray{district("d2", "m2")};
    json["rsGroups"] = QJsonArray{group("m2", true)};
    json["wardDirectoryPdfDate"] = QString("2026-08-15");
    json["ministeringPdfDate"] = QString("2026-08-20");
    json["responseData"] = emergencyResponse();
    return json;
}


} // namespace ModelSamples
