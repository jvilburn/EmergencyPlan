#include "AccessRosterTest.h"

#include "AccessRoster.h"
#include "Address.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Document.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "RosterDevice.h"
#include "RosterPerson.h"

#include <QJsonArray>
#include <QtTest>

namespace
{

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::generateDataKey().bytes.left(MemberId::Size);
    return member;
}

RosterDevice newDevice(const QString& name)
{
    return RosterDevice::create(name,
                                DocumentCrypto::generateKeypair().publicKey,
                                DocumentCrypto::generateSigningKeypair().verifyKey);
}

RosterPerson newPerson(const QString& displayName,
                       const QString& deviceName,
                       RosterPerson::Role role)
{
    return RosterPerson::create(newMember(), displayName, newDevice(deviceName), role);
}

} // namespace

void AccessRosterTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Devices -----------------------------------------------------------------

void AccessRosterTest::deviceFingerprintFollowsBothKeys()
{
    const Keypair sealing = DocumentCrypto::generateKeypair();
    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();

    const RosterDevice device =
        RosterDevice::create("Laptop", sealing.publicKey, signing.verifyKey);

    QVERIFY(device.fingerprint()
            == DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey));

    // Renaming a device does not make it a different device.
    RosterDevice renamed = device;
    renamed.setDeviceName("Old laptop");
    QVERIFY(renamed.fingerprint() == device.fingerprint());

    // Changing either key does.
    const RosterDevice otherSealing = RosterDevice::create("Laptop",
                                                           DocumentCrypto::generateKeypair().publicKey,
                                                           signing.verifyKey);
    QVERIFY(!(otherSealing.fingerprint() == device.fingerprint()));

    const RosterDevice otherSigning =
        RosterDevice::create("Laptop",
                             sealing.publicKey,
                             DocumentCrypto::generateSigningKeypair().verifyKey);
    QVERIFY(!(otherSigning.fingerprint() == device.fingerprint()));
}

void AccessRosterTest::deviceRoundTripsThroughJson()
{
    const RosterDevice device = newDevice("Phone");

    const std::optional<RosterDevice> back = RosterDevice::fromJson(device.toJson());
    QVERIFY(back.has_value());
    QVERIFY(back.value() == device);
    QVERIFY(back.value().fingerprint() == device.fingerprint());
}

void AccessRosterTest::deviceFromJsonRefusesMissingOrIllFormedFields()
{
    const RosterDevice device = newDevice("Phone");
    const QJsonObject complete = device.toJson();

    for (const QString& key : complete.keys())
    {
        QJsonObject json = complete;
        json.remove(key);
        QVERIFY(!RosterDevice::fromJson(json).has_value());
    }

    QJsonObject emptyName = complete;
    emptyName["deviceName"] = "";
    QVERIFY(!RosterDevice::fromJson(emptyName).has_value());

    QJsonObject shortKey = complete;
    shortKey["publicKey"] = HexCodec::encode(QByteArray(PublicKey::Size - 1, '\x01'));
    QVERIFY(!RosterDevice::fromJson(shortKey).has_value());

    QJsonObject shortVerify = complete;
    shortVerify["verifyKey"] = HexCodec::encode(QByteArray(VerifyKey::Size - 1, '\x01'));
    QVERIFY(!RosterDevice::fromJson(shortVerify).has_value());

    QJsonObject notHex = complete;
    notHex["publicKey"] = QString(PublicKey::Size * 2, QLatin1Char('z'));
    QVERIFY(!RosterDevice::fromJson(notHex).has_value());
}

