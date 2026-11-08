#include "WardFileTest.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "EveryField.h"
#include "WardFileEdits.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

namespace
{

QJsonValue removed()
{
    return QJsonValue(QJsonValue::Undefined);
}

QString encodedBytes(int length)
{
    return Base64Url::encode(Crypto::randomBytes(length));
}

}  // namespace

WardFileTest::WardFileTest(QObject* parent)
    : QObject(parent)
    , m_laptop(Identity::generate())
    , m_phone(Identity::generate())
{
}

void WardFileTest::init()
{
    m_document = DocumentId::generate();
    m_version = VersionRank{4, m_phone.fingerprint()};
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_payload = EveryField::payload(m_laptop, m_phone);
}

QByteArray WardFileTest::written() const
{
    return WardFile::write(m_document, m_version, m_dataKey, m_payload);
}

WardRead WardFileTest::readSealedTo(const QList<RecipientKeys>& recipients) const
{
    QByteArray file = WardFileEdits::sealedFile(m_document, m_dataKey, recipients, m_payload.toJson());
    return WardFile::read(file, m_laptop, PayloadKind::LocalFile);
}

void WardFileTest::roundTripRecoversThePayloadAndTheDataKey()
{
    QByteArray file = written();

    WardRead read = WardFile::read(file, m_laptop, PayloadKind::LocalFile);

    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.header->document() == m_document);
    QCOMPARE(read.header->version().number, qint64(4));
    QCOMPARE(read.header->version().writer, m_phone.fingerprint());
    QCOMPARE(read.dataKey, m_dataKey);
    QVERIFY(read.payload.value() == m_payload);
    QCOMPARE(read.header->recipients().size(), qsizetype(5));
    QCOMPARE(read.header->recipients()[0].fingerprint, m_laptop.fingerprint());
    QCOMPARE(read.header->recipients()[1].fingerprint, m_phone.fingerprint());
    QVERIFY(WardFile::read(file, m_phone, PayloadKind::LocalFile).payload.value() == m_payload);
}

void WardFileTest::writesOfAnUnchangedPayloadUseFreshNonces()
{
    QByteArray first = written();
    QByteArray second = written();

    QString firstNonce = WardFileEdits::headerOf(first)["nonce"].toString();
    QString secondNonce = WardFileEdits::headerOf(second)["nonce"].toString();
    QVERIFY(Base64Url::decode(firstNonce, Crypto::NonceBytes).has_value());
    QVERIFY(Base64Url::decode(secondNonce, Crypto::NonceBytes).has_value());
    QVERIFY(firstNonce != secondNonce);
    QVERIFY(first.mid(first.indexOf('\n')) != second.mid(second.indexOf('\n')));
    QVERIFY(WardFile::read(first, m_laptop, PayloadKind::LocalFile).payload.value() == m_payload);
    QVERIFY(WardFile::read(second, m_laptop, PayloadKind::LocalFile).payload.value() == m_payload);
}

void WardFileTest::headerIsBoundAsAssociatedData()
{
    QByteArray file = written();
    QJsonObject header = WardFileEdits::headerOf(file);
    WardRead control = WardFile::read(WardFileEdits::withHeader(file, header), m_laptop, PayloadKind::LocalFile);
    QVERIFY2(control.status == WardReadStatus::Read, qPrintable(control.problem));

    QByteArray otherKey = Crypto::seal(Crypto::randomBytes(Crypto::KeyBytes), m_phone.publicKey());
    QVERIFY(m_phone.unseal(otherKey).has_value());
    QJsonObject changed = WardFileEdits::edited(header, {"recipients", "1", "sealedKey"}, Base64Url::encode(otherKey));
    QByteArray tampered = WardFileEdits::withHeader(file, changed);
    QVERIFY(WardHeader::parse(tampered.left(tampered.indexOf('\n'))).status == HeaderStatus::Read);

    WardRead read = WardFile::read(tampered, m_laptop, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, QString("the payload does not decrypt"));
}

