#include "AppSettingsTest.h"

#include "AppSettings.h"
#include "ServerTestSupport.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimeZone>
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
    // One roster for every call, so two records from it compare equal.
    static const QJsonObject roster =
        rosterOf({newIdentity("Alice", "Laptop"), newIdentity("Bob", "Phone")}).toJson();

    DocumentRecord record;
    record.baseVersion = 12;
    record.baseHash = DocumentCrypto::payloadHash("a base copy");
    record.baseSaveCounter = 41;
    record.rosterShown = roster;
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
    const QString documentId = ServerTestSupport::newDocumentId();

    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setDeveloperExport(true);
        settings->setServerAddressOverride("https://example.test");
        settings->setDefaultLocation(AppSettings::Location{40.5, -111.25});
        settings->setRecord(documentId, fullRecord());
        settings->setUpdateCheck(AppSettings::UpdateCheck{QDateTime(QDate(2026, 10, 12), QTime(14, 30), QTimeZone::UTC), "1.4.0"});
        settings->setDismissedUpdate("1.3.0");
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
    QCOMPARE(reread->updateCheck()->when, QDateTime(QDate(2026, 10, 12), QTime(14, 30), QTimeZone::UTC));
    QCOMPARE(reread->updateCheck()->latestVersion, QString("1.4.0"));
    QCOMPARE(reread->dismissedUpdate(), QString("1.3.0"));

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
    const QString documentId = ServerTestSupport::newDocumentId();

    DocumentRecord record = fullRecord();
    record.baseVersion = DocumentHeader::MaxSaveCounter;
    record.baseSaveCounter = DocumentHeader::MaxSaveCounter;
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
    QVERIFY(stored["baseSaveCounter"].isDouble());
}

void AppSettingsTest::recordsForTwoIdsStaySeparate()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString first = ServerTestSupport::newDocumentId();
    const QString second = ServerTestSupport::newDocumentId();

    DocumentRecord other;
    other.baseSaveCounter = 1;
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setRecord(first, fullRecord());
        settings->setRecord(second, other);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread->record(first) == fullRecord());
    QVERIFY(reread->record(second) == other);
}

void AppSettingsTest::removingARecordLeavesTheOthers()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString gone = ServerTestSupport::newDocumentId();
    const QString kept = ServerTestSupport::newDocumentId();
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

void AppSettingsTest::wardEntriesReadBackThroughASecondInstance()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString opened = ServerTestSupport::newDocumentId();
    const QString fresh = ServerTestSupport::newDocumentId();

    AppSettings::WardEntry openedWard;
    openedWard.wardName = "Tanglewood Ward";
    openedWard.lastOpened = QDateTime(QDate(2026, 10, 6), QTime(9, 30, 15, 250), QTimeZone::UTC);
    openedWard.copies = {{"C:/Users/someone/Desktop/Tanglewood.wardplanning", ServerTestSupport::newDocumentId()},
                         {"E:/Tanglewood.wardplanning", ServerTestSupport::newDocumentId()}};

    // A ward not yet named, never opened, with no copies.
    const AppSettings::WardEntry freshWard;
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setWard(opened, openedWard);
        settings->setWard(fresh, freshWard);
        settings->setLastWardId(opened);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread);
    QVERIFY(reread->ward(opened) == openedWard);
    QVERIFY(reread->ward(fresh) == freshWard);
    QVERIFY(!reread->ward(fresh)->wardName.has_value());
    QVERIFY(!reread->ward(fresh)->lastOpened.has_value());
    QCOMPARE(reread->lastWardId(), opened);
    QStringList ids = reread->wardIds();
    ids.sort();
    QStringList expected{opened, fresh};
    expected.sort();
    QCOMPARE(ids, expected);

    // Wards and records are separate: a ward alone has no record.
    QVERIFY(reread->documentIds().isEmpty());
}

void AppSettingsTest::removingAWardLeavesItsRecordAndClearsTheLastWard()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath("settings.json");
    const QString gone = ServerTestSupport::newDocumentId();
    const QString kept = ServerTestSupport::newDocumentId();

    AppSettings::WardEntry ward;
    ward.wardName = "Tanglewood Ward";
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setWard(gone, ward);
        settings->setWard(kept, ward);
        settings->setRecord(gone, fullRecord());
        settings->setLastWardId(kept);

        // Removing another ward leaves the last ward.
        settings->removeWard(gone);
        QCOMPARE(settings->lastWardId(), kept);

        settings->removeWard(kept);
        QVERIFY(settings->lastWardId().isEmpty());
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread);
    QVERIFY(reread->wardIds().isEmpty());
    QVERIFY(reread->lastWardId().isEmpty());
    QVERIFY(reread->record(gone) == fullRecord());
}

