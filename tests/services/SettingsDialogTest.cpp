#include "SettingsDialogTest.h"
#include "AppSettings.h"
#include "Coordinates.h"
#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QHash>
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
    settings.setEmail("alice@example.com");

    SettingsDialog dialog(settings, Coordinates(40.2338, -111.6585), nullptr);
    QPushButton* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QHash<QString, QLineEdit*> fields;
    const QList<QLineEdit*> edits = dialog.findChildren<QLineEdit*>();
    for (QLineEdit* edit : edits)
    {
        fields.insert(edit->text(), edit);
    }
    QCOMPARE(fields.size(), qsizetype(3));
    QLineEdit* deviceName = fields.value("Laptop");
    QLineEdit* userName = fields.value("Alice Smith");
    QLineEdit* email = fields.value("alice@example.com");
    QVERIFY(deviceName && userName && email);
    QVERIFY(ok->isEnabled());

    email->clear();
    QVERIFY(ok->isEnabled());
    QCOMPARE(dialog.email(), QString());

    deviceName->clear();
    QVERIFY(!ok->isEnabled());

    deviceName->setText("Laptop");
    QVERIFY(ok->isEnabled());
    userName->clear();
    QVERIFY(!ok->isEnabled());
}
