#include "AppSettingsTest.h"

#include "AppSettings.h"
#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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

} // namespace

void AppSettingsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void AppSettingsTest::aMissingFileIsAnEmptyStore()
{
    QTemporaryDir folder;
    const QString path = folder.filePath("settings.json");

    QString error;
    const std::unique_ptr<AppSettings> settings = AppSettings::open(path, &error);
    QVERIFY(settings);
    QVERIFY(error.isEmpty());
    QVERIFY(settings->documentIds().isEmpty());
    QVERIFY(settings->displayName().isEmpty());
    QVERIFY(!settings->developerExport());
    QVERIFY(!settings->defaultLocation().has_value());
    QVERIFY(settings->lastDocumentId().isEmpty());
    QVERIFY(settings->record("anything") == DocumentRecord());

    // Reading writes nothing.
    QVERIFY(!QFile::exists(path));
}

void AppSettingsTest::everyValueReadsBackThroughASecondInstance()
{
    QTemporaryDir folder;
    const QString path = folder.filePath("nested/settings.json");
    const QString documentId = ChainTestSupport::newDocumentId();
    const QDateTime opened = QDateTime::fromString("2026-10-04T15:30:12.345Z", Qt::ISODateWithMs);

    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setDisplayName("Alice Adams");
        settings->setDeveloperExport(true);
        settings->setServerAddressOverride("https://example.test");
        settings->setDefaultLocation(AppSettings::Location{40.5, -111.25});
        settings->setRecord(documentId, fullRecord());
        settings->setWardName(documentId, "Tanglewood Ward");
        settings->setLastOpened(documentId, opened);
        settings->setCopyPaths(documentId, {"C:/copies/one.wardplanning", "C:/copies/two.wardplanning"});
        settings->setLastDocumentId(documentId);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread);
    QCOMPARE(reread->displayName(), QString("Alice Adams"));
    QVERIFY(reread->developerExport());
    QCOMPARE(reread->serverAddress(), QString("https://example.test"));
    QCOMPARE(reread->defaultLocation()->latitude, 40.5);
    QCOMPARE(reread->defaultLocation()->longitude, -111.25);
    QVERIFY(reread->record(documentId) == fullRecord());
    QCOMPARE(reread->wardName(documentId), QString("Tanglewood Ward"));
    QVERIFY(reread->lastOpened(documentId) == opened);
    QCOMPARE(reread->copyPaths(documentId),
             (QStringList{"C:/copies/one.wardplanning", "C:/copies/two.wardplanning"}));
    QCOMPARE(reread->lastDocumentId(), documentId);
    QCOMPARE(reread->documentIds(), QStringList{documentId});

    // Clearing the override falls back to the built-in address.
    reread->setServerAddressOverride(QString());
    QVERIFY(openAt(path)->serverAddress() != QString("https://example.test"));
}

void AppSettingsTest::numbersRoundTripAsNumbers()
{
    QTemporaryDir folder;
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
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(json["map"].toObject()["defaultLat"].isDouble());
    const QJsonObject stored = json["documents"].toObject()[documentId].toObject()["record"].toObject();
    QVERIFY(stored["baseVersion"].isDouble());
    QVERIFY(stored["highWaterMark"].toObject()["saveCounter"].isDouble());
}

void AppSettingsTest::recordsForTwoIdsStaySeparate()
{
    QTemporaryDir folder;
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
        settings->setWardName(first, "First Ward");
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread->record(first) == fullRecord());
    QVERIFY(reread->record(second) == other);
    QCOMPARE(reread->wardName(first), QString("First Ward"));
    QVERIFY(reread->wardName(second).isEmpty());
}

void AppSettingsTest::aRecordWithPendingRosterChangesRoundTrips()
{
    QTemporaryDir folder;
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

void AppSettingsTest::removingARecordForgetsTheWholeDocument()
{
    QTemporaryDir folder;
    const QString path = folder.filePath("settings.json");
    const QString documentId = ChainTestSupport::newDocumentId();
    {
        std::unique_ptr<AppSettings> settings = openAt(path);
        settings->setRecord(documentId, fullRecord());
        settings->setWardName(documentId, "Gone Ward");
        settings->setCopyPaths(documentId, {"C:/copies/one.wardplanning"});
        settings->setLastDocumentId(documentId);
        settings->removeRecord(documentId);
    }

    const std::unique_ptr<AppSettings> reread = openAt(path);
    QVERIFY(reread->documentIds().isEmpty());
    QVERIFY(reread->wardName(documentId).isEmpty());
    QVERIFY(reread->copyPaths(documentId).isEmpty());
    QVERIFY(reread->lastDocumentId().isEmpty());
}

void AppSettingsTest::aMalformedFileIsReportedAndLeftAlone()
{
    QTemporaryDir folder;
    const QString path = folder.filePath("settings.json");

    // Anchor: a well-formed file opens.
    openAt(path)->setDisplayName("Alice");
    QVERIFY(openAt(path));

    const QList<QByteArray> broken = {
        "{ not json",
        "[]",
        R"({"documents": {}, "favouriteColour": "blue"})",
        R"({"documents": {}, "map": {"defaultLat": "40"}})",
        R"({"documents": {"abc": {"record": {"highWaterMark": {"keyGeneration": 1}, "pendingChanges": []}}}})",
    };
    for (const QByteArray& bytes : broken)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(bytes);
        file.close();

        QString error;
        QVERIFY2(!AppSettings::open(path, &error), bytes.constData());
        QVERIFY(!error.isEmpty());

        // Reported, not replaced: the file is as it was.
        QFile check(path);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), bytes);
    }
}
