#include "SealingSetTest.h"

#include "DocumentCrypto.h"
#include "SealingSet.h"

#include <QJsonArray>
#include <QtTest>

namespace
{

DeviceKeys newKeys()
{
    return DeviceKeys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
}

NamedDevice newDevice(const QString& name)
{
    return NamedDevice{name, newKeys()};
}

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    return member;
}

Invite newInvite(Invite::For purpose, const MemberId& person, Role role, const QString& label)
{
    Invite invite;
    invite.purpose = purpose;
    invite.maker = person;
    invite.role = role;
    invite.label = label;
    invite.keys = newKeys();
    return invite;
}

QString hexOf(const DeviceKeys& keys)
{
    return DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey).toHex();
}

QSet<QString> recipientsOf(const SealingSet& set)
{
    QSet<QString> fingerprints;
    for (const DeviceKeys& keys : set.recipients())
    {
        fingerprints.insert(hexOf(keys));
    }
    return fingerprints;
}

/// Alice, an admin on her laptop, with Bob joined as a member on his phone.
struct Pair
{
    MemberId alice = newMember();
    MemberId bob = newMember();
    NamedDevice laptop = newDevice("Laptop");
    NamedDevice phone = newDevice("Phone");
    SealingSet set;

    Pair()
    {
        set = SealingSet::of(alice, "Alice", laptop);
        const Invite forBob = newInvite(Invite::For::User, alice, Role::Member, "for Bob");
        set.addInvite(forBob);
        set.join(forBob.fingerprint(), phone, bob, "Bob");
    }
};

} // namespace

void SealingSetTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- The roster --------------------------------------------------------------------

void SealingSetTest::ofMakesItsMakerAnAdminOnOneDevice()
{
    const MemberId alice = newMember();
    const NamedDevice laptop = newDevice("Laptop");
    const SealingSet set = SealingSet::of(alice, "Alice", laptop);

    QCOMPARE(set.people().size(), 1);
    const Holder maker = set.people().first();
    QVERIFY(maker.id == alice);
    QCOMPARE(maker.name, QString("Alice"));
    QVERIFY(maker.role == Role::Admin);
    QCOMPARE(maker.devices, QList<NamedDevice>{laptop});
    QCOMPARE(set.adminCount(), 1);
    QCOMPARE(set.recipients(), QList<DeviceKeys>{laptop.keys});
    QVERIFY(set.invites().isEmpty());
    QVERIFY(set.personOf(laptop.fingerprint()) == maker);
}

void SealingSetTest::anInviteIsARecipientUntilItIsJoined()
{
    const MemberId alice = newMember();
    SealingSet set = SealingSet::of(alice, "Alice", newDevice("Laptop"));
    const Invite forBob = newInvite(Invite::For::User, alice, Role::Member, "for Bob");

    QVERIFY(set.addInvite(forBob));
    QCOMPARE(set.recipients().size(), 2);
    QVERIFY(set.invite(forBob.fingerprint()).has_value());
    QVERIFY(set.invite(forBob.fingerprint()) == forBob);
    QCOMPARE(set.invites(), QList<Invite>{forBob});

    // An invite is nobody yet: no person holds it, and it is not a device.
    QVERIFY(!set.personOf(forBob.fingerprint()).has_value());
    QVERIFY(!set.device(forBob.fingerprint()).has_value());
    QCOMPARE(set.people().size(), 1);

    // Withdrawn, it is no recipient.
    QVERIFY(set.removeInvite(forBob.fingerprint()));
    QVERIFY(!set.invite(forBob.fingerprint()).has_value());
    QCOMPARE(set.recipients().size(), 1);
    QVERIFY(!set.removeInvite(forBob.fingerprint()));
}

