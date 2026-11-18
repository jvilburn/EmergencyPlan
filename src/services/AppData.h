#pragma once

#include <QString>

/// Where Ward Planning keeps its files: the ward files, the device keys, the settings, the
/// instance lock and an invitation waiting to be opened, in local, non-roaming app data. The ward files and the keys are sibling
/// folders, so the folder that holds the ward files holds no key.
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
    QString lockFilePath() const { return m_root + "/instance.lock"; }

    /// The code of an invitation link, left by the installer the link's page downloads, which
    /// reads it from its own file name (installer.iss).
    QString invitationFilePath() const { return m_root + "/invitation"; }

private:
    QString m_root;
};
