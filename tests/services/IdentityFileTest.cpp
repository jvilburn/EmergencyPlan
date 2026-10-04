#include "IdentityFileTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "IdentityFile.h"

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
    record.member = ChainTestSupport::newMember();
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
    keysOnly.member.reset();
    const std::optional<IdentityRecord> back = IdentityRecord::fromBytes(keysOnly.toBytes());
    QVERIFY(back == keysOnly);
    QVERIFY(!back->member.has_value());

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

    QCOMPARE(json.keys(), (QStringList{"member", "sealingSecret", "signingSeed"}));
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
    for (const char* key : {"sealingSecret", "signingSeed", "member"})
    {
        QJsonObject json = QJsonDocument::fromJson(record.toBytes()).object();
        json[key] = HexCodec::encode(QByteArray(5, 'x'));
        QVERIFY2(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value(), key);
    }

    QVERIFY(!IdentityRecord::fromBytes("not json").has_value());
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

    const QFileDevice::Permissions permissions = QFile::permissions(path);
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
#ifndef Q_OS_WIN
    // Windows reports these from the read-only flag alone; its folder's access
    // list is what keeps the file to its user there.
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
#endif
}