void WardFileTest::versionIsBoundAsAssociatedData()
{
    QByteArray file = written();
    QJsonObject header = WardFileEdits::headerOf(file);
    QCOMPARE(header["number"].toInteger(), qint64(4));
    QCOMPARE(header["writer"].toString(), Base64Url::encode(m_phone.fingerprint()));

    const QList<QJsonObject> relabeled = {
        WardFileEdits::edited(header, {"number"}, 5),
        WardFileEdits::edited(header, {"writer"}, Base64Url::encode(m_laptop.fingerprint())),
    };
    for (const QJsonObject& changed : relabeled)
    {
        QByteArray tampered = WardFileEdits::withHeader(file, changed);
        QVERIFY(WardHeader::parse(tampered.left(tampered.indexOf('\n'))).status == HeaderStatus::Read);

        WardRead read = WardFile::read(tampered, m_laptop, PayloadKind::LocalFile);

        QVERIFY(read.status == WardReadStatus::Unreadable);
        QCOMPARE(read.problem, QString("the payload does not decrypt"));
    }
}

void WardFileTest::formatAboveEveryKnownVersionIsNewer()
{
    QByteArray file = written();
    QJsonObject newer;
    newer["format"] = 2;
    newer["shape"] = QString("a header this app has never seen");

    WardRead read = WardFile::read(WardFileEdits::withHeader(file, newer), m_laptop, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Newer);
    newer["format"] = 1;
    QVERIFY(WardFile::read(WardFileEdits::withHeader(file, newer), m_laptop, PayloadKind::LocalFile).status
            == WardReadStatus::Unreadable);
}

void WardFileTest::formatThisAppDoesNotReadIsUnreadable_data()
{
    QTest::addColumn<QJsonValue>("format");

    QTest::newRow("below every known version") << QJsonValue(0);
    QTest::newRow("negative") << QJsonValue(-1);
    QTest::newRow("a string") << QJsonValue(QString("1"));
    QTest::newRow("a fraction") << QJsonValue(1.5);
    QTest::newRow("a boolean") << QJsonValue(true);
    QTest::newRow("null") << QJsonValue(QJsonValue::Null);
    QTest::newRow("missing") << removed();
}

void WardFileTest::formatThisAppDoesNotReadIsUnreadable()
{
    QFETCH(QJsonValue, format);
    QByteArray file = written();
    QJsonObject header = WardFileEdits::edited(WardFileEdits::headerOf(file), {"format"}, format);

    WardRead read = WardFile::read(WardFileEdits::withHeader(file, header), m_laptop, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Unreadable);
}

void WardFileTest::headerWithAMissingOrMalformedFieldIsUnreadable_data()
{
    QTest::addColumn<QStringList>("path");
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<QString>("problem");

    QTest::newRow("unchanged") << QStringList({"format"}) << QJsonValue(1) << QString();
    QTest::newRow("document missing") << QStringList({"document"}) << removed()
                                      << QString("document: missing");
    QTest::newRow("document padded") << QStringList({"document"}) << QJsonValue(encodedBytes(16) + "==")
                                     << QString("document: is not base64url of 16 bytes");
    QTest::newRow("document of 15 bytes") << QStringList({"document"}) << QJsonValue(encodedBytes(15))
                                          << QString("document: is not base64url of 16 bytes");
    QTest::newRow("number missing") << QStringList({"number"}) << removed()
                                    << QString("number: missing");
    QTest::newRow("number zero") << QStringList({"number"}) << QJsonValue(0)
                                 << QString("number: is below 1");
    QTest::newRow("number a fraction") << QStringList({"number"}) << QJsonValue(4.5)
                                       << QString("number: is not an integer");
    QTest::newRow("number a string") << QStringList({"number"}) << QJsonValue(QString("4"))
                                     << QString("number: is not an integer");
    QTest::newRow("writer missing") << QStringList({"writer"}) << removed()
                                    << QString("writer: missing");
    QTest::newRow("writer of 15 bytes") << QStringList({"writer"}) << QJsonValue(encodedBytes(15))
                                        << QString("writer: is not base64url of 16 bytes");
    QTest::newRow("nonce of 23 bytes") << QStringList({"nonce"}) << QJsonValue(encodedBytes(23))
                                       << QString("nonce: is not base64url of 24 bytes");
    QTest::newRow("recipients not an array") << QStringList({"recipients"}) << QJsonValue(QJsonObject())
                                             << QString("recipients: is not an array");
    QTest::newRow("fingerprint missing") << QStringList({"recipients", "0", "fingerprint"}) << removed()
                                         << QString("recipients[0].fingerprint: missing");
    QTest::newRow("sealed key of 79 bytes") << QStringList({"recipients", "0", "sealedKey"}) << QJsonValue(encodedBytes(79))
                                            << QString("recipients[0].sealedKey: is not base64url of 80 bytes");
    QTest::newRow("unexpected field") << QStringList({"comment"}) << QJsonValue(QString("hello"))
                                      << QString("comment: is not expected");
    QTest::newRow("unexpected recipient field") << QStringList({"recipients", "0", "name"}) << QJsonValue(QString("Laptop"))
                                                << QString("recipients[0].name: is not expected");
}