void AccessRosterTest::parsersRefuseAnUnknownKey()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Admin);
    QVERIFY(roster.addPerson(alice));

    // Each positive first, so a refusal below is the extra key and nothing else.
    const QJsonObject device = alice.devices().first().toJson();
    const QJsonObject person = alice.toJson();
    const QJsonObject whole = roster.toJson();
    QVERIFY(RosterDevice::fromJson(device).has_value());
    QVERIFY(RosterPerson::fromJson(person).has_value());
    QVERIFY(AccessRoster::fromJson(whole).has_value());

    QJsonObject deviceExtra = device;
    deviceExtra["trusted"] = true;
    QVERIFY(!RosterDevice::fromJson(deviceExtra).has_value());

    QJsonObject personExtra = person;
    personExtra["canRemove"] = true;
    QVERIFY(!RosterPerson::fromJson(personExtra).has_value());

    QJsonObject rosterExtra = whole;
    rosterExtra["owners"] = QJsonArray();
    QVERIFY(!AccessRoster::fromJson(rosterExtra).has_value());

    // And the same key buried a level down refuses the whole roster.
    QJsonObject nested = whole;
    QJsonArray people = nested["people"].toArray();
    QJsonObject first = people.at(0).toObject();
    QJsonArray devices = first["devices"].toArray();
    devices.replace(0, deviceExtra);
    first["devices"] = devices;
    people.replace(0, first);
    nested["people"] = people;
    QVERIFY(!AccessRoster::fromJson(nested).has_value());
}

// --- People ------------------------------------------------------------------

void AccessRosterTest::personAddsAndRemovesDevices()
{
    RosterPerson person = newPerson("Bob Hale", "Laptop", RosterPerson::Role::Member);
    const RosterDevice phone = newDevice("Phone");

    QCOMPARE(person.deviceCount(), 1);
    QVERIFY(person.addDevice(phone));
    QCOMPARE(person.deviceCount(), 2);
    QVERIFY(person.hasDevice(phone.fingerprint()));

    QVERIFY(person.removeDevice(phone.fingerprint()));
    QCOMPARE(person.deviceCount(), 1);
    QVERIFY(!person.hasDevice(phone.fingerprint()));

    // Removing one that is not here is not a removal.
    QVERIFY(!person.removeDevice(phone.fingerprint()));
}

void AccessRosterTest::personRefusesToRemoveItsLastDevice()
{
    RosterPerson person = newPerson("Bob Hale", "Laptop", RosterPerson::Role::Member);
    const Fingerprint only = person.devices().first().fingerprint();

    // A person without devices is a person nobody can reach. Taking the last one
    // away is removing the person, which takes an admin.
    QVERIFY(!person.removeDevice(only));
    QCOMPARE(person.deviceCount(), 1);
}

void AccessRosterTest::personSortsDevicesByName()
{
    RosterPerson person = newPerson("Bob Hale", "Laptop", RosterPerson::Role::Member);
    QVERIFY(person.addDevice(newDevice("Phone")));
    QVERIFY(person.addDevice(newDevice("Desktop")));

    const QList<RosterDevice> devices = person.devices();
    QCOMPARE(devices.size(), qsizetype(3));
    QCOMPARE(devices.at(0).deviceName(), QString("Desktop"));
    QCOMPARE(devices.at(1).deviceName(), QString("Laptop"));
    QCOMPARE(devices.at(2).deviceName(), QString("Phone"));
}

void AccessRosterTest::personFromJsonRefusesADeviceListedTwice()
{
    const RosterPerson person = newPerson("Bob Hale", "Laptop", RosterPerson::Role::Member);

    QJsonObject json = person.toJson();
    QJsonArray devices = json["devices"].toArray();
    devices.append(devices.at(0));
    json["devices"] = devices;

    QVERIFY(!RosterPerson::fromJson(json).has_value());
}

void AccessRosterTest::personRoleRoundTripsThroughJson()
{
    const RosterPerson admin = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Admin);
    const RosterPerson member = newPerson("Bob Hale", "Laptop", RosterPerson::Role::Member);

    QCOMPARE(admin.toJson().value("role").toString(), QString("admin"));
    QCOMPARE(member.toJson().value("role").toString(), QString("member"));

    const std::optional<RosterPerson> adminBack = RosterPerson::fromJson(admin.toJson());
    const std::optional<RosterPerson> memberBack = RosterPerson::fromJson(member.toJson());
    QVERIFY(adminBack.has_value());
    QVERIFY(memberBack.has_value());
    QVERIFY(adminBack.value().isAdmin());
    QVERIFY(!memberBack.value().isAdmin());

    // The role takes part in equality, or a changed role would never be saved.
    RosterPerson demoted = admin;
    demoted.setRole(RosterPerson::Role::Member);
    QVERIFY(demoted != admin);
}

