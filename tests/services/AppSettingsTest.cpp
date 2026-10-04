#include "AppSettingsTest.h"

#include "AppSettings.h"
#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

using namespace SessionTestSupport;

namespace
{

std::unique_ptr<AppSettings> openAt(const QString& path)
{
    return AppSettings::open(path, nullptr);
}

/// A shared ward's record, with every field set.
DocumentRecord fullRecord()
{
    DocumentRecord record;
    record.baseVersion = 12;
    record.baseHash = DocumentCrypto::payloadHash("a base copy");
    record.highWaterMark.keyGeneration = 3;
    record.highWaterMark.saveCounter = 41;
    return record;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

void AppSettingsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void AppSettingsTest::aMissingFileIsAnEmptyStore()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");

    QString error;
    const std::unique_ptr<AppSettings> settings = AppSettings::open(path, &error);
    QVERIFY(settings);
    QVERIFY(error.isEmpty());
    QVERIFY(settings->documentIds().isEmpty());
    QVERIFY(!settings->developerExport());
    QVERIFY(!settings->defaultLocation().has_value());
    QVERIFY(!settings->writeFailed());
    QVERIFY(settings->record("anything") == DocumentRecord());

    // Reading writes nothing.
    QVERIFY(!QFile::exists(path));
}

void AppSettingsTest::everyValueReadsBackThroughASecondInstance()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("nested/settings.json");
    const QString documentId = ChainTestSupport::newDocumentId();

    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setDeveloperExport(true);
        settings->setServerAddressOverride("https://example.test");
        settings->setDefaultLocation(AppSettings::Location{40.5, -111.25});
        settings->setRecord(documentId, fullRecord());
        QVERIFY(!settings->writeFailed());
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread);
    QVERIFY(reread->developerExport());
    QCOMPARE(reread->serverAddress(), QString("https://example.test"));
    QCOMPARE(reread->defaultLocation()->latitude, 40.5);
    QCOMPARE(reread->defaultLocation()->longitude, -111.25);
    QVERIFY(reread->record(documentId) == fullRecord());
    QCOMPARE(reread->documentIds(), QStringList{documentId});

    // Cleared, each falls back: the override to the built-in address, the toggle off.
    reread->setServerAddressOverride(QString());
    reread->setDeveloperExport(false);
    const std::unique_ptr<AppSettings> cleared = openAt(path);
    QVERIFY(cleared);
    QVERIFY(cleared->serverAddress() != QString("https://example.test"));
    QVERIFY(!cleared->developerExport());
}

void AppSettingsTest::numbersRoundTripAsNumbers()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString documentId = ChainTestSupport::newDocumentId();

    DocumentRecord record = fullRecord();
    record.baseVersion = DocumentHeader::MaxSaveCounter;
    record.highWaterMark.saveCounter = DocumentHeader::MaxSaveCounter;
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setDefaultLocation(AppSettings::Location{40.123456789012, -111.987654321098});
        settings->setRecord(documentId, record);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QCOMPARE(reread->defaultLocation()->latitude, 40.123456789012);
    QCOMPARE(reread->defaultLocation()->longitude, -111.987654321098);
    QVERIFY(reread->record(documentId) == record);

    // Numbers in the file, not strings.
    const QJsonObject json = QJsonDocument::fromJson(readFile(path)).object();
    QVERIFY(json["map"].toObject()["defaultLat"].isDouble());
    const QJsonObject stored = json["records"].toObject()[documentId].toObject();
    QVERIFY(stored["baseVersion"].isDouble());
    QVERIFY(stored["highWaterMark"].toObject()["saveCounter"].isDouble());
}

void AppSettingsTest::recordsForTwoIdsStaySeparate()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString first = ChainTestSupport::newDocumentId();
    const QString second = ChainTestSupport::newDocumentId();

    DocumentRecord other;
    other.highWaterMark.keyGeneration = 1;
    other.highWaterMark.saveCounter = 2;
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setRecord(first, fullRecord());
        settings->setRecord(second, other);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread->record(first) == fullRecord());
    QVERIFY(reread->record(second) == other);
}

void AppSettingsTest::aRecordWithPendingRosterChangesRoundTrips()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString documentId = ChainTestSupport::newDocumentId();
    const DeviceIdentity bob = newIdentity("Bob", "Phone");

    RosterChange grant;
    grant.kind = RosterChange::Kind::Grant;
    grant.person = bob.asNewPerson();
    RosterChange unlink;
    unlink.kind = RosterChange::Kind::Unlink;
    unlink.device = bob.asRosterDevice();
    RosterChange role;
    role.kind = RosterChange::Kind::SetRole;
    role.target = bob.member;
    role.role = RosterPerson::Role::Admin;

    DocumentRecord record = fullRecord();
    record.pendingChanges = {grant, unlink, role};
    openAt(path)->setRecord(documentId, record);

    const DocumentRecord reread = openAt(path)->record(documentId);
    QCOMPARE(reread.pendingChanges.size(), qsizetype(3));
    QVERIFY(reread == record);
}

