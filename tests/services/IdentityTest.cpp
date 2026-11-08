#include "IdentityTest.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Identity.h"
#include "JsonReader.h"
#include "KeyFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace
{

/// RFC 7748 section 6.1, Alice's X25519 secret key.
const char* RfcSealingSecret = "dwdtCnMYpX08FsFyUbJmRd9ML4frwJkqsXf7pR25LCo";
/// RFC 8032 section 7.1 test 1, the Ed25519 secret key (its seed).
const char* RfcSigningSeed = "nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A";

QByteArray contentsOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray();
    }
    return file.readAll();
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
}

}  // namespace

IdentityTest::IdentityTest(QObject* parent)
    : QObject(parent)
{
}

void IdentityTest::fingerprintIsTheKnownAnswer()
{
    QByteArray publicKey;
    QByteArray verifyKey;
    for (int i = 0; i < 32; ++i)
    {
        publicKey.append(char(i));
        verifyKey.append(char(32 + i));
    }
    QCOMPARE(Base64Url::encode(Identity::fingerprintOf(publicKey, verifyKey)), QString("YwoyV2kctuI1OuIvdMVNUQ"));

    Identity rfc = Identity::fromSecrets(*Base64Url::decode(RfcSealingSecret, 32), *Base64Url::decode(RfcSigningSeed, 32));
    QCOMPARE(Base64Url::encode(rfc.fingerprint()), QString("DI_sTH61XLT7R-VIQNmOqw"));
    QCOMPARE(Base64Url::encode(Identity::fingerprintOf(rfc.verifyKey(), rfc.publicKey())), QString("DvFtelC8pSi0ML6Lf-zWhA"));
    QCOMPARE(Base64Url::encode(Crypto::hash(rfc.publicKey() + rfc.verifyKey(), 16, QByteArray())), QString("TQFcCcNbKX3zZ6pVBJv_pQ"));
}

void IdentityTest::keyFileSecretsGiveTheKnownKeysAndFingerprint()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/keys/device.key";
    writeFile(path, QByteArray("{\"format\":1,\"sealingSecret\":\"") + RfcSealingSecret
                        + "\",\"signingSeed\":\"" + RfcSigningSeed + "\"}");

    KeyFileResult result = KeyFile::loadOrCreate(path);

    QVERIFY2(result.status == KeyFileStatus::Loaded, qPrintable(result.problem));
    QCOMPARE(Base64Url::encode(result.identity->publicKey()), QString("hSDwCYkwp1R0i33ctD73Wg2_Og0mOBr066SpjqqbTmo"));
    QCOMPARE(Base64Url::encode(result.identity->verifyKey()), QString("11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"));
    QCOMPARE(Base64Url::encode(result.identity->fingerprint()), QString("DI_sTH61XLT7R-VIQNmOqw"));
}

void IdentityTest::keysAreMadeOnlyWhenNoKeyFileExists()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/keys/device.key";

    KeyFileResult first = KeyFile::loadOrCreate(path);
    QVERIFY2(first.status == KeyFileStatus::Created, qPrintable(first.problem));
    QByteArray written = contentsOf(path);

    KeyFileResult second = KeyFile::loadOrCreate(path);

    QVERIFY(second.status == KeyFileStatus::Loaded);
    QCOMPARE(second.identity->fingerprint(), first.identity->fingerprint());
    QCOMPARE(second.identity->publicKey(), first.identity->publicKey());
    QCOMPARE(second.identity->verifyKey(), first.identity->verifyKey());
    QCOMPARE(contentsOf(path), written);
    std::optional<QJsonObject> json = JsonReader::parseObject(written);
    QVERIFY(json.has_value());
    QCOMPARE(json->keys(), QStringList({"format", "sealingSecret", "signingSeed"}));
    QCOMPARE(json->value("format").toInt(), 1);
}

void IdentityTest::keyFileThatDoesNotParseIsUnreadableAndLeftAsItWas()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/keys/device.key";
    QByteArray damaged = QByteArray("{\"format\":1,\"sealingSecret\":\"short\",\"signingSeed\":\"") + RfcSigningSeed + "\"}";
    writeFile(path, damaged);

    KeyFileResult first = KeyFile::loadOrCreate(path);
    KeyFileResult second = KeyFile::loadOrCreate(path);

    QVERIFY(first.status == KeyFileStatus::Unreadable);
    QCOMPARE(first.problem, QString("sealingSecret: is not base64url of 32 bytes"));
    QVERIFY(!first.identity.has_value());
    QVERIFY(second.status == KeyFileStatus::Unreadable);
    QCOMPARE(contentsOf(path), damaged);
}

void IdentityTest::keyPathThatCannotBeReadIsAFaultAndNothingIsMade()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/keys/device.key";
    QVERIFY(QDir().mkpath(path));

    KeyFileResult result = KeyFile::loadOrCreate(path);

    QVERIFY(result.status == KeyFileStatus::Fault);
    QVERIFY(!result.identity.has_value());
    QVERIFY(QFileInfo(path).isDir());
}

void IdentityTest::dataKeySealedToADeviceOpensForItAlone()
{
    Identity laptop = Identity::generate();
    Identity phone = Identity::generate();
    QByteArray dataKey = Crypto::randomBytes(Crypto::KeyBytes);

    QByteArray sealed = Crypto::seal(dataKey, laptop.publicKey());

    QCOMPARE(sealed.size(), qsizetype(Crypto::SealedKeyBytes));
    QCOMPARE(laptop.unseal(sealed).value(), dataKey);
    QVERIFY(!phone.unseal(sealed).has_value());
}
