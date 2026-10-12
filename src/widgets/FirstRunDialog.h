#pragma once

#include "ServerContract.h"

#include <QDialog>
#include <QTimer>

#include <memory>

class IdentityService;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class ServerConnection;

/// The first screen, while no person exists on this device. It asks one question -
/// does this person already use Ward Planning on another device - before anything
/// else, so one person never becomes two by accident.
///
/// No asks for a display name and a device name and creates the person. Yes names
/// this device, shows a link code to enter on the other device, and waits: once that
/// device approves it, this one joins that person, as the server says.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    /// How often the link is asked about while it waits for approval.
    static constexpr int LinkCheckMilliseconds = 3 * 1000;

    FirstRunDialog(IdentityService& identity, ServerContract::ServerApi& server, QWidget* parent);
    ~FirstRunDialog() override;

private slots:
    void onNewPerson();
    void onLinkDevice();
    void onBack();
    void onCreate();
    void onNamesChanged();
    void onShowCode();

private:
    void onCode(const ServerContract::Result<QString>& code);
    void checkLink();
    void onLink(const ServerContract::Result<ServerContract::LinkState>& state);
    void linkFailed(const QString& message);

    IdentityService& m_identity;
    ServerContract::ServerApi& m_server;
    std::unique_ptr<ServerConnection> m_connection;
    QString m_code;
    QTimer m_linkCheck;

    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_displayName = nullptr;
    QLineEdit* m_deviceName = nullptr;
    QPushButton* m_createButton = nullptr;
    QLineEdit* m_linkDeviceName = nullptr;
    QPushButton* m_showCode = nullptr;
    QLabel* m_codeLabel = nullptr;
    QLabel* m_linkStatus = nullptr;
};