void SealingSetTest::aUserInviteJoinsAsANewPerson()
{
    const MemberId alice = newMember();
    const MemberId bob = newMember();
    const NamedDevice phone = newDevice("Phone");
    SealingSet set = SealingSet::of(alice, "Alice", newDevice("Laptop"));
    const Invite forBob = newInvite(Invite::For::User, alice, Role::Admin, "for Bob");
    QVERIFY(set.addInvite(forBob));

    QVERIFY(set.join(forBob.fingerprint(), phone, bob, "  Bob Jones "));

    QCOMPARE(set.people().size(), 2);
    const std::optional<Holder> joined = set.person(bob);
    QVERIFY(joined.has_value());
    QCOMPARE(joined->name, QString("Bob Jones"));
    QVERIFY(joined->role == Role::Admin);
    QCOMPARE(joined->devices, QList<NamedDevice>{phone});
    QCOMPARE(set.adminCount(), 2);

    // The invite's place is taken: it is gone, and the phone is sealed to instead.
    QVERIFY(set.invites().isEmpty());
    QVERIFY(!set.invite(forBob.fingerprint()).has_value());
    QVERIFY(set.device(phone.fingerprint()).has_value());
    QCOMPARE(set.recipients().size(), 2);
}

void SealingSetTest::aDeviceInviteJoinsAsAnotherDeviceOfItsPerson()
{
    const MemberId alice = newMember();
    const NamedDevice laptop = newDevice("Laptop");
    const NamedDevice phone = newDevice("Phone");
    SealingSet set = SealingSet::of(alice, "Alice", laptop);
    // The role on a Device invite is not the joiner's: the device takes its person's.
    const Invite forPhone = newInvite(Invite::For::Device, alice, Role::Member, "for Phone");
    QVERIFY(set.addInvite(forPhone));

    // The joiner's own id and name are not used for a device invite.
    QVERIFY(set.join(forPhone.fingerprint(), phone, newMember(), "Somebody Else"));

    QCOMPARE(set.people().size(), 1);
    const Holder person = set.person(alice).value();
    QCOMPARE(person.name, QString("Alice"));
    QVERIFY(person.role == Role::Admin);
    QCOMPARE(person.devices.size(), 2);
    QVERIFY(person.devices.contains(laptop));
    QVERIFY(person.devices.contains(phone));
    QVERIFY(set.personOf(phone.fingerprint()) == person);
    QVERIFY(set.invites().isEmpty());
    QCOMPARE(set.recipients().size(), 2);
}

void SealingSetTest::joinRefusesWhatWouldLeaveTheSetInconsistent()
{
    Pair pair;
    const Invite forCarol = newInvite(Invite::For::User, pair.alice, Role::Member, "for Carol");
    QVERIFY(pair.set.addInvite(forCarol));
    const SealingSet before = pair.set;

    // An invite the set does not hold.
    QVERIFY(!pair.set.join(newInvite(Invite::For::User, pair.alice, Role::Member, "never made").fingerprint(),
                           newDevice("Tablet"),
                           newMember(),
                           "Carol"));
    // A device sharing a key with one already here.
    QVERIFY(!pair.set.join(forCarol.fingerprint(),
                           NamedDevice{"Tablet", DeviceKeys{pair.phone.keys.publicKey, newKeys().verifyKey}},
                           newMember(),
                           "Carol"));
    // A user invite joined without a name, without a person id, or as a person already here.
    QVERIFY(!pair.set.join(forCarol.fingerprint(), newDevice("Tablet"), newMember(), "   "));
    QVERIFY(!pair.set.join(forCarol.fingerprint(), newDevice("Tablet"), MemberId(), "Carol"));
    QVERIFY(!pair.set.join(forCarol.fingerprint(), newDevice("Tablet"), pair.bob, "Bob"));
    QVERIFY(pair.set == before);

    // Anchor: the same invite joins with a device and person of its own, once.
    const MemberId carol = newMember();
    QVERIFY(pair.set.join(forCarol.fingerprint(), newDevice("Tablet"), carol, "Carol"));
    QCOMPARE(pair.set.person(carol)->name, QString("Carol"));
    QVERIFY(!pair.set.join(forCarol.fingerprint(), newDevice("Desktop"), newMember(), "Dave"));
    QCOMPARE(pair.set.people().size(), 3);
}