void AppSettingsTest::aMalformedFileIsReportedAndLeftAlone_data()
{
    const QString id = ServerTestSupport::newDocumentId();
    const QString hash = HexCodec::encode(DocumentCrypto::payloadHash("a base copy"));
    const QString mark = R"("baseSaveCounter": 2)";

    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("not json") << QByteArray("{ not json");
    QTest::newRow("not an object") << QByteArray("[]");
    QTest::newRow("no records") << QByteArray(R"({"wards": {}, "serverAddress": "https://example.test"})");
    QTest::newRow("no wards") << QByteArray(R"({"records": {}, "serverAddress": "https://example.test"})");
    QTest::newRow("unknown key") << QByteArray(R"({"records": {}, "wards": {}, "favouriteColour": "blue"})");
    QTest::newRow("developer export false") << QByteArray(R"({"records": {}, "wards": {}, "developerExport": false})");
    QTest::newRow("empty server address") << QByteArray(R"({"records": {}, "wards": {}, "serverAddress": ""})");
    QTest::newRow("latitude as text")
        << QByteArray(R"({"records": {}, "wards": {}, "map": {"defaultLat": "40", "defaultLng": 1}})");
    QTest::newRow("empty document id")
        << QString(R"({"records": {"": {%1}}, "wards": {}})").arg(mark).toUtf8();
    QTest::newRow("record not an object") << QString(R"({"records": {"%1": 7}, "wards": {}})").arg(id).toUtf8();
    QTest::newRow("no base save counter")
        << QString(R"({"records": {"%1": {"rosterShown": {}}}, "wards": {}})").arg(id).toUtf8();
    QTest::newRow("roster shown not an object")
        << QString(R"({"records": {"%1": {"rosterShown": [], %2}}, "wards": {}})").arg(id, mark).toUtf8();
    QTest::newRow("a record key no build writes")
        << QString(R"({"records": {"%1": {"historySeen": 0, %2}}, "wards": {}})").arg(id, mark).toUtf8();
    QTest::newRow("base version without hash")
        << QString(R"({"records": {"%1": {"baseVersion": 3, %2}}, "wards": {}})").arg(id, mark).toUtf8();
    QTest::newRow("hash of the wrong size")
        << QString(R"({"records": {"%1": {"baseVersion": 3, "baseHash": "abcd", %2}}, "wards": {}})")
               .arg(id, mark)
               .toUtf8();
    QTest::newRow("linked devices, which no build reads")
        << QByteArray(R"({"records": {}, "wards": {}, "linkedDevices": []})");

    QTest::newRow("ward not an object") << QString(R"({"records": {}, "wards": {"%1": 7}})").arg(id).toUtf8();
    QTest::newRow("empty ward id") << QByteArray(R"({"records": {}, "wards": {"": {"wardName": "Tanglewood Ward"}}})");
    QTest::newRow("empty ward name") << QString(R"({"records": {}, "wards": {"%1": {"wardName": ""}}})").arg(id).toUtf8();
    QTest::newRow("unknown ward key")
        << QString(R"({"records": {}, "wards": {"%1": {"wardName": "Tanglewood Ward", "colour": "blue"}}})").arg(id).toUtf8();
    QTest::newRow("last opened not a time")
        << QString(R"({"records": {}, "wards": {"%1": {"wardName": "Tanglewood Ward", "lastOpened": "yesterday"}}})")
               .arg(id)
               .toUtf8();
    QTest::newRow("empty copies")
        << QString(R"({"records": {}, "wards": {"%1": {"wardName": "Tanglewood Ward", "copies": []}}})").arg(id).toUtf8();
    QTest::newRow("a copy without its id")
        << QString(R"({"records": {}, "wards": {"%1": {"copies": [{"path": "E:/Tanglewood.wardplanning"}]}}})")
               .arg(id)
               .toUtf8();
    QTest::newRow("a copy with an empty path")
        << QString(R"({"records": {}, "wards": {"%1": {"copies": [{"path": "", "documentId": "%1"}]}}})").arg(id).toUtf8();
    QTest::newRow("empty last ward")
        << QString(R"({"records": {}, "wards": {"%1": {"wardName": "Tanglewood Ward"}}, "lastWard": ""})").arg(id).toUtf8();

    // Anchor: the same shapes, well formed, read.
    QTest::newRow("well formed")
        << QString(R"({"records": {"%1": {"baseVersion": 3, "baseHash": "%2", %3}},)"
                   R"( "wards": {"%1": {"wardName": "Tanglewood Ward", "lastOpened": "2026-10-06T09:30:15.250Z",)"
                   R"( "copies": [{"path": "E:/Tanglewood.wardplanning", "documentId": "%4"}]}, "%4": {}},)"
                   R"( "lastWard": "%1"})")
               .arg(id, hash, mark, ServerTestSupport::newDocumentId())
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
