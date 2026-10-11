#include "AccessEventsTest.h"

#include "AccessEvents.h"
#include "DocumentCrypto.h"
#include "SessionTestSupport.h"

#include <QtTest>

using namespace SessionTestSupport;
using Kind = AccessEvents::Event::Kind;

namespace
{

/// Alice, an admin; Bob and Tom, members; Bob's second device.
struct People
{
    DeviceIdentity alice = newIdentity("Alice Reed", "Laptop");
    DeviceIdentity bob = newIdentity("Bob Wilson", "Phone");
    DeviceIdentity bobTablet = newDeviceOf(bob, "Tablet");
    DeviceIdentity tom = newIdentity("Tom Hale", "Laptop");

    /// All three, by name.
    AccessRoster roster() const
    {
        AccessRoster roster;
        roster.addPerson(alice.asNewPerson());
        roster.addPerson(bob.asNewPerson());
        roster.addPerson(tom.asNewPerson());
        return roster;
    }
};

RecipientEntry entryFor(const DeviceIdentity& device)
{
    RecipientEntry entry;
    entry.member = device.member;
    entry.fingerprint = device.fingerprint();
    return entry;
}

/// A header written by writer at minute, listing devices and admins.
DocumentHeader header(const DeviceIdentity& writer,
                      int minute,
                      const QList<DeviceIdentity>& devices,
                      const QList<DeviceIdentity>& admins)
{
    DocumentHeader header;
    header.lastWriter = writer.fingerprint();
    header.lastWriteTime = QDateTime(QDate(2026, 10, 8), QTime(9, minute), QTimeZone::UTC);
    for (const DeviceIdentity& device : devices)
    {
        header.recipients.append(entryFor(device));
    }
    for (const DeviceIdentity& admin : admins)
    {
        header.admins.append(admin.member);
    }
    return header;
}

QStringList described(const QList<AccessEvents::Event>& events)
{
    QStringList lines;
    for (const AccessEvents::Event& event : events)
    {
        lines.append(event.describe());
    }
    return lines;
}

} // namespace

void AccessEventsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void AccessEventsTest::eachKindComesFromTheHeaderPairThatMakesIt()
{
    const People p;
    const AccessRoster roster = p.roster();
    const DocumentHeader alone = header(p.alice, 0, {p.alice}, {p.alice});
    const DocumentHeader withBob = header(p.alice, 1, {p.alice, p.bob}, {p.alice});
    const DocumentHeader bobAdmin = header(p.alice, 2, {p.alice, p.bob}, {p.alice, p.bob});
    const DocumentHeader bobMember = header(p.alice, 3, {p.alice, p.bob}, {p.alice});
    const DocumentHeader bobGone = header(p.alice, 4, {p.alice}, {p.alice});

    const QList<AccessEvents::Event> added = AccessEvents::between(alone, {withBob}, roster, roster);
    QCOMPARE(added.size(), qsizetype(1));
    QCOMPARE(added.at(0).kind, Kind::Added);
    QCOMPARE(added.at(0).actor, QString("Alice Reed"));
    QCOMPARE(added.at(0).subject, QString("Bob Wilson"));
    QCOMPARE(added.at(0).when, withBob.lastWriteTime);
    QCOMPARE(added.at(0).describe(), QString("Alice Reed added Bob Wilson"));

    const QList<AccessEvents::Event> madeAdmin = AccessEvents::between(withBob, {bobAdmin}, roster, roster);
    QCOMPARE(described(madeAdmin), QStringList{"Alice Reed made Bob Wilson an admin"});
    QCOMPARE(madeAdmin.at(0).kind, Kind::MadeAdmin);

    const QList<AccessEvents::Event> madeMember = AccessEvents::between(bobAdmin, {bobMember}, roster, roster);
    QCOMPARE(described(madeMember), QStringList{"Alice Reed made Bob Wilson a member"});
    QCOMPARE(madeMember.at(0).kind, Kind::MadeMember);

    const QList<AccessEvents::Event> removed = AccessEvents::between(bobMember, {bobGone}, roster, roster);
    QCOMPARE(described(removed), QStringList{"Alice Reed removed Bob Wilson"});
    QCOMPARE(removed.at(0).kind, Kind::Removed);

    // A header that changes nothing about people says nothing.
    QVERIFY(AccessEvents::between(withBob, {header(p.bob, 5, {p.alice, p.bob}, {p.alice})}, roster, roster).isEmpty());
}

