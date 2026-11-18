#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QDialog>
#include <QString>

class AppSettings;
class QWidget;
class WardSession;
class WardSync;

/// The sharing actions on the open ward as a whole, which the Sharing menu and Who has access
/// both offer. Each shows its dialogs over parent.
class WardSharing
{
    Q_DECLARE_TR_FUNCTIONS(WardSharing)

public:
    /// What the window says stops the open ward from sharing through the server: that a newer
    /// Ward Planning wrote the server's version, that the server's version could not be read,
    /// that this device's changes could not be shared, or that it is working offline. Empty
    /// when nothing does.
    static QString whatStopsSharing(const WardSync& sync);

    /// Makes an invite for the user named label, an admin when admin is set: its fingerprint
    /// when the invite applied; otherwise empty, and a blank label makes nothing.
    static QByteArray inviteUser(WardSession& session, const QString& label, bool admin);

    /// Asks who the invitation is for, makes the invite and saves its invitation. With the
    /// server in use it is offered only while the ward is syncing, and otherwise says what
    /// stops it.
    static void addUser(WardSession& session, const AppSettings& settings, WardSync& sync, QWidget* parent);

    /// Makes an invite for another device of this device's user and saves its invitation,
    /// offered as addUser is.
    static void addDevice(WardSession& session, const AppSettings& settings, WardSync& sync, QWidget* parent);

    /// Tells the user what leaving the ward needs.
    static void leaveWard(const AppSettings& settings, QWidget* parent);

    /// Hands the user the invitation of invite, one of invitesMadeHere. With the server in
    /// use it is a link, shown with the message to pass on once the invite's own version has
    /// reached the server (SendLinkDialog). Otherwise it is a file, written where the user
    /// chooses and then shown ready to drag into an email (SendInvitationDialog for a new
    /// user).
    static void saveInvitation(WardSession& session, const AppSettings& settings, WardSync& sync,
                               const QByteArray& invite, QWidget* parent);

private:
    /// Why no invitation can be made through the server now: the ward is no longer this
    /// device's, its latest change is not saved, there is no connection, or what
    /// whatStopsSharing says. Empty while the ward is syncing.
    static QString whatStopsInviting(const WardSession& session, const WardSync& sync);

    /// Shows the link of invite with the message to pass on.
    static void showLink(WardSession& session, const QByteArray& invite, QWidget* parent);

    /// Waits, behind a small dialog, to hear that the server's version names invite, which is
    /// when its link works. False when it does not, after telling the user why unless they
    /// gave up waiting.
    static bool inviteReachedServer(WardSession& session, WardSync& sync, const QByteArray& invite, QWidget* parent);
};

/// The small dialog shown until the server is seen to hold a new invite. It closes when the
/// answer for its invite arrives, and Cancel gives up waiting.
class InvitationWait : public QDialog
{
    Q_OBJECT

public:
    InvitationWait(const QByteArray& invite, QWidget* parent);

    /// True once the server's answer for the invite has arrived.
    bool isAnswered() const { return m_answered; }

    /// True when the answer was that the server's version names the invite.
    bool isOnServer() const { return m_onServer; }

public slots:
    void onInviteConfirmed(const QByteArray& invite, bool onServer);

private:
    QByteArray m_invite;
    bool m_answered = false;
    bool m_onServer = false;
};
