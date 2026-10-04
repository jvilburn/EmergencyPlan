#include "IdentityFileTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "IdentityFile.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

IdentityRecord newRecord()
{
    IdentityRecord record;
    record.sealing = DocumentCrypto::generateKeypair();
    record.signing = DocumentCrypto::generateSigningKeypair();
    record.person = IdentityRecord::Person{ChainTestSupport::newMember(), "Alice Adams", "Laptop"};
    return record;
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

void IdentityFileTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- The record ------------------------------------------------------------------

void IdentityFileTest::aRecordRoundTripsWithAndWithoutAPerson()
{
    const IdentityRecord withPerson = newRecord();
    QVERIFY(IdentityRecord::fromBytes(withPerson.toBytes()) == withPerson);

    IdentityRecord keysOnly = withPerson;
    keysOnly.person.reset();
    const std::optional<IdentityRecord> back = IdentityRecord::fromBytes(keysOnly.toBytes());
    QVERIFY(back == keysOnly);
    QVERIFY(!back->person.has_value());

    // Each part counts: a record differing in only one of them reads back as itself
    // and not as the other.
    QVERIFY(back != withPerson);
    IdentityRecord otherSigning = withPerson;
    otherSigning.signing = DocumentCrypto::generateSigningKeypair();
    const std::optional<IdentityRecord> otherBack = IdentityRecord::fromBytes(otherSigning.toBytes());
    QVERIFY(otherBack == otherSigning);
    QVERIFY(otherBack != withPerson);
}

void IdentityFileTest::onlyTheSecretHalvesAreWritten()
{
    const IdentityRecord record = newRecord();
    const QJsonObject json = QJsonDocument::fromJson(record.toBytes()).object();

    QCOMPARE(json.keys(), (QStringList{"person", "sealingSecret", "signingSeed"}));
    QCOMPARE(json["sealingSecret"].toString(), HexCodec::encode(record.sealing.secretKey.bytes));
    QCOMPARE(json["signingSeed"].toString(), HexCodec::encode(DocumentCrypto::seedOf(record.signing.signingKey)));

    // The public halves come back derived, equal to the ones generated with them.
    const IdentityRecord back = IdentityRecord::fromBytes(record.toBytes()).value();
    QVERIFY(back.sealing.publicKey == record.sealing.publicKey);
    QVERIFY(back.signing.verifyKey == record.signing.verifyKey);
}

void IdentityFileTest::aRecordWithASecretOfTheWrongSizeIsRefused()
{
    const IdentityRecord record = newRecord();
    for (const char* key : {"sealingSecret", "signingSeed"})
    {
        QJsonObject json = QJsonDocument::fromJson(record.toBytes()).object();
        json[key] = HexCodec::encode(QByteArray(5, 'x'));
        QVERIFY2(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value(), key);
    }

    QVERIFY(!IdentityRecord::fromBytes("not json").has_value());

    // The person whole or not at all: a member id of the wrong size, an empty name,
    // or a missing one.
    const QJsonObject full = QJsonDocument::fromJson(record.toBytes()).object();
    const QList<QPair<QString, QJsonValue>> damaged = {
        {"member", HexCodec::encode(QByteArray(5, 'x'))},
        {"displayName", QString()},
        {"deviceName", QString()},
    };
    for (const QPair<QString, QJsonValue>& change : damaged)
    {
        QJsonObject json = full;
        QJsonObject person = json["person"].toObject();
        person[change.first] = change.second;
        json["person"] = person;
        QVERIFY2(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value(), qPrintable(change.first));
    }
    QJsonObject withoutName = full;
    QJsonObject person = withoutName["person"].toObject();
    person.remove("deviceName");
    withoutName["person"] = person;
    QVERIFY(!IdentityRecord::fromBytes(QJsonDocument(withoutName).toJson()).has_value());

    // A required field missing, or of the wrong type.
    QJsonObject missing = QJsonDocument::fromJson(record.toBytes()).object();
    missing.remove("signingSeed");
    QVERIFY(!IdentityRecord::fromBytes(QJsonDocument(missing).toJson()).has_value());

    QJsonObject number = QJsonDocument::fromJson(record.toBytes()).object();
    number["sealingSecret"] = 5;
    QVERIFY(!IdentityRecord::fromBytes(QJsonDocument(number).toJson()).has_value());
}

void IdentityFileTest::aRecordWithAnUnknownFieldIsRefused()
{
    // A public key written beside its secret is refused too: there is one place
    // each key comes from.
    for (const char* key : {"displayName", "verifyKey"})
    {
        QJsonObject json = QJsonDocument::fromJson(newRecord().toBytes()).object();
        json[key] = "something";
        QVERIFY2(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value(), key);
    }
}

// --- The file --------------------------------------------------------------------

void IdentityFileTest::aMissingFileIsNotFound()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    IdentityRecord record;
    QString error;
    QCOMPARE(IdentityFile::load(folder.filePath("identity.json"), record, &error), IdentityFile::Load::NotFound);
    QVERIFY(error.isEmpty());
}

