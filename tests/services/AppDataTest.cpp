#include "AppDataTest.h"
#include "AppData.h"

#include <QTest>

AppDataTest::AppDataTest(QObject* parent)
    : QObject(parent)
{
}

void AppDataTest::wardsAndKeysAreSiblingFoldersOfTheRoot()
{
    AppData appData("C:/data/Ward Planning");

    QCOMPARE(appData.wardsDirectory(), QString("C:/data/Ward Planning/wards"));
    QCOMPARE(appData.keysDirectory(), QString("C:/data/Ward Planning/keys"));
    QCOMPARE(appData.keyFilePath(), QString("C:/data/Ward Planning/keys/device.key"));
    QVERIFY(!appData.keyFilePath().startsWith(appData.wardsDirectory() + "/"));
}