void AppSettingsTest::removingARecordLeavesTheOthers()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString gone = ChainTestSupport::newDocumentId();
    const QString kept = ChainTestSupport::newDocumentId();
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setRecord(gone, fullRecord());
        settings->setRecord(kept, fullRecord());
        settings->setServerAddressOverride("https://kept.test");
        settings->removeRecord(gone);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QCOMPARE(reread->documentIds(), QStringList{kept});
    QVERIFY(reread->record(kept) == fullRecord());
    QVERIFY(reread->record(gone) == DocumentRecord());
    QCOMPARE(reread->serverAddress(), QString("https://kept.test"));
}

void AppSettingsTest::aMalformedFileIsReportedAndLeftAlone_data()
{
    const QString id = ChainTestSupport::newDocumentId();
    const QString hash = HexCodec::encode(DocumentCrypto::payloadHash("a base copy"));
    const QString mark = R"("highWaterMark": {"keyGeneration": 1, "saveCounter": 2})";

    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("not json") << QByteArray("{ not json");
    QTest::newRow("not an object") << QByteArray("[]");
    QTest::newRow("no records") << QByteArray(R"({"serverAddress": "https://example.test"})");
    QTest::newRow("unknown key") << QByteArray(R"({"records": {}, "favouriteColour": "blue"})");
    QTest::newRow("developer export false") << QByteArray(R"({"records": {}, "developerExport": false})");
    QTest::newRow("empty server address") << QByteArray(R"({"records": {}, "serverAddress": ""})");
    QTest::newRow("latitude as text") << QByteArray(R"({"records": {}, "map": {"defaultLat": "40", "defaultLng": 1}})");
    QTest::newRow("empty document id") << QString(R"({"records": {"": {%1, "pendingChanges": []}}})").arg(mark).toUtf8();
    QTest::newRow("record not an object") << QString(R"({"records": {"%1": 7}})").arg(id).toUtf8();
    QTest::newRow("no high-water mark") << QString(R"({"records": {"%1": {"pendingChanges": []}}})").arg(id).toUtf8();
    QTest::newRow("base version without hash")
        << QString(R"({"records": {"%1": {"baseVersion": 3, %2, "pendingChanges": []}}})").arg(id, mark).toUtf8();
    QTest::newRow("hash of the wrong size")
        << QString(R"({"records": {"%1": {"baseVersion": 3, "baseHash": "abcd", %2, "pendingChanges": []}}})")
               .arg(id, mark)
               .toUtf8();
    QTest::newRow("a pending change of no kind")
        << QString(R"({"records": {"%1": {%2, "pendingChanges": [{"kind": "promote"}]}}})").arg(id, mark).toUtf8();

    // Anchor: the same shapes, well formed, read.
    QTest::newRow("well formed") << QString(R"({"records": {"%1": {"baseVersion": 3, "baseHash": "%2", %3, "pendingChanges": []}}})")
                                        .arg(id, hash, mark)
                                        .toUtf8();
}

void AppSettingsTest::aMalformedFileIsReportedAndLeftAlone()
{
    QFETCH(QByteArray, bytes);

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    QVERIFY(writeFile(path, bytes));

    QString error;
    const std::unique_ptr<AppSettings> settings = AppSettings::open(path, &error);
    if (QByteArray(QTest::currentDataTag()) == "well formed")
    {
        QVERIFY2(settings, qPrintable(error));
        return;
    }

    QVERIFY(!settings);
    QVERIFY(!error.isEmpty());

    // Reported, not replaced: the file is as it was.
    QCOMPARE(readFile(path), bytes);
}

void AppSettingsTest::aFailedWriteKeepsTheValuesAndSaysSo()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    // The settings file's folder is a file, so nothing can be written there.
    const QString blocker = folder.filePath("blocker");
    QVERIFY(writeFile(blocker, "a file, not a folder"));
    const std::unique_ptr<AppSettings> settings = openAt(blocker + "/settings.json");
    QVERIFY(settings);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Settings could not be written"));
    settings->setServerAddressOverride("https://example.test");
    QVERIFY(settings->writeFailed());
    QCOMPARE(settings->serverAddress(), QString("https://example.test"));
    QVERIFY(!QFile::exists(blocker + "/settings.json"));
}