void AccessEventsTest::aLeaveIsLeftNotRemoved()
{
    const People p;
    const AccessRoster roster = p.roster();
    const DocumentHeader withBob = header(p.alice, 0, {p.alice, p.bob}, {p.alice});

    // Written by one of Bob's own devices, which the header that drops him no longer lists.
    const QList<AccessEvents::Event> events =
        AccessEvents::between(withBob, {header(p.bob, 1, {p.alice}, {p.alice})}, roster, roster);
    QCOMPARE(events.size(), qsizetype(1));
    QCOMPARE(events.at(0).kind, Kind::Left);
    QCOMPARE(events.at(0).describe(), QString("Bob Wilson left"));
}

void AccessEventsTest::steppingDownIsToldApartFromBeingMadeAMember()
{
    const People p;
    const AccessRoster roster = p.roster();
    const DocumentHeader bothAdmins = header(p.alice, 0, {p.alice, p.bob}, {p.alice, p.bob});

    const QList<AccessEvents::Event> events =
        AccessEvents::between(bothAdmins, {header(p.bob, 1, {p.alice, p.bob}, {p.alice})}, roster, roster);
    QCOMPARE(events.size(), qsizetype(1));
    QCOMPARE(events.at(0).kind, Kind::SteppedDown);
    QCOMPARE(events.at(0).describe(), QString("Bob Wilson stepped down as admin"));

    // Two people sharing a name are still told apart: one demoting the other is not
    // stepping down, which is the case the shared-name warning is there to catch.
    const DeviceIdentity otherBob = newIdentity("Bob Wilson", "Laptop");
    AccessRoster twoBobs = roster;
    twoBobs.addPerson(otherBob.asNewPerson());
    const QList<AccessEvents::Event> demoted =
        AccessEvents::between(header(p.alice, 2, {p.alice, p.bob, otherBob}, {p.alice, p.bob, otherBob}),
                              {header(otherBob, 3, {p.alice, p.bob, otherBob}, {p.alice, otherBob})},
                              twoBobs,
                              twoBobs);
    QCOMPARE(demoted.size(), qsizetype(1));
    QCOMPARE(demoted.at(0).kind, Kind::MadeMember);
    QCOMPARE(demoted.at(0).describe(), QString("Bob Wilson made Bob Wilson a member"));
}

void AccessEventsTest::aGrantThenARemovalBetweenTwoSyncsYieldsBoth()
{
    const People p;
    const AccessRoster roster = p.roster();
    const DocumentHeader base = header(p.alice, 0, {p.alice, p.bob}, {p.alice});

    const QList<AccessEvents::Event> events =
        AccessEvents::between(base,
                              {header(p.alice, 1, {p.alice, p.bob, p.tom}, {p.alice}),
                               header(p.alice, 2, {p.alice, p.bob}, {p.alice})},
                              roster,
                              roster);
    QCOMPARE(described(events), (QStringList{"Alice Reed added Tom Hale", "Alice Reed removed Tom Hale"}));
}

void AccessEventsTest::linkingOrUnlinkingOnesOwnDeviceYieldsNothing()
{
    const People p;
    const AccessRoster roster = p.roster();
    const DocumentHeader phoneOnly = header(p.alice, 0, {p.alice, p.bob}, {p.alice});
    const DocumentHeader bothDevices = header(p.bob, 1, {p.alice, p.bob, p.bobTablet}, {p.alice});

    QVERIFY(AccessEvents::between(phoneOnly, {bothDevices}, roster, roster).isEmpty());
    QVERIFY(AccessEvents::between(bothDevices, {header(p.bob, 2, {p.alice, p.bob}, {p.alice})}, roster, roster)
                .isEmpty());

    // Anchor: the same pair with Bob's person added instead is an event.
    QCOMPARE(AccessEvents::between(header(p.alice, 0, {p.alice}, {p.alice}), {bothDevices}, roster, roster).size(),
             qsizetype(1));
}