void WardFileTest::headerWithAMissingOrMalformedFieldIsUnreadable()
{
    QFETCH(QStringList, path);
    QFETCH(QJsonValue, value);
    QFETCH(QString, problem);
    QByteArray file = written();
    QJsonObject header = WardFileEdits::edited(WardFileEdits::headerOf(file), path, value);

    WardRead read = WardFile::read(WardFileEdits::withHeader(file, header), m_laptop, PayloadKind::LocalFile);

    if (problem.isEmpty())
    {
        QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
        return;
    }
    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, problem);
    QVERIFY(!read.payload.has_value());
}

void WardFileTest::payloadWithAMissingOrMalformedFieldIsUnreadable_data()
{
    QTest::addColumn<QStringList>("path");
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<QString>("problem");

    QString zeroFingerprint = Base64Url::encode(QByteArray(Crypto::FingerprintBytes, '\0'));

    QTest::newRow("unchanged") << QStringList({"access", "users", "0", "name"}) << QJsonValue(QString("Alice Smith"))
                               << QString();
    QTest::newRow("access missing") << QStringList({"access"}) << removed()
                                    << QString("access: missing");
    QTest::newRow("unexpected part") << QStringList({"history"}) << QJsonValue(QJsonArray())
                                     << QString("history: is not expected");
    QTest::newRow("role neither admin nor member") << QStringList({"access", "users", "0", "role"}) << QJsonValue(QString("owner"))
                                                   << QString("access.users[0].role: is not one of admin, member");
    QTest::newRow("verify key of 31 bytes") << QStringList({"access", "users", "0", "devices", "1", "verifyKey"})
                                            << QJsonValue(encodedBytes(31))
                                            << QString("access.users[0].devices[1].verifyKey: is not base64url of 32 bytes");
    QTest::newRow("device invite with a label") << QStringList({"access", "invites", "1", "label"}) << QJsonValue(QString("Carol's phone"))
                                                << QString("access.invites[1].label: is not expected");
    QTest::newRow("user invite without a label") << QStringList({"access", "invites", "0", "label"}) << removed()
                                                 << QString("access.invites[0].label: missing");
    QTest::newRow("invites written out empty") << QStringList({"access", "invites"}) << QJsonValue(QJsonArray())
                                               << QString("access.invites: is written out empty");
    QTest::newRow("former key of 31 bytes") << QStringList({"access", "formerKeys", "0"}) << QJsonValue(encodedBytes(31))
                                            << QString("access.formerKeys[0]: is not base64url of 32 bytes");
    QTest::newRow("landed key not a fingerprint") << QStringList({"landed", "not-a-fingerprint"}) << QJsonValue(4)
                                                  << QString("landed.not-a-fingerprint: is not a fingerprint");
    QTest::newRow("landed mark not an integer") << QStringList({"landed", zeroFingerprint}) << QJsonValue(QString("3"))
                                                << QString("landed.%1: is not an integer").arg(zeroFingerprint);
    QTest::newRow("flag written out false") << QStringList({"device", "noLongerShared"}) << QJsonValue(false)
                                            << QString("device.noLongerShared: is written out empty");
    QTest::newRow("invite seeds written out empty") << QStringList({"device", "inviteSeeds"}) << QJsonValue(QJsonArray())
                                                    << QString("device.inviteSeeds: is written out empty");
    QTest::newRow("journal missing") << QStringList({"device", "journal"}) << removed()
                                     << QString("device.journal: missing");
    QTest::newRow("sequence counter a fraction") << QStringList({"device", "journal", "lastSequence"}) << QJsonValue(1.5)
                                                 << QString("device.journal.lastSequence: is not an integer");
    QTest::newRow("journal entry of an unknown command") << QStringList({"device", "journal", "entries", "0", "command", "type"})
                                                         << QJsonValue(QString("noSuchCommand"))
                                                         << QString("device.journal.entries[0].command.type: is not a command this app knows");
    QTest::newRow("unexpected ward field") << QStringList({"ward", "comment"}) << QJsonValue(QString("hello"))
                                           << QString("ward.comment: is not expected");
}

