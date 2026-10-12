#include "SealingSetTest.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "SealingSet.h"

#include <QJsonArray>
#include <QtTest>

namespace
{

DeviceKeys newDevice()
{
    return DeviceKeys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
}

QString hexOf(const DeviceKeys& device)
{
    return DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey).toHex();
}

SealingSet setOf(const QList<DeviceKeys>& devices)
{
    SealingSet set;
    for (const DeviceKeys& device : devices)
    {
        set.add(device);
    }
    return set;
}

} // namespace

void SealingSetTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- The rule ----------------------------------------------------------------------

void SealingSetTest::keepingEveryListedDeviceKeepsTheKey()
{
    const DeviceKeys alice = newDevice();
    const DeviceKeys bob = newDevice();
    const SealingSet set = setOf({alice, bob});

    // The allow-list may list more than the set holds.
    const SealingSet::Next next = set.next({hexOf(alice), hexOf(bob), hexOf(newDevice())}, {});
    QVERIFY(!next.rotate);
    QVERIFY(next.devices == set);
}

void SealingSetTest::droppingADeviceRotates()
{
    const DeviceKeys alice = newDevice();
    const DeviceKeys bob = newDevice();
    const DeviceKeys carol = newDevice();

    const SealingSet::Next next = setOf({alice, bob, carol}).next({hexOf(alice), hexOf(bob)}, {});
    QVERIFY(next.rotate);
    QCOMPARE(next.devices.size(), 2);
    QVERIFY(!next.devices.contains(DocumentCrypto::fingerprintOf(carol.publicKey, carol.verifyKey)));
    QVERIFY(next.devices == setOf({alice, bob}));
}

void SealingSetTest::anApprovedListedDeviceIsAdded()
{
    const DeviceKeys laptop = newDevice();
    const DeviceKeys phone = newDevice();

    const SealingSet::Next next = setOf({laptop}).next({hexOf(laptop), hexOf(phone)}, {phone});
    QVERIFY(!next.rotate);
    QVERIFY(next.devices == setOf({laptop, phone}));
}

void SealingSetTest::anApprovedDeviceTheServerDoesNotListIsNotAdded()
{
    const DeviceKeys laptop = newDevice();
    const DeviceKeys unlinked = newDevice();

    const SealingSet::Next next = setOf({laptop}).next({hexOf(laptop)}, {unlinked});
    QVERIFY(!next.rotate);
    QVERIFY(next.devices == setOf({laptop}));
}

void SealingSetTest::aDeviceOnlyTheServerListsIsNeverAdded()
{
    const DeviceKeys laptop = newDevice();
    const DeviceKeys planted = newDevice();
    const DeviceKeys phone = newDevice();

    // Listed and approved: added. Listed alone: not.
    const SealingSet::Next next = setOf({laptop}).next({hexOf(laptop), hexOf(planted), hexOf(phone)}, {phone});
    QCOMPARE(next.devices.size(), 2);
    QVERIFY(!next.devices.contains(DocumentCrypto::fingerprintOf(planted.publicKey, planted.verifyKey)));
}

void SealingSetTest::formerKeysCarryOver()
{
    const DeviceKeys alice = newDevice();
    const DeviceKeys bob = newDevice();
    const DataKey first = DocumentCrypto::generateDataKey();
    const DataKey second = DocumentCrypto::generateDataKey();

    SealingSet set = setOf({alice, bob});
    QVERIFY(!set.descendsFrom(first));
    set.addFormerKey(first);
    set.addFormerKey(second);
    QVERIFY(set.descendsFrom(first));
    QVERIFY(set.descendsFrom(second));
    QVERIFY(!set.descendsFrom(DocumentCrypto::generateDataKey()));

    // Through a rotation, and through JSON.
    const SealingSet::Next next = set.next({hexOf(alice)}, {});
    QVERIFY(next.rotate);
    QVERIFY(next.devices.descendsFrom(first));
    QVERIFY(next.devices.descendsFrom(second));
    QVERIFY(SealingSet::fromJson(next.devices.toJson()).value().descendsFrom(first));

    // Equality covers them: two sets of the same devices with different histories differ.
    QVERIFY(set != setOf({alice, bob}));
}

