#include "AppSettingsTest.h"
#include "AppSettings.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

AppSettingsTest::AppSettingsTest(QObject* parent)
    : QObject(parent)
{
}

void AppSettingsTest::missingFileReadsAsEveryDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AppSettings missing(dir.filePath("settings.json"));
    QVERIFY(!missing.useServer());
    QCOMPARE(missing.deviceName(), QString());
    QCOMPARE(missing.userName(), QString());
    QCOMPARE(missing.email(), QString());
    QVERIFY(!missing.lastWard().has_value());
    QVERIFY(!missing.mapHome().has_value());
}

void AppSettingsTest::aHandWrittenFileReadsEveryKey()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"useServer":true,"deviceName":"Laptop","userName":"Alice Smith","email":"alice@example.com",)"
               R"("lastWardId":"5f1c2a7e-3b9d-4e8a-9c61-0d2f7b4a8e13","defaultLat":40.2338,"defaultLng":-111.6585})");
    file.close();

    AppSettings written(path);
    QVERIFY(written.useServer());
    QCOMPARE(written.deviceName(), QString("Laptop"));
    QCOMPARE(written.userName(), QString("Alice Smith"));
    QCOMPARE(written.email(), QString("alice@example.com"));
    QVERIFY(written.lastWard() == DocumentId::fromString("5f1c2a7e-3b9d-4e8a-9c61-0d2f7b4a8e13"));
    QVERIFY(written.mapHome() == Coordinates(40.2338, -111.6585));
}

void AppSettingsTest::eachValueSurvivesANewSettingsOnTheSameFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("settings.json");
    DocumentId document = DocumentId::generate();

    AppSettings first(path);
    first.setUseServer(true);
    first.setDeviceName("Laptop");
    first.setUserName("Alice Smith");
    first.setEmail("alice@example.com");
    first.setLastWard(document);
    first.setMapHome(Coordinates(40.2338, -111.6585));

    AppSettings second(path);
    QVERIFY(second.useServer());
    QCOMPARE(second.deviceName(), QString("Laptop"));
    QCOMPARE(second.userName(), QString("Alice Smith"));
    QCOMPARE(second.email(), QString("alice@example.com"));
    QVERIFY(second.lastWard() == document);
    QVERIFY(second.mapHome() == Coordinates(40.2338, -111.6585));
}
