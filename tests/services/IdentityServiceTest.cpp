#include "IdentityServiceTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "IdentityService.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

std::unique_ptr<IdentityService> openAt(const QString& path)
{
    IdentityFile::Load outcome = IdentityFile::Load::NotFound;
    return IdentityService::open(path, outcome, nullptr);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
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

    IdentityFile::Load outcome = IdentityFile::Load::Failed;
    const std::unique_ptr<IdentityService> service = IdentityService::open(path, outcome, nullptr);
    QVERIFY(service);
    QCOMPARE(outcome, IdentityFile::Load::NotFound);
    QVERIFY(QFile::exists(path));
    QVERIFY(!service->identity().has_value());
    QVERIFY(service->deviceKeys().isValid());
}

void IdentityServiceTest::aFirstRunThatCannotStoreFails()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    // The identity's folder is a file, so the first identity cannot be stored, and
    // keys that would be lost on the next start are never handed out.
    const QString blocker = folder.filePath("blocker");
    QVERIFY(writeFile(blocker, "a file, not a folder"));

    IdentityFile::Load outcome = IdentityFile::Load::Failed;
    QString error;
    QVERIFY(!IdentityService::open(blocker + "/identity.json", outcome, &error));
    QCOMPARE(outcome, IdentityFile::Load::NotFound);
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFile::exists(blocker + "/identity.json"));
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

    // Typed names are trimmed.
    QCOMPARE(identity.displayName, QString("Alice Adams"));
    QCOMPARE(identity.deviceName, QString("Laptop"));

    // The identity is this device's keys.
    QVERIFY(identity.sealing.publicKey == alice->deviceKeys().publicKey);
    QVERIFY(identity.signing.verifyKey == alice->deviceKeys().verifyKey);
}

void IdentityServiceTest::joinPersonKeepsTheGivenIdAndNameAndSurvivesAReload()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");
    const MemberId member = ChainTestSupport::newMember();

    // The display name exactly as handed over, as the roster carries it.
    const QString handedOver(" Alice Adams ");
    std::unique_ptr<IdentityService> phone = openAt(path);
    QVERIFY(phone->joinPerson(member, handedOver, " Phone ", nullptr));
    QVERIFY(phone->identity()->member == member);
    QCOMPARE(phone->identity()->displayName, handedOver);
    QCOMPARE(phone->identity()->deviceName, QString("Phone"));
    phone.reset();

    const DeviceIdentity reloaded = openAt(path)->identity().value();
    QVERIFY(reloaded.member == member);
    QCOMPARE(reloaded.displayName, handedOver);
    QCOMPARE(reloaded.deviceName, QString("Phone"));
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

void IdentityServiceTest::namesAndAMemberIdAreRequiredAndARefusalWritesNothing()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");
    std::unique_ptr<IdentityService> service = openAt(path);
    const QByteArray before = readFile(path);

    QString error;
    QVERIFY(!service->createPerson("   ", "Laptop", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!service->createPerson("Alice Adams", "", nullptr));
    QVERIFY(!service->joinPerson(ChainTestSupport::newMember(), "  ", "Phone", nullptr));

    // An invalid id says so, rather than asking for names.
    QString idError;
    QVERIFY(!service->joinPerson(MemberId(), "Alice Adams", "Laptop", &idError));
    QVERIFY(!idError.isEmpty());
    QVERIFY(idError != error);

    QVERIFY(!service->identity().has_value());
    QCOMPARE(readFile(path), before);

    // Anchor: with everything given, it takes.
    QVERIFY(service->createPerson("Alice Adams", "Laptop", nullptr));
    QVERIFY(readFile(path) != before);
}

void IdentityServiceTest::aPersonThatCannotBeStoredIsNotTaken()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString subfolder = folder.filePath("sub");
    const QString path = subfolder + "/identity.json";
    std::unique_ptr<IdentityService> service = openAt(path);
    QVERIFY(service);

    // The folder becomes a file, so the person cannot be stored.
    QVERIFY(QDir(subfolder).removeRecursively());
    QVERIFY(writeFile(subfolder, "a file, not a folder"));

    QString error;
    QVERIFY(!service->createPerson("Alice Adams", "Laptop", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!service->identity().has_value());
}

void IdentityServiceTest::aDamagedFileIsReportedAndNotReplaced()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");
    QVERIFY(writeFile(path, "{ not an identity"));

    IdentityFile::Load outcome = IdentityFile::Load::Loaded;
    QString error;
    QVERIFY(!IdentityService::open(path, outcome, &error));
    QCOMPARE(outcome, IdentityFile::Load::Failed);
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(path), QByteArray("{ not an identity"));
}

void IdentityServiceTest::aNewerVersionsFileIsReportedAndNotReplaced()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");
    openAt(path);

    // A later version that also added a field this build does not know.
    QJsonObject json = QJsonDocument::fromJson(readFile(path)).object();
    json["version"] = IdentityRecord::CurrentVersion + 1;
    json["somethingNew"] = true;
    const QByteArray newer = QJsonDocument(json).toJson();
    QVERIFY(writeFile(path, newer));

    IdentityFile::Load outcome = IdentityFile::Load::Loaded;
    QString error;
    QVERIFY(!IdentityService::open(path, outcome, &error));
    QCOMPARE(outcome, IdentityFile::Load::NeedsNewerVersion);
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(path), newer);
}

void IdentityServiceTest::startingOverKeepsTheOldFileAndMakesANewDevice()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    std::unique_ptr<IdentityService> before = openAt(path);
    QVERIFY(before->createPerson("Alice Adams", "Laptop", nullptr));
    const DeviceKeys oldKeys = before->deviceKeys();
    const QByteArray oldBytes = readFile(path);
    before.reset();

    std::unique_ptr<IdentityService> after = IdentityService::startOver(path, nullptr);
    QVERIFY(after);
    QVERIFY(after->deviceKeys() != oldKeys);
    QVERIFY(!after->identity().has_value());

    // The old file sits beside the new one, byte for byte, ready to be restored.
    const QStringList setAside = QDir(folder.path()).entryList({"identity-set-aside-*.json"}, QDir::Files);
    QCOMPARE(setAside.size(), qsizetype(1));
    QCOMPARE(readFile(folder.filePath(setAside.first())), oldBytes);
    QVERIFY(openAt(path)->deviceKeys() == after->deviceKeys());
}