// --- The set -----------------------------------------------------------------------

void SealingSetTest::aDeviceSharingAKeyIsRefused()
{
    const DeviceKeys alice = newDevice();
    SealingSet set = SealingSet::of(alice);

    const DeviceKeys samePublicKey{alice.publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
    const DeviceKeys sameVerifyKey{DocumentCrypto::generateKeypair().publicKey, alice.verifyKey};
    QVERIFY(!set.add(samePublicKey));
    QVERIFY(!set.add(sameVerifyKey));
    QVERIFY(!set.add(DeviceKeys()));
    QCOMPARE(set.size(), 1);

    // Anchor: a device of its own is taken.
    QVERIFY(set.add(newDevice()));
    QCOMPARE(set.size(), 2);
}

void SealingSetTest::addingADeviceTwiceKeepsOne()
{
    const DeviceKeys alice = newDevice();
    SealingSet set = SealingSet::of(alice);
    QVERIFY(set.add(alice));
    QCOMPARE(set.size(), 1);
}

void SealingSetTest::devicesComeInFingerprintOrder()
{
    QList<DeviceKeys> devices;
    for (int i = 0; i < 5; ++i)
    {
        devices.append(newDevice());
    }
    const QList<DeviceKeys> listed = setOf(devices).devices();
    QCOMPARE(listed.size(), 5);
    for (qsizetype i = 1; i < listed.size(); ++i)
    {
        QVERIFY(hexOf(listed.at(i - 1)) < hexOf(listed.at(i)));
    }
}

void SealingSetTest::ofInvalidKeysIsEmpty()
{
    QVERIFY(SealingSet::of(DeviceKeys()).isEmpty());
    QCOMPARE(SealingSet::of(newDevice()).size(), 1);
}

// --- JSON --------------------------------------------------------------------------

void SealingSetTest::roundTripsThroughJson()
{
    SealingSet set = setOf({newDevice(), newDevice(), newDevice()});
    set.addFormerKey(DocumentCrypto::generateDataKey());

    const std::optional<SealingSet> read = SealingSet::fromJson(set.toJson());
    QVERIFY(read.has_value());
    QVERIFY(read.value() == set);
}

void SealingSetTest::fromJsonRefusesAnythingIllFormed()
{
    const DeviceKeys alice = newDevice();
    const DeviceKeys bob = newDevice();
    const QJsonObject good = setOf({alice, bob}).toJson();
    QVERIFY(SealingSet::fromJson(good).has_value());

    QJsonObject empty = good;
    empty["devices"] = QJsonArray();
    QVERIFY(!SealingSet::fromJson(empty).has_value());

    QJsonObject repeated = good;
    QJsonArray twice = good["devices"].toArray();
    twice.append(twice.first());
    repeated["devices"] = twice;
    QVERIFY(!SealingSet::fromJson(repeated).has_value());

    QJsonObject sharing = good;
    QJsonArray shared = good["devices"].toArray();
    QJsonObject borrowed = DeviceKeys{alice.publicKey, DocumentCrypto::generateSigningKeypair().verifyKey}.toJson();
    shared.append(borrowed);
    sharing["devices"] = shared;
    QVERIFY(!SealingSet::fromJson(sharing).has_value());

    QJsonObject extra = good;
    extra["names"] = QJsonArray();
    QVERIFY(!SealingSet::fromJson(extra).has_value());

    QJsonObject shortKey = good;
    QJsonArray truncated = good["devices"].toArray();
    QJsonObject device = truncated.first().toObject();
    device["publicKey"] = device["publicKey"].toString().left(10);
    truncated[0] = device;
    shortKey["devices"] = truncated;
    QVERIFY(!SealingSet::fromJson(shortKey).has_value());

    QJsonObject badHash = good;
    badHash["formerKeys"] = QJsonArray{QString("not a hash")};
    QVERIFY(!SealingSet::fromJson(badHash).has_value());

    QJsonObject missingHistory = good;
    missingHistory.remove("formerKeys");
    QVERIFY(!SealingSet::fromJson(missingHistory).has_value());
}
