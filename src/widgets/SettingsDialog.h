#pragma once

#include "Coordinates.h"

#include <QDialog>
#include <QString>

#include <optional>

class AppSettings;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// Settings: the server switch, this device's name, your name, your email and the map home, as
/// one short form holding the current values, and the wards folder, with a button that opens
/// it. It changes nothing itself; the caller reads what the user set once it is accepted.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// wardsFolder is the folder that holds this device's ward files. currentView is what the
    /// map shows, which Use current view takes as the map home.
    SettingsDialog(const AppSettings& settings, const QString& wardsFolder, const Coordinates& currentView,
                   QWidget* parent);

    bool useServer() const;

    /// This device's name, trimmed.
    QString deviceName() const;

    /// Your name, trimmed.
    QString userName() const;

    /// Your email, trimmed; empty when left empty.
    QString email() const;

    /// The map home Use current view set, or none when it was not pressed.
    std::optional<Coordinates> mapHome() const { return m_mapHome; }

private slots:
    void onUseCurrentView();

    /// OK is enabled while both names are not empty.
    void updateOkButton();

private:
    /// Shows home as the map home, or that there is none.
    void showMapHome(const std::optional<Coordinates>& home);

    Coordinates m_currentView;
    std::optional<Coordinates> m_mapHome;

    QCheckBox* m_useServer;
    QLineEdit* m_deviceName;
    QLineEdit* m_userName;
    QLineEdit* m_email;
    QLabel* m_mapHomeLabel;
    QPushButton* m_okButton;
};
