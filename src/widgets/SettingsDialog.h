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

/// Settings: the server switch, this device's name, your name and the map home, as one short
/// form holding the current values. It changes nothing itself; the caller reads what the user
/// set once it is accepted.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// currentView is what the map shows, which Use current view takes as the map home.
    SettingsDialog(const AppSettings& settings, const Coordinates& currentView, QWidget* parent);

    bool useServer() const;

    /// This device's name, trimmed.
    QString deviceName() const;

    /// Your name, trimmed.
    QString userName() const;

    /// The map home Use current view set, or none when it was not pressed.
    std::optional<Coordinates> mapHome() const { return m_mapHome; }

private slots:
    void onUseCurrentView();

    /// OK is enabled while this device's name is not empty, and your name is not empty or was
    /// empty when the dialog opened.
    void updateOkButton();

private:
    /// Shows home as the map home, or that there is none.
    void showMapHome(const std::optional<Coordinates>& home);

    Coordinates m_currentView;
    bool m_userNameWasEmpty;
    std::optional<Coordinates> m_mapHome;

    QCheckBox* m_useServer;
    QLineEdit* m_deviceName;
    QLineEdit* m_userName;
    QLabel* m_mapHomeLabel;
    QPushButton* m_okButton;
};
