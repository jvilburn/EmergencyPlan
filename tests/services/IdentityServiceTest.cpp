#include "IdentityServiceTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "IdentityService.h"

#include <QFile>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

std::unique_ptr<IdentityService> openAt(const QString& path)
{
    return IdentityService::open(path, nullptr);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

void IdentityServiceTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void IdentityServiceTest::aFirstRunCreatesKeysAndNoPerson()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    const std::unique_ptr<IdentityService> service = openAt(path);
    QVERIFY(service);
    QVERIFY(QFile::exists(path));
    QVERIFY(!service->hasPerson());
    QVERIFY(!service->identity().has_value());
    QVERIFY(service->deviceKeys().isValid());
}

void IdentityServiceTest::theKeysSurviveReopening()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    const DeviceKeys first = openAt(path)->deviceKeys();
    QVERIFY(openAt(path)->deviceKeys() == first);

    // A different place is a different device.
    QVERIFY(openAt(folder.filePath("other.json"))->deviceKeys() != first);
}

void IdentityServiceTest::createPersonGivesAFreshMemberId()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    std::unique_ptr<IdentityService> alice = openAt(folder.filePath("alice.json"));
    std::unique_ptr<IdentityService> bob = openAt(folder.filePath("bob.json"));
    QVERIFY(alice->createPerson("  Alice Adams ", " Laptop ", nullptr));
    QVERIFY(bob->createPerson("Bob Baker", "Phone", nullptr));

    const DeviceIdentity identity = alice->identity().value();
    QVERIFY(identity.member.isValid());
    QVERIFY(identity.member != bob->identity()->member);
    QCOMPARE(identity.displayName, QString("Alice Adams"));
    QCOMPARE(identity.deviceName, QString("Laptop"));

    // The identity is this device's keys.
    QVERIFY(identity.sealing.publicKey == alice->deviceKeys().publicKey);
    QVERIFY(identity.signing.verifyKey == alice->deviceKeys().verifyKey);
    QVERIFY(identity.fingerprint().isValid());
}

void IdentityServiceTest::joinPersonKeepsTheGivenMemberId()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const MemberId member = ChainTestSupport::newMember();

    std::unique_ptr<IdentityService> phone = openAt(folder.filePath("identity.json"));
    QVERIFY(phone->joinPerson(member, "Alice Adams", "Phone", nullptr));

    QVERIFY(phone->identity()->member == member);
    QCOMPARE(phone->identity()->displayName, QString("Alice Adams"));
    QCOMPARE(phone->identity()->deviceName, QString("Phone"));
}

void IdentityServiceTest::thePersonAndNamesSurviveAReload()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    std::unique_ptr<IdentityService> service = openAt(path);
    QVERIFY(service->createPerson("Alice Adams", "Laptop", nullptr));
    const DeviceIdentity before = service->identity().value();
    service.reset();

    const DeviceIdentity after = openAt(path)->identity().value();
    QVERIFY(after.member == before.member);
    QCOMPARE(after.displayName, before.displayName);
    QCOMPARE(after.deviceName, before.deviceName);
    QVERIFY(after.fingerprint() == before.fingerprint());
    QVERIFY(after.signing.signingKey == before.signing.signingKey);
}

void IdentityServiceTest::aPersonIsSetOnce()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    std::unique_ptr<IdentityService> service = openAt(path);
    QVERIFY(service->createPerson("Alice Adams", "Laptop", nullptr));
    const DeviceIdentity first = service->identity().value();

    QString error;
    QVERIFY(!service->createPerson("Someone Else", "Desktop", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!service->joinPerson(ChainTestSupport::newMember(), "Someone Else", "Desktop", nullptr));

    QVERIFY(service->identity()->member == first.member);
    QCOMPARE(openAt(path)->identity()->displayName, QString("Alice Adams"));
}

void IdentityServiceTest::namesAndAMemberIdAreRequired()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    std::unique_ptr<IdentityService> service = openAt(folder.filePath("identity.json"));

    QString error;
    QVERIFY(!service->createPerson("   ", "Laptop", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!service->createPerson("Alice Adams", "", nullptr));
    QVERIFY(!service->joinPerson(MemberId(), "Alice Adams", "Laptop", nullptr));
    QVERIFY(!service->hasPerson());

    // Anchor: with everything given, it takes.
    QVERIFY(service->createPerson("Alice Adams", "Laptop", nullptr));
}

void IdentityServiceTest::aDamagedFileIsReportedAndNotReplaced()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not an identity");
    file.close();

    QString error;
    QVERIFY(!IdentityService::open(path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(path), QByteArray("{ not an identity"));
}

void IdentityServiceTest::theDefaultDeviceNameIsTheHostName()
{
    QCOMPARE(IdentityService::defaultDeviceName(), QSysInfo::machineHostName());
    QVERIFY(!IdentityService::defaultDeviceName().isEmpty());
}
