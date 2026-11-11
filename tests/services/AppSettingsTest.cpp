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
    QVERIFY(!missing.lastWard().has_value());
    QVERIFY(!missing.mapHome().has_value());

    QString path = dir.filePath("written.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"deviceName":"Laptop"})");
    file.close();
    AppSettings written(path);
    QCOMPARE(written.deviceName(), QString("Laptop"));
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
    first.setLastWard(document);
    first.setMapHome(Coordinates(40.2338, -111.6585));

    AppSettings second(path);
    QVERIFY(second.useServer());
    QCOMPARE(second.deviceName(), QString("Laptop"));
    QCOMPARE(second.userName(), QString("Alice Smith"));
    QVERIFY(second.lastWard() == document);
    QVERIFY(second.mapHome() == Coordinates(40.2338, -111.6585));
}