void AccessRosterTest::personFromJsonRefusesAMissingOrUnknownRole()
{
    const QJsonObject complete = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member).toJson();

    QJsonObject missing = complete;
    missing.remove("role");
    QVERIFY(!RosterPerson::fromJson(missing).has_value());

    // A role this build does not know is not one to guess at, and the spelling
    // is exact.
    QJsonObject unknown = complete;
    unknown["role"] = "owner";
    QVERIFY(!RosterPerson::fromJson(unknown).has_value());

    QJsonObject wrongCase = complete;
    wrongCase["role"] = "Admin";
    QVERIFY(!RosterPerson::fromJson(wrongCase).has_value());

    QJsonObject notAString = complete;
    notAString["role"] = 1;
    QVERIFY(!RosterPerson::fromJson(notAString).has_value());
}

// --- Roster ------------------------------------------------------------------

void AccessRosterTest::addsRemovesAndFindsPeople()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);

    QVERIFY(roster.isEmpty());
    QVERIFY(roster.addPerson(alice));
    QCOMPARE(roster.personCount(), 1);
    QVERIFY(roster.contains(alice.member()));
    QVERIFY(roster.person(alice.member()).has_value());

    QVERIFY(roster.removePerson(alice.member()));
    QVERIFY(!roster.contains(alice.member()));
    QVERIFY(!roster.removePerson(alice.member()));
}

void AccessRosterTest::refusesAMemberIdAlreadyPresent()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));

    // A member id is unique within the roster, so an id arriving while its person
    // is still here is a mistake or an attempt to overwrite someone.
    const RosterPerson impostor =
        RosterPerson::create(alice.member(),
                             "Alice Reed",
                             newDevice("Phone"),
                             RosterPerson::Role::Member);
    QVERIFY(!roster.addPerson(impostor));
    QCOMPARE(roster.personCount(), 1);
}

void AccessRosterTest::addingTheSameDeviceTwiceLeavesOne()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));

    const RosterDevice phone = newDevice("Phone");
    QVERIFY(roster.addDevice(alice.member(), phone));
    QVERIFY(roster.addDevice(alice.member(), phone));

    QCOMPARE(roster.person(alice.member()).value().deviceCount(), 2);
}

void AccessRosterTest::refusesADeviceHeldByAnotherPerson()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    const RosterPerson bob = newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));
    QVERIFY(roster.addPerson(bob));

    // One device answers to one person. Otherwise a second machine of Alice's
    // could be presented as Bob, and act with Bob's rights.
    const RosterDevice aliceLaptop = alice.devices().first();
    QVERIFY(!roster.addDevice(bob.member(), aliceLaptop));
    QCOMPARE(roster.person(bob.member()).value().deviceCount(), 1);
}

void AccessRosterTest::refusesADeviceSharingAKeyWithAnotherDevice()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    const RosterPerson bob = newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));
    QVERIFY(roster.addPerson(bob));

    const RosterDevice aliceLaptop = alice.devices().first();

    // Alice's public key with a fresh verify key: a different fingerprint, so the
    // fingerprint rule alone would let it in. The header forbids two entries
    // sharing a key, and every write reseals from the roster, so accepting it
    // would stop the ward from ever publishing again.
    const RosterDevice copiedPublicKey =
        RosterDevice::create("Phone",
                             aliceLaptop.publicKey(),
                             DocumentCrypto::generateSigningKeypair().verifyKey);
    QVERIFY(!(copiedPublicKey.fingerprint() == aliceLaptop.fingerprint()));
    QVERIFY(!roster.addDevice(bob.member(), copiedPublicKey));

    // The same under Alice herself: one person's two devices may not share one
    // either.
    QVERIFY(!roster.addDevice(alice.member(), copiedPublicKey));

    const RosterDevice copiedVerifyKey =
        RosterDevice::create("Phone",
                             DocumentCrypto::generateKeypair().publicKey,
                             aliceLaptop.verifyKey());
    QVERIFY(!roster.addDevice(bob.member(), copiedVerifyKey));

    // And through a new person, which is how a granted access request arrives.
    const RosterPerson carol =
        RosterPerson::create(newMember(), "Carol West", copiedPublicKey, RosterPerson::Role::Member);
    QVERIFY(!roster.addPerson(carol));

    QCOMPARE(roster.personCount(), 2);
    QCOMPARE(roster.person(bob.member()).value().deviceCount(), 1);
}

