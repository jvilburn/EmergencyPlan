#include "SettingsDialogTest.h"
#include "AppSettings.h"
#include "Coordinates.h"
#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

SettingsDialogTest::SettingsDialogTest(QObject* parent)
    : QObject(parent)
{
}

void SettingsDialogTest::okIsEnabledOnlyWhileBothNamesAreNotEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath("settings.json"));
    settings.setDeviceName("Laptop");
    settings.setUserName("Alice Smith");

    SettingsDialog dialog(settings, Coordinates(40.2338, -111.6585), nullptr);
    QPushButton* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QList<QLineEdit*> names = dialog.findChildren<QLineEdit*>();
    QCOMPARE(names.size(), qsizetype(2));
    QLineEdit* deviceName = names[0]->text() == "Laptop" ? names[0] : names[1];
    QLineEdit* userName = names[0]->text() == "Laptop" ? names[1] : names[0];
    QCOMPARE(userName->text(), QString("Alice Smith"));
    QVERIFY(ok->isEnabled());

    deviceName->clear();
    QVERIFY(!ok->isEnabled());

    deviceName->setText("Laptop");
    QVERIFY(ok->isEnabled());
    userName->clear();
    QVERIFY(!ok->isEnabled());
}