void AccessEventsTest::namesComeFromTheCurrentRosterThenTheBaseThenAFormerMember()
{
    const People p;
    AccessRoster current;
    current.addPerson(p.alice.asNewPerson());
    AccessRoster base;
    base.addPerson(p.alice.asNewPerson());
    base.addPerson(p.bob.asNewPerson());

    // Bob is only in the base copy's roster; Tom is in neither.
    const QList<AccessEvents::Event> events =
        AccessEvents::between(header(p.alice, 0, {p.alice, p.bob, p.tom}, {p.alice}),
                              {header(p.alice, 1, {p.alice}, {p.alice})},
                              current,
                              base);
    QCOMPARE(events.size(), qsizetype(2));
    QVERIFY(described(events).contains("Alice Reed removed Bob Wilson"));
    QVERIFY(described(events).contains("Alice Reed removed a former member"));

    // A writer listed in neither header is a former member too.
    const QList<AccessEvents::Event> unlisted =
        AccessEvents::between(header(p.alice, 2, {p.alice}, {p.alice}),
                              {header(p.tom, 3, {p.alice, p.bob}, {p.alice})},
                              current,
                              base);
    QCOMPARE(described(unlisted), QStringList{"a former member added Bob Wilson"});
}

void AccessEventsTest::eachDeviceIsAddedByTheWriterOfTheHeaderThatFirstListsIt()
{
    const People p;
    const DocumentHeader created = header(p.alice, 0, {p.alice}, {p.alice});
    const DocumentHeader granted = header(p.alice, 5, {p.alice, p.bob}, {p.alice});
    const DocumentHeader linked = header(p.bob, 9, {p.alice, p.bob, p.bobTablet}, {p.alice});

    const QHash<QString, AccessEvents::Origin> origins = AccessEvents::deviceOrigins({created, granted, linked});
    QCOMPARE(origins.size(), qsizetype(3));
    QVERIFY(origins.value(p.alice.fingerprint().toHex()).addedBy == p.alice.fingerprint());
    QCOMPARE(origins.value(p.alice.fingerprint().toHex()).addedAt, created.lastWriteTime);
    QVERIFY(origins.value(p.bob.fingerprint().toHex()).addedBy == p.alice.fingerprint());
    QCOMPARE(origins.value(p.bob.fingerprint().toHex()).addedAt, granted.lastWriteTime);
    QVERIFY(origins.value(p.bobTablet.fingerprint().toHex()).addedBy == p.bob.fingerprint());
    QCOMPARE(origins.value(p.bobTablet.fingerprint().toHex()).addedAt, linked.lastWriteTime);

    // Unlinked, a device is gone from the result.
    const DocumentHeader unlinked = header(p.bob, 10, {p.alice, p.bob}, {p.alice});
    const QHash<QString, AccessEvents::Origin> gone = AccessEvents::deviceOrigins({created, granted, linked, unlinked});
    QCOMPARE(gone.size(), qsizetype(2));
    QVERIFY(!gone.contains(p.bobTablet.fingerprint().toHex()));

    // Linked again - by Bob's phone, since only Bob adds Bob's devices - it was added
    // by the later link.
    const DocumentHeader relinked = header(p.bob, 12, {p.alice, p.bob, p.bobTablet}, {p.alice});
    const QHash<QString, AccessEvents::Origin> again =
        AccessEvents::deviceOrigins({created, granted, linked, unlinked, relinked});
    QVERIFY(again.value(p.bobTablet.fingerprint().toHex()).addedBy == p.bob.fingerprint());
    QCOMPARE(again.value(p.bobTablet.fingerprint().toHex()).addedAt, relinked.lastWriteTime);
}