void AccessRosterTest::findsAPersonAndDeviceByFingerprint()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));

    const RosterDevice phone = newDevice("Phone");
    QVERIFY(roster.addDevice(alice.member(), phone));

    const std::optional<RosterPerson> owner = roster.personFor(phone.fingerprint());
    QVERIFY(owner.has_value());
    QVERIFY(owner.value().member() == alice.member());

    const std::optional<RosterDevice> found = roster.deviceFor(phone.fingerprint());
    QVERIFY(found.has_value());
    QVERIFY(found.value() == phone);

    QVERIFY(!roster.personFor(newDevice("Stranger").fingerprint()).has_value());
    QVERIFY(!roster.deviceFor(Fingerprint()).has_value());
}

void AccessRosterTest::removingAPersonsLastDeviceIsRefused()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));

    const Fingerprint only = alice.devices().first().fingerprint();
    QVERIFY(!roster.removeDevice(alice.member(), only));
    QVERIFY(roster.contains(alice.member()));
}

void AccessRosterTest::twoPeopleSharingADisplayNameAreDistinctAndSortStably()
{
    AccessRoster roster;
    const RosterPerson first = newPerson("Chris Ford", "Laptop", RosterPerson::Role::Member);
    const RosterPerson second = newPerson("Chris Ford", "Phone", RosterPerson::Role::Member);
    const RosterPerson third = newPerson("Alice Reed", "Desktop", RosterPerson::Role::Member);

    QVERIFY(roster.addPerson(first));
    QVERIFY(roster.addPerson(second));
    QVERIFY(roster.addPerson(third));
    QCOMPARE(roster.personCount(), 3);

    const QList<RosterPerson> people = roster.people();
    QCOMPARE(people.at(0).displayName(), QString("Alice Reed"));
    QCOMPARE(people.at(1).displayName(), QString("Chris Ford"));
    QCOMPARE(people.at(2).displayName(), QString("Chris Ford"));

    // The tie is broken by member id, so the order is the same on every device
    // rather than following insertion or hash order.
    QVERIFY(people.at(1).member().toHex() < people.at(2).member().toHex());
    QVERIFY(roster.people() == people);
}

void AccessRosterTest::peopleAndDevicesSortIgnoringCase()
{
    AccessRoster roster;
    const RosterPerson young = newPerson("Young", "Laptop", RosterPerson::Role::Member);
    const RosterPerson deGroot = newPerson("de Groot", "Laptop", RosterPerson::Role::Member);
    const RosterPerson adams = newPerson("Adams", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(young));
    QVERIFY(roster.addPerson(deGroot));
    QVERIFY(roster.addPerson(adams));

    // A byte-wise comparison would put every capitalised name ahead of
    // "de Groot".
    const QList<RosterPerson> people = roster.people();
    QCOMPARE(people.at(0).displayName(), QString("Adams"));
    QCOMPARE(people.at(1).displayName(), QString("de Groot"));
    QCOMPARE(people.at(2).displayName(), QString("Young"));

    RosterPerson person = newPerson("Adams", "Surface", RosterPerson::Role::Member);
    QVERIFY(person.addDevice(newDevice("iPhone")));
    QVERIFY(person.addDevice(newDevice("Desktop")));

    const QList<RosterDevice> devices = person.devices();
    QCOMPARE(devices.at(0).deviceName(), QString("Desktop"));
    QCOMPARE(devices.at(1).deviceName(), QString("iPhone"));
    QCOMPARE(devices.at(2).deviceName(), QString("Surface"));
}