void SealingSetTest::addInviteRefusesAnUnknownPersonOrASharedKey()
{
    Pair pair;
    const SealingSet before = pair.set;

    QVERIFY(!pair.set.addInvite(newInvite(Invite::For::Device, newMember(), Role::Member, "for nobody")));
    Invite sharing = newInvite(Invite::For::User, pair.alice, Role::Member, "sharing");
    sharing.keys.verifyKey = pair.laptop.keys.verifyKey;
    QVERIFY(!pair.set.addInvite(sharing));
    Invite invalid = newInvite(Invite::For::User, pair.alice, Role::Member, "invalid");
    invalid.keys = DeviceKeys();
    QVERIFY(!pair.set.addInvite(invalid));
    QVERIFY(pair.set == before);

    // Anchor: one by a person here, with keys of its own.
    QVERIFY(pair.set.addInvite(newInvite(Invite::For::Device, pair.bob, Role::Member, "for Bob's tablet")));
    QCOMPARE(pair.set.invites().size(), 1);
}

void SealingSetTest::removeDeviceRefusesAPersonsLastDevice()
{
    const MemberId alice = newMember();
    const NamedDevice laptop = newDevice("Laptop");
    const NamedDevice phone = newDevice("Phone");
    SealingSet set = SealingSet::of(alice, "Alice", laptop);
    const Invite forPhone = newInvite(Invite::For::Device, alice, Role::Member, "for Phone");
    QVERIFY(set.addInvite(forPhone));
    QVERIFY(set.join(forPhone.fingerprint(), phone, alice, "Alice"));
    QCOMPARE(set.person(alice)->devices.size(), 2);

    QVERIFY(set.removeDevice(phone.fingerprint()));
    QCOMPARE(set.person(alice)->devices, QList<NamedDevice>{laptop});
    QVERIFY(!set.device(phone.fingerprint()).has_value());

    // The laptop is now her last: taking it out is removePerson's to do.
    QVERIFY(!set.removeDevice(laptop.fingerprint()));
    QVERIFY(!set.removeDevice(newDevice("Unknown").fingerprint()));
    QCOMPARE(set.person(alice)->devices, QList<NamedDevice>{laptop});
}

void SealingSetTest::removePersonRefusesTheLastPersonAndTakesTheirInvites()
{
    Pair pair;
    const Invite bobsTablet = newInvite(Invite::For::Device, pair.bob, Role::Member, "for Bob's tablet");
    const Invite fromBob = newInvite(Invite::For::User, pair.bob, Role::Member, "for Carol");
    const Invite fromAlice = newInvite(Invite::For::User, pair.alice, Role::Member, "for Dave");
    QVERIFY(pair.set.addInvite(bobsTablet));
    QVERIFY(pair.set.addInvite(fromBob));
    QVERIFY(pair.set.addInvite(fromAlice));
    QCOMPARE(pair.set.recipients().size(), 5);

    QVERIFY(pair.set.removePerson(pair.bob));
    QVERIFY(!pair.set.person(pair.bob).has_value());
    QCOMPARE(pair.set.invites(), QList<Invite>{fromAlice});
    QCOMPARE(recipientsOf(pair.set), (QSet<QString>{hexOf(pair.laptop.keys), hexOf(fromAlice.keys)}));

    QVERIFY(!pair.set.removePerson(pair.bob));
    QVERIFY(!pair.set.removePerson(pair.alice));
    QCOMPARE(pair.set.people().size(), 1);
    QVERIFY(pair.set.person(pair.alice).has_value());
}

void SealingSetTest::setRoleChangesOnlyThatPerson()
{
    Pair pair;
    QCOMPARE(pair.set.adminCount(), 1);

    QVERIFY(pair.set.setRole(pair.bob, Role::Admin));
    QVERIFY(pair.set.person(pair.bob)->role == Role::Admin);
    QVERIFY(pair.set.person(pair.alice)->role == Role::Admin);
    QCOMPARE(pair.set.adminCount(), 2);

    QVERIFY(pair.set.setRole(pair.alice, Role::Member));
    QVERIFY(pair.set.person(pair.alice)->role == Role::Member);
    QVERIFY(pair.set.person(pair.bob)->role == Role::Admin);
    QCOMPARE(pair.set.adminCount(), 1);

    QVERIFY(!pair.set.setRole(newMember(), Role::Admin));
    QCOMPARE(pair.set.adminCount(), 1);
}

