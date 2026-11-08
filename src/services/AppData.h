#pragma once

#include <QString>

/// Where Ward Planning keeps its files: the ward files, the device keys and the settings, in
/// local, non-roaming app data. The ward files and the keys are sibling folders, so the folder
/// that holds the ward files holds no key.
class AppData
{
public:
    explicit AppData(const QString& root);

    /// The app's own: QStandardPaths::AppLocalDataLocation.
    static AppData standard();

    QString wardsDirectory() const { return m_root + "/wards"; }
    QString keysDirectory() const { return m_root + "/keys"; }
    QString keyFilePath() const { return keysDirectory() + "/device.key"; }
    QString settingsFilePath() const { return m_root + "/settings.json"; }

private:
    QString m_root;
};