void AccessRosterTest::setRoleChangesWhoIsListedAsAdmin()
{
    AccessRoster roster;
    QList<RosterPerson> people;
    for (const char* name : {"Alice Reed", "Bob Hale", "Carol West", "Dave Kent"})
    {
        people.append(newPerson(name, "Laptop", RosterPerson::Role::Admin));
        QVERIFY(roster.addPerson(people.last()));
    }

    // Ascending hex, whatever the insertion or hash order, so it compares
    // directly against a header's list.
    const QList<MemberId> admins = roster.admins();
    QCOMPARE(admins.size(), 4);
    for (qsizetype index = 1; index < admins.size(); ++index)
    {
        QVERIFY(admins.at(index - 1).toHex() < admins.at(index).toHex());
    }

    const MemberId bob = people.at(1).member();
    QVERIFY(roster.setRole(bob, RosterPerson::Role::Member));
    QCOMPARE(roster.admins().size(), 3);
    QVERIFY(!roster.admins().contains(bob));
    QVERIFY(!roster.person(bob).value().isAdmin());

    QVERIFY(roster.setRole(bob, RosterPerson::Role::Admin));
    QVERIFY(roster.admins() == admins);

    // Someone not in the roster has no role to set, and nothing else changes.
    QVERIFY(!roster.setRole(newMember(), RosterPerson::Role::Admin));
    QVERIFY(roster.admins() == admins);
    QCOMPARE(roster.personCount(), 4);
}

void AccessRosterTest::rosterRoundTripsThroughJson()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Admin);
    const RosterPerson bob = newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));
    QVERIFY(roster.addPerson(bob));
    QVERIFY(roster.addDevice(alice.member(), newDevice("Phone")));

    const std::optional<AccessRoster> back = AccessRoster::fromJson(roster.toJson());
    QVERIFY(back.has_value());
    QVERIFY(back.value() == roster);
    QCOMPARE(back.value().personCount(), 2);
    QCOMPARE(back.value().person(alice.member()).value().deviceCount(), 2);
    QVERIFY(back.value().admins() == QList<MemberId>{alice.member()});
}

void AccessRosterTest::rosterFromJsonRefusesAMalformedPerson()
{
    AccessRoster roster;
    QVERIFY(roster.addPerson(newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member)));

    QJsonObject json = roster.toJson();
    QJsonArray people = json["people"].toArray();

    QJsonObject person = people.at(0).toObject();
    person.remove("member");
    people.replace(0, person);
    json["people"] = people;

    // The whole roster fails, rather than coming back one person short.
    QVERIFY(!AccessRoster::fromJson(json).has_value());

    QJsonObject notAnObject = roster.toJson();
    QJsonArray broken;
    broken.append(QJsonValue("a person"));
    notAnObject["people"] = broken;
    QVERIFY(!AccessRoster::fromJson(notAnObject).has_value());

    QJsonObject noArray = roster.toJson();
    noArray["people"] = 3;
    QVERIFY(!AccessRoster::fromJson(noArray).has_value());
}

void AccessRosterTest::rosterFromJsonRefusesADeviceUnderTwoPeople()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    const RosterPerson bob = newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));
    QVERIFY(roster.addPerson(bob));

    QJsonObject json = roster.toJson();
    QJsonArray people = json["people"].toArray();

    // Give Bob a copy of Alice's device. addDevice would refuse this, so the
    // reader has to refuse it too or the rule holds only in memory.
    QJsonObject bobJson = people.at(1).toObject();
    QJsonArray bobDevices = bobJson["devices"].toArray();
    bobDevices.append(people.at(0).toObject()["devices"].toArray().at(0));
    bobJson["devices"] = bobDevices;
    people.replace(1, bobJson);
    json["people"] = people;

    QVERIFY(!AccessRoster::fromJson(json).has_value());
}

void AccessRosterTest::rosterFromJsonRefusesDevicesSharingAKey()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    const RosterPerson bob = newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));
    QVERIFY(roster.addPerson(bob));

    QJsonObject json = roster.toJson();
    QJsonArray people = json["people"].toArray();

    // Written by hand, since addDevice would refuse it: Bob's device carrying
    // Alice's public key. The reader must refuse it too, or the rule holds only
    // for rosters this app built.
    const QString alicePublicKey =
        people.at(0).toObject()["devices"].toArray().at(0).toObject()["publicKey"].toString();

    QJsonObject bobJson = people.at(1).toObject();
    QJsonArray bobDevices = bobJson["devices"].toArray();
    QJsonObject bobDevice = bobDevices.at(0).toObject();
    bobDevice["publicKey"] = alicePublicKey;
    bobDevices.replace(0, bobDevice);
    bobJson["devices"] = bobDevices;
    people.replace(1, bobJson);
    json["people"] = people;

    QVERIFY(!AccessRoster::fromJson(json).has_value());
}

