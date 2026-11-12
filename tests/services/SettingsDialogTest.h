#pragma once

#include <QObject>

/// Settings enables OK only while both this device's name and your name are not empty, and
/// whether or not your email is.
class SettingsDialogTest : public QObject
{
    Q_OBJECT

public:
    explicit SettingsDialogTest(QObject* parent);

private slots:
    void okIsEnabledOnlyWhileBothNamesAreNotEmpty();
};