void SealingSetTest::everythingComesInASteadyOrder()
{
    const MemberId maker = newMember();
    SealingSet set = SealingSet::of(maker, "Maker", newDevice("First"));
    for (int i = 0; i < 4; ++i)
    {
        const Invite person = newInvite(Invite::For::User, maker, Role::Member, "person");
        QVERIFY(set.addInvite(person));
        QVERIFY(set.join(person.fingerprint(), newDevice("Device"), newMember(), QString("Person %1").arg(i)));
        const Invite device = newInvite(Invite::For::Device, maker, Role::Member, "device");
        QVERIFY(set.addInvite(device));
        QVERIFY(set.join(device.fingerprint(), newDevice("Another"), maker, "Maker"));
        QVERIFY(set.addInvite(newInvite(Invite::For::User, maker, Role::Member, "open")));
    }

    const QList<Holder> people = set.people();
    QCOMPARE(people.size(), 5);
    for (qsizetype i = 1; i < people.size(); ++i)
    {
        QVERIFY(people.at(i - 1).id.toHex() < people.at(i).id.toHex());
    }
    const QList<NamedDevice> devices = set.person(maker)->devices;
    QCOMPARE(devices.size(), 5);
    for (qsizetype i = 1; i < devices.size(); ++i)
    {
        QVERIFY(devices.at(i - 1).fingerprint().toHex() < devices.at(i).fingerprint().toHex());
    }
    const QList<Invite> invites = set.invites();
    QCOMPARE(invites.size(), 4);
    for (qsizetype i = 1; i < invites.size(); ++i)
    {
        QVERIFY(invites.at(i - 1).fingerprint().toHex() < invites.at(i).fingerprint().toHex());
    }
    const QList<DeviceKeys> recipients = set.recipients();
    QCOMPARE(recipients.size(), 13);
    for (qsizetype i = 1; i < recipients.size(); ++i)
    {
        QVERIFY(hexOf(recipients.at(i - 1)) < hexOf(recipients.at(i)));
    }
}

// --- The rule ----------------------------------------------------------------------

void SealingSetTest::anAdditionKeepsTheKey()
{
    Pair pair;
    const QSet<QString> previous = recipientsOf(pair.set);
    QVERIFY(!pair.set.rotatesFrom(previous));

    // An invite made, a role changed: nobody the previous version sealed to is lost.
    QVERIFY(pair.set.addInvite(newInvite(Invite::For::User, pair.alice, Role::Member, "for Carol")));
    QVERIFY(!pair.set.rotatesFrom(previous));
    QVERIFY(pair.set.setRole(pair.bob, Role::Admin));
    QVERIFY(!pair.set.rotatesFrom(previous));

    // Anchor: the same set judged against a version that also sealed to someone else.
    QSet<QString> withStranger = previous;
    withStranger.insert(hexOf(newKeys()));
    QVERIFY(pair.set.rotatesFrom(withStranger));
}

void SealingSetTest::aRemovalRotates()
{
    Pair pair;
    const Invite forCarol = newInvite(Invite::For::User, pair.alice, Role::Member, "for Carol");
    QVERIFY(pair.set.addInvite(forCarol));
    const QSet<QString> previous = recipientsOf(pair.set);
    QCOMPARE(previous.size(), 3);
    QVERIFY(!pair.set.rotatesFrom(previous));

    SealingSet withoutBob = pair.set;
    QVERIFY(withoutBob.removePerson(pair.bob));
    QVERIFY(withoutBob.rotatesFrom(previous));

    SealingSet withdrawn = pair.set;
    QVERIFY(withdrawn.removeInvite(forCarol.fingerprint()));
    QVERIFY(withdrawn.rotatesFrom(previous));

    // A join takes the invite out as it puts the joiner in, so it rotates too.
    SealingSet joined = pair.set;
    QVERIFY(joined.join(forCarol.fingerprint(), newDevice("Tablet"), newMember(), "Carol"));
    QCOMPARE(joined.recipients().size(), 3);
    QVERIFY(joined.rotatesFrom(previous));
}