void IdentityFileTest::aStoredIdentityLoadsBack()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("nested/identity.json");

    const IdentityRecord first = newRecord();
    QVERIFY(IdentityFile::store(path, first, nullptr));
    IdentityRecord loaded;
    QCOMPARE(IdentityFile::load(path, loaded, nullptr), IdentityFile::Load::Loaded);
    QVERIFY(loaded == first);

    // Storing again replaces it.
    const IdentityRecord second = newRecord();
    QVERIFY(IdentityFile::store(path, second, nullptr));
    QCOMPARE(IdentityFile::load(path, loaded, nullptr), IdentityFile::Load::Loaded);
    QVERIFY(loaded == second);
}

void IdentityFileTest::aDamagedFileFailsAndIsLeftAlone()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    QByteArray damaged = newRecord().toBytes();
    damaged.chop(5);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(damaged);
    file.close();

    IdentityRecord record;
    QString error;
    QCOMPARE(IdentityFile::load(path, record, &error), IdentityFile::Load::Failed);
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(path), damaged);
}

void IdentityFileTest::theFileIsReadableOnlyByItsOwner()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");
    QVERIFY(IdentityFile::store(path, newRecord(), nullptr));

#ifdef Q_OS_WIN
    // Read from the file's access list rather than its read-only flag.
    QNtfsPermissionCheckGuard ntfsPermissions;
#endif
    const QFileDevice::Permissions permissions = QFile::permissions(path);
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
#ifndef Q_OS_WIN
    // On Windows a file's group is its owner's primary group, often the owner
    // itself, so there the access list's other entries are the ones to check.
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::WriteGroup));
#endif
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
    QVERIFY(!permissions.testFlag(QFileDevice::WriteOther));
}

void IdentityFileTest::aPathThatCannotBeOpenedFails()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    // There, but a folder: not a first run, and not a file to read.
    const QString path = folder.filePath("identity.json");
    QVERIFY(QDir().mkpath(path));

    IdentityRecord record;
    QString error;
    QCOMPARE(IdentityFile::load(path, record, &error), IdentityFile::Load::Failed);
    QVERIFY(!error.isEmpty());
}

void IdentityFileTest::aStoreThatCannotWriteFails()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    // The identity's folder is a file, so nothing can be written there.
    const QString blocker = folder.filePath("blocker");
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QString error;
    QVERIFY(!IdentityFile::store(blocker + "/identity.json", newRecord(), &error));
    QVERIFY(!error.isEmpty());
}

void IdentityFileTest::aRecordThatIsNotWholeIsNotStored()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("identity.json");

    // A public half that is not its secret's would be lost on writing, and the
    // stored identity would not be this one.
    IdentityRecord mismatched = newRecord();
    mismatched.sealing.publicKey = DocumentCrypto::generateKeypair().publicKey;
    QString error;
    QVERIFY(!IdentityFile::store(path, mismatched, &error));
    QVERIFY(!error.isEmpty());

    IdentityRecord invalid = newRecord();
    invalid.signing.signingKey = SigningKey();
    QVERIFY(!IdentityFile::store(path, invalid, nullptr));

    QVERIFY(!QFile::exists(path));
    QVERIFY(IdentityFile::store(path, newRecord(), nullptr));
}
