#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

class AppSettings;
class QWidget;
class WardSession;

/// The sharing actions on the open ward as a whole, which the Sharing menu and Who has access
/// both offer. Each shows its dialogs over parent.
class WardSharing
{
    Q_DECLARE_TR_FUNCTIONS(WardSharing)

public:
    /// Makes an invite for the user named label, an admin when admin is set: its fingerprint
    /// when the invite applied; otherwise empty, and a blank label makes nothing.
    static QByteArray inviteUser(WardSession& session, const QString& label, bool admin);

    /// Asks who the invitation is for, makes the invite and saves its invitation.
    static void addUser(WardSession& session, const AppSettings& settings, QWidget* parent);

    /// Makes an invite for another device of this device's user and saves its invitation.
    static void addDevice(WardSession& session, const AppSettings& settings, QWidget* parent);

    /// Tells the user what leaving the ward needs.
    static void leaveWard(const AppSettings& settings, QWidget* parent);

    /// Writes the invitation of invite, one of invitesMadeHere, where the user chooses, then
    /// shows the message to send with it and the file ready to drag into an email.
    static void saveInvitation(WardSession& session, const QByteArray& invite, QWidget* parent);
};
