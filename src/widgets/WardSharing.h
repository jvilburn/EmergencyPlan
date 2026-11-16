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

    /// Writes the invitation of invite, one of invitesMadeHere, where the user chooses, then
    /// shows what to do with it and the file ready to drag into an email: for a new user, the
    /// message to pass on to them (SendInvitationDialog). With the
    /// server in use the invitation carries the version the server holds, fetched once the
    /// invite's own version has reached it; otherwise it is written at once.
    static void saveInvitation(WardSession& session, const AppSettings& settings, WardSync& sync,
                               const QByteArray& invite, QWidget* parent);

private:
    /// Why no invitation can be made through the server now: the ward is no longer this
    /// device's, its latest change is not saved, there is no connection, or what
    /// whatStopsSharing says. Empty while the ward is syncing.
    static QString whatStopsInviting(const WardSession& session, const WardSync& sync);

    /// The invitation of invite carrying the server's version, awaited behind a small dialog.
    /// Empty when there is none, after telling the user why unless they closed the dialog.
    static QByteArray fetchedInvitation(WardSession& session, WardSync& sync, const QByteArray& invite,
                                        QWidget* parent);
};

/// The small dialog shown while an invitation is fetched from the server. It closes when the
/// invitation for its invite arrives, and Cancel gives up waiting.
class InvitationWait : public QDialog
{
    Q_OBJECT

public:
    InvitationWait(const QByteArray& invite, QWidget* parent);

    /// True once the server's answer for the invite has arrived.
    bool isAnswered() const { return m_answered; }

    /// The invitation's bytes; empty when there is none.
    const QByteArray& invitation() const { return m_invitation; }

public slots:
    void onInvitationFetched(const QByteArray& invite, const QByteArray& invitation);

private:
    QByteArray m_invite;
    QByteArray m_invitation;
    bool m_answered = false;
};