// --- Document ----------------------------------------------------------------

void AccessRosterTest::documentCarriesTheRoster()
{
    AccessRoster roster;
    const RosterPerson alice = newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member);
    QVERIFY(roster.addPerson(alice));

    Document document = Document::empty();
    document.setAccessRoster(roster);

    QCOMPARE(document.accessRoster().personCount(), 1);
    QVERIFY(document.toJson().contains("accessRoster"));
}

void AccessRosterTest::documentParseAcceptsWhatToJsonWrote()
{
    AccessRoster roster;
    QVERIFY(roster.addPerson(newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member)));
    QVERIFY(roster.addPerson(newPerson("Bob Hale", "Desktop", RosterPerson::Role::Member)));

    Document document = Document::empty();
    document.setAccessRoster(roster);
    const Family family = Family::create(40.0, -111.0, Address(), {});
    document.addFamily(family);
    EmergencyResponse response = EmergencyResponse::create("Storm");
    response.setStartedAt(QDateTime::fromString("2026-09-27T12:00:00Z", Qt::ISODate));
    response.addFamilyRecord(FamilyResponseRecord::create(family.id(), "Adams", "1 Oak St"));
    document.setEmergencyResponse(response);

    // The whole document, not just the roster: a parse that dropped the family or
    // the emergency response would otherwise pass.
    const std::optional<Document> out = Document::parse(document.toJson());
    QVERIFY(out.has_value());
    QVERIFY(out.value() == document);
}

void AccessRosterTest::documentParseRefusesAMalformedRoster()
{
    AccessRoster roster;
    QVERIFY(roster.addPerson(newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member)));

    Document document = Document::empty();
    document.setAccessRoster(roster);

    QJsonObject json = document.toJson();
    QJsonObject rosterJson = json["accessRoster"].toObject();
    QJsonArray people = rosterJson["people"].toArray();

    QJsonObject person = people.at(0).toObject();
    QJsonArray devices = person["devices"].toArray();
    QJsonObject device = devices.at(0).toObject();
    device["publicKey"] = HexCodec::encode(QByteArray(PublicKey::Size - 1, '\x01'));
    devices.replace(0, device);
    person["devices"] = devices;
    people.replace(0, person);
    rosterJson["people"] = people;
    json["accessRoster"] = rosterJson;

    QVERIFY(!Document::parse(json).has_value());

    json.remove("accessRoster");
    QVERIFY(!Document::parse(json).has_value());
}

void AccessRosterTest::documentParseRefusesAnUnknownTopLevelKey()
{
    AccessRoster roster;
    QVERIFY(roster.addPerson(newPerson("Alice Reed", "Laptop", RosterPerson::Role::Admin)));
    Document document = Document::empty();
    document.setAccessRoster(roster);

    QVERIFY(Document::parse(document.toJson()).has_value());

    QJsonObject json = document.toJson();
    json["sharedWith"] = QJsonArray();
    QVERIFY(!Document::parse(json).has_value());
}

void AccessRosterTest::documentParseRefusesAnEmptyRoster()
{
    // Every real document has at least the person who created it.
    Document document = Document::empty();
    QVERIFY(document.accessRoster().isEmpty());

    QVERIFY(!Document::parse(document.toJson()).has_value());
}

void AccessRosterTest::rosterTakesPartInDocumentEquality()
{
    // A copy, not a second empty(): each empty document seeds its default
    // emergency assets with fresh ids.
    Document first = Document::empty();
    Document second = first;
    QVERIFY(first == second);

    AccessRoster roster;
    QVERIFY(roster.addPerson(newPerson("Alice Reed", "Laptop", RosterPerson::Role::Member)));
    first.setAccessRoster(roster);

    QVERIFY(!(first == second));
    QVERIFY(first != second);

    second.setAccessRoster(roster);
    QVERIFY(first == second);
}