void WardFileTest::payloadWithAMissingOrMalformedFieldIsUnreadable()
{
    QFETCH(QStringList, path);
    QFETCH(QJsonValue, value);
    QFETCH(QString, problem);
    QJsonObject payload = WardFileEdits::edited(m_payload.toJson(), path, value);
    QByteArray file = WardFileEdits::sealedFile(m_document, m_dataKey, m_payload.ward.access().recipients(), payload);

    WardRead read = WardFile::read(file, m_laptop, PayloadKind::LocalFile);

    if (problem.isEmpty())
    {
        QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
        QVERIFY(read.payload.value() == m_payload);
        return;
    }
    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, problem);
    QVERIFY(!read.payload.has_value());
}

void WardFileTest::versionCarriesNoDeviceStateAndALocalFileMust()
{
    WardPayload version = m_payload;
    version.device = std::nullopt;
    QVERIFY(!version.toJson().contains("device"));
    QVERIFY(m_payload.toJson().contains("device"));
    QByteArray versionFile = WardFile::write(m_document, m_version, m_dataKey, version);
    QByteArray localFile = written();

    WardRead versionAsVersion = WardFile::read(versionFile, m_laptop, PayloadKind::Version);
    WardRead versionAsLocal = WardFile::read(versionFile, m_laptop, PayloadKind::LocalFile);
    WardRead localAsVersion = WardFile::read(localFile, m_laptop, PayloadKind::Version);

    QVERIFY2(versionAsVersion.status == WardReadStatus::Read, qPrintable(versionAsVersion.problem));
    QVERIFY(versionAsVersion.payload.value() == version);
    QVERIFY(versionAsLocal.status == WardReadStatus::Unreadable);
    QCOMPARE(versionAsLocal.problem, QString("device: missing"));
    QVERIFY(localAsVersion.status == WardReadStatus::Unreadable);
    QCOMPARE(localAsVersion.problem, QString("device: is not expected"));
}

void WardFileTest::headerMustNameExactlyTheAccessListsKeys()
{
    QList<RecipientKeys> keys = m_payload.ward.access().recipients();
    QCOMPARE(keys.size(), qsizetype(5));
    WardRead control = readSealedTo(keys);
    QVERIFY2(control.status == WardReadStatus::Read, qPrintable(control.problem));

    Identity stranger = Identity::generate();
    QList<RecipientKeys> extra = keys;
    extra.append(RecipientKeys{stranger.publicKey(), stranger.verifyKey()});
    QList<RecipientKeys> mixed = keys;
    mixed[1] = RecipientKeys{keys[1].publicKey, keys[2].verifyKey};
    QList<RecipientKeys> missing = keys;
    missing.removeLast();
    QList<RecipientKeys> twice = keys;
    twice.append(keys[1]);

    const QList<QList<RecipientKeys>> refused = {extra, mixed, missing, twice};
    for (const QList<RecipientKeys>& recipients : refused)
    {
        WardRead read = readSealedTo(recipients);
        QVERIFY(read.status == WardReadStatus::Unreadable);
        QCOMPARE(read.problem, QString("the header and the access list differ"));
    }
}