void SealingSetTest::formerKeysCarryOver()
{
    Pair pair;
    const DataKey first = DocumentCrypto::generateDataKey();
    const DataKey second = DocumentCrypto::generateDataKey();

    SealingSet set = pair.set;
    QVERIFY(!set.descendsFrom(first));
    set.addFormerKey(first);
    set.addFormerKey(second);
    QVERIFY(set.descendsFrom(first));
    QVERIFY(set.descendsFrom(second));
    QVERIFY(!set.descendsFrom(DocumentCrypto::generateDataKey()));

    // Through roster edits, and through JSON.
    QVERIFY(set.removePerson(pair.bob));
    QVERIFY(set.descendsFrom(first));
    QVERIFY(SealingSet::fromJson(set.toJson()).value().descendsFrom(second));

    // Equality covers them: the same roster with a different history differs.
    SealingSet sameRoster = pair.set;
    QVERIFY(sameRoster == pair.set);
    sameRoster.addFormerKey(first);
    QVERIFY(sameRoster != pair.set);
}

// --- Merging -----------------------------------------------------------------------

void SealingSetTest::mergeTakesEachEntryFromTheSideThatChangedIt()
{
    // Base: an open invite. Theirs: Carol joined with it. Mine: a second invite,
    // made before that join was seen. Both carry: the join, and the new invite.
    Pair pair;
    const Invite forCarol = newInvite(Invite::For::User, pair.alice, Role::Member, "for Carol");
    QVERIFY(pair.set.addInvite(forCarol));
    const SealingSet base = pair.set;
    SealingSet theirs = base;
    QVERIFY(theirs.join(forCarol.fingerprint(), newDevice("Tablet"), newMember(), "Carol"));
    const Invite forDave = newInvite(Invite::For::User, pair.alice, Role::Member, "for Dave");
    SealingSet mine = base;
    QVERIFY(mine.addInvite(forDave));
    const SealingSet merged = SealingSet::merge(base, mine, theirs);
    QCOMPARE(merged.invites(), QList<Invite>{forDave});
    QCOMPARE(merged.people().size(), 3);

    // A device invite joined there changes Alice's own entry; her new invite here
    // still carries.
    const Invite forPhone = newInvite(Invite::For::Device, pair.alice, Role::Member, "for Phone");
    SealingSet phoneInvited = base;
    QVERIFY(phoneInvited.addInvite(forPhone));
    SealingSet phoneJoined = phoneInvited;
    QVERIFY(phoneJoined.join(forPhone.fingerprint(), newDevice("Phone"), pair.alice, "Alice"));
    SealingSet phoneAndInvite = phoneInvited;
    QVERIFY(phoneAndInvite.addInvite(forDave));
    const SealingSet crossed = SealingSet::merge(phoneInvited, phoneAndInvite, phoneJoined);
    QCOMPARE(crossed.person(pair.alice)->devices.size(), 2);
    QCOMPARE(crossed.invites().size(), 2);
    QVERIFY(!crossed.invite(forPhone.fingerprint()).has_value());

    // The former keys of both sides, theirs first.
    const DataKey mineRotated = DocumentCrypto::generateDataKey();
    const DataKey theirsRotated = DocumentCrypto::generateDataKey();
    SealingSet mineKeys = base;
    mineKeys.addFormerKey(mineRotated);
    SealingSet theirsKeys = base;
    theirsKeys.addFormerKey(theirsRotated);
    const SealingSet keys = SealingSet::merge(base, mineKeys, theirsKeys);
    QVERIFY(keys.descendsFrom(mineRotated));
    QVERIFY(keys.descendsFrom(theirsRotated));
    const QJsonArray theirsThenMine{theirsKeys.toJson()["formerKeys"].toArray().first(),
                                    mineKeys.toJson()["formerKeys"].toArray().first()};
    QCOMPARE(keys.toJson()["formerKeys"].toArray(), theirsThenMine);
}