void WardFileTest::accessListNamingADeviceTwiceIsRefused()
{
    QList<RecipientKeys> keys = m_payload.ward.access().recipients();
    QJsonObject payload = m_payload.toJson();
    QJsonObject alice = payload.value("access").toObject().value("users").toArray().at(0).toObject();
    QJsonArray devices = alice.value("devices").toArray();
    devices.append(devices[0]);
    QCOMPARE(devices.size(), qsizetype(3));
    QJsonObject repeated = WardFileEdits::edited(payload, {"access", "users", "0", "devices"}, devices);
    WardRead control = WardFile::read(WardFileEdits::sealedFile(m_document, m_dataKey, keys, payload), m_laptop,
                                      PayloadKind::LocalFile);
    QVERIFY2(control.status == WardReadStatus::Read, qPrintable(control.problem));

    WardRead read = WardFile::read(WardFileEdits::sealedFile(m_document, m_dataKey, keys, repeated), m_laptop,
                                   PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, QString("the header and the access list differ"));
    QVERIFY(!read.payload.has_value());
}

void WardFileTest::payloadThatIsNotCompressedJsonIsUnreadable()
{
    QList<RecipientKeys> keys = m_payload.ward.access().recipients();
    QByteArray compressedPayload = qCompress(QJsonDocument(m_payload.toJson()).toJson(QJsonDocument::Compact));
    WardRead control = WardFile::read(WardFileEdits::sealedPlaintext(m_document, m_dataKey, keys, compressedPayload),
                                      m_laptop, PayloadKind::LocalFile);
    QVERIFY2(control.status == WardReadStatus::Read, qPrintable(control.problem));

    const QList<QByteArray> plaintexts = {QByteArray("not compressed at all"), qCompress("[\"a JSON array\"]")};
    for (const QByteArray& plaintext : plaintexts)
    {
        QByteArray file = WardFileEdits::sealedPlaintext(m_document, m_dataKey, keys, plaintext);

        WardRead read = WardFile::read(file, m_laptop, PayloadKind::LocalFile);

        QVERIFY(read.status == WardReadStatus::Unreadable);
        QCOMPARE(read.problem, QString("the payload is not a compressed JSON object"));
        QVERIFY(read.header->document() == m_document);
        QVERIFY(!read.payload.has_value());
    }
}

void WardFileTest::headerNamingNoOneIsADeletedWard()
{
    WardRead read = readSealedTo({});

    QVERIFY(read.status == WardReadStatus::Deleted);
    QVERIFY(read.header->document() == m_document);
    QVERIFY(!read.payload.has_value());
}

void WardFileTest::headerNamingOthersButNotThisDeviceIsNotNamed()
{
    QByteArray file = written();
    Identity stranger = Identity::generate();

    WardRead read = WardFile::read(file, stranger, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::NotNamed);
    QVERIFY(!read.payload.has_value());
    QVERIFY(WardFile::read(file, m_laptop, PayloadKind::LocalFile).status == WardReadStatus::Read);
}

void WardFileTest::dataKeyThatDoesNotUnsealIsUnreadable()
{
    QByteArray file = written();
    QByteArray sealedToPhone = Crypto::seal(m_dataKey, m_phone.publicKey());
    QJsonObject header = WardFileEdits::edited(WardFileEdits::headerOf(file), {"recipients", "0", "sealedKey"},
                                               Base64Url::encode(sealedToPhone));

    WardRead read = WardFile::read(WardFileEdits::withHeader(file, header), m_laptop, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, QString("the data key does not unseal"));
}

void WardFileTest::fileWithNoHeaderLineIsUnreadable()
{
    QByteArray file = written();
    QByteArray oneLine = file.left(file.indexOf('\n'));

    WardRead read = WardFile::read(oneLine, m_laptop, PayloadKind::LocalFile);

    QVERIFY(read.status == WardReadStatus::Unreadable);
    QCOMPARE(read.problem, QString("the file has no header line"));
}