void SealingSetTest::theSameEntryChangedOnBothSidesIsTheirs()
{
    Pair pair;
    const SealingSet base = pair.set;
    SealingSet bobAdmin = base;
    QVERIFY(bobAdmin.setRole(pair.bob, Role::Admin));
    SealingSet bobGone = base;
    QVERIFY(bobGone.removePerson(pair.bob));

    // Promoted here, removed there, and the other way round: theirs each time.
    QVERIFY(!SealingSet::merge(base, bobAdmin, bobGone).person(pair.bob).has_value());
    QVERIFY(SealingSet::merge(base, bobGone, bobAdmin).person(pair.bob)->role == Role::Admin);

    // A removed person's invites go with them, whichever side made the invite and
    // whichever removed him.
    SealingSet bobsInvite = base;
    QVERIFY(bobsInvite.addInvite(newInvite(Invite::For::User, pair.bob, Role::Member, "for Erin")));
    QVERIFY(SealingSet::merge(base, bobsInvite, bobGone) == bobGone);
    QVERIFY(SealingSet::merge(base, bobGone, bobsInvite) == bobGone);

    // Anchor: the same change on both sides is that change.
    QVERIFY(SealingSet::merge(base, bobAdmin, bobAdmin).person(pair.bob)->role == Role::Admin);
}

void SealingSetTest::aMergeThatLeavesNobodyIsTheirsWhole()
{
    // Each side removes the other: entry by entry, nobody is left, so theirs stands.
    Pair pair;
    const SealingSet base = pair.set;
    SealingSet bobGone = base;
    QVERIFY(bobGone.removePerson(pair.bob));
    SealingSet aliceGone = base;
    QVERIFY(aliceGone.removePerson(pair.alice));
    QVERIFY(SealingSet::merge(base, bobGone, aliceGone) == aliceGone);
    QVERIFY(SealingSet::merge(base, aliceGone, bobGone) == bobGone);
}

// --- JSON --------------------------------------------------------------------------

void SealingSetTest::roundTripsThroughJson()
{
    Pair pair;
    const Invite device = newInvite(Invite::For::Device, pair.bob, Role::Member, "for Bob's tablet");
    const Invite user = newInvite(Invite::For::User, pair.alice, Role::Admin, "for Carol");
    const Invite forDesktop = newInvite(Invite::For::Device, pair.alice, Role::Member, "for Desktop");
    QVERIFY(pair.set.addInvite(device));
    QVERIFY(pair.set.addInvite(user));
    QVERIFY(pair.set.addInvite(forDesktop));
    QVERIFY(pair.set.join(forDesktop.fingerprint(), newDevice("Desktop"), pair.alice, "Alice"));
    pair.set.addFormerKey(DocumentCrypto::generateDataKey());

    const QJsonObject json = pair.set.toJson();
    QCOMPARE(json["people"].toArray().size(), 2);
    QCOMPARE(json["invites"].toArray().size(), 2);
    QCOMPARE(json["formerKeys"].toArray().size(), 1);

    const std::optional<SealingSet> read = SealingSet::fromJson(json);
    QVERIFY(read.has_value());
    QVERIFY(read.value() == pair.set);
    QCOMPARE(read->person(pair.alice)->devices.size(), 2);
    QCOMPARE(read->person(pair.bob)->name, QString("Bob"));
    QVERIFY(read->person(pair.bob)->role == Role::Member);
    QVERIFY(read->invite(device.fingerprint()) == device);
    QVERIFY(read->invite(user.fingerprint()) == user);
    QCOMPARE(read->toJson(), json);
}

void SealingSetTest::fromJsonRefusesAnythingIllFormed()
{
    Pair pair;
    QVERIFY(pair.set.addInvite(newInvite(Invite::For::User, pair.alice, Role::Member, "for Carol")));
    const QJsonObject good = pair.set.toJson();
    QVERIFY(SealingSet::fromJson(good).has_value());

    const QJsonArray people = good["people"].toArray();
    const QJsonArray invites = good["invites"].toArray();
    const QJsonObject alice = people.at(0).toObject()["id"].toString() == pair.alice.toHex() ? people.at(0).toObject()
                                                                                           : people.at(1).toObject();
    QCOMPARE(alice["name"].toString(), QString("Alice"));

    QList<QJsonObject> bad;

    QJsonObject nobody = good;
    nobody["people"] = QJsonArray();
    nobody["invites"] = QJsonArray();
    bad.append(nobody);

    QJsonObject noDevice = good;
    QJsonObject deviceless = alice;
    deviceless["devices"] = QJsonArray();
    noDevice["people"] = QJsonArray{deviceless};
    bad.append(noDevice);

    // The same id twice, on different devices.
    QJsonObject twice = good;
    const QJsonObject aliceAgain =
        SealingSet::of(pair.alice, "Alice", newDevice("Tablet")).toJson()["people"].toArray().first().toObject();
    twice["people"] = QJsonArray{alice, aliceAgain};
    bad.append(twice);

    // One person, one device key twice.
    QJsonObject sameKeyTwice = good;
    QJsonObject doubled = alice;
    QJsonArray twoDevices = alice["devices"].toArray();
    twoDevices.append(twoDevices.first());
    doubled["devices"] = twoDevices;
    sameKeyTwice["people"] = QJsonArray{doubled};
    bad.append(sameKeyTwice);

    // Two people, one device key between them.
    QJsonObject sharing = good;
    QJsonObject borrower = alice;
    borrower["id"] = newMember().toHex();
    sharing["people"] = QJsonArray{alice, borrower};
    bad.append(sharing);

    QJsonObject badRole = good;
    QJsonObject king = alice;
    king["role"] = "king";
    badRole["people"] = QJsonArray{king};
    bad.append(badRole);

    QJsonObject badPurpose = good;
    QJsonObject purpose = invites.first().toObject();
    purpose["for"] = "everyone";
    badPurpose["invites"] = QJsonArray{purpose};
    bad.append(badPurpose);

    QJsonObject orphan = good;
    QJsonObject unknownPerson = invites.first().toObject();
    unknownPerson["maker"] = newMember().toHex();
    orphan["invites"] = QJsonArray{unknownPerson};
    bad.append(orphan);

    // An invite with a device's keys.
    QJsonObject sharedKey = good;
    QJsonObject borrowed = invites.first().toObject();
    const QJsonObject aliceLaptop = alice["devices"].toArray().first().toObject();
    borrowed["publicKey"] = aliceLaptop["publicKey"];
    borrowed["verifyKey"] = aliceLaptop["verifyKey"];
    sharedKey["invites"] = QJsonArray{borrowed};
    bad.append(sharedKey);

    QJsonObject badInviteRole = good;
    QJsonObject duke = invites.first().toObject();
    duke["role"] = "duke";
    badInviteRole["invites"] = QJsonArray{duke};
    bad.append(badInviteRole);

    QJsonObject shortKey = good;
    QJsonObject truncated = alice;
    QJsonArray devices = truncated["devices"].toArray();
    QJsonObject device = devices.first().toObject();
    device["publicKey"] = device["publicKey"].toString().left(10);
    devices[0] = device;
    truncated["devices"] = devices;
    shortKey["people"] = QJsonArray{truncated};
    bad.append(shortKey);

    QJsonObject extra = good;
    extra["names"] = QJsonArray();
    bad.append(extra);

    QJsonObject badHash = good;
    badHash["formerKeys"] = QJsonArray{QString("not a hash")};
    bad.append(badHash);

    QJsonObject missingHistory = good;
    missingHistory.remove("formerKeys");
    bad.append(missingHistory);

    QJsonObject missingInvites = good;
    missingInvites.remove("invites");
    bad.append(missingInvites);

    for (qsizetype i = 0; i < bad.size(); ++i)
    {
        QVERIFY2(!SealingSet::fromJson(bad.at(i)).has_value(), qPrintable(QString("case %1").arg(i)));
    }

    // Anchor: Alice alone, with no invites, is a set.
    QJsonObject aliceAlone = good;
    aliceAlone["people"] = QJsonArray{alice};
    aliceAlone["invites"] = QJsonArray();
    QVERIFY(SealingSet::fromJson(aliceAlone).has_value());
}
