#pragma once

#include "AccessList.h"

#include <QByteArray>
#include <QDialog>
#include <QList>
#include <QMap>
#include <QString>

class AppSettings;
class QBoxLayout;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class WardSession;

/// What can be done to the selected row of Who has access.
enum class AccessAction
{
    Remove,            ///< remove the selected user
    MakeAdmin,         ///< make the selected member an admin
    MakeMember,        ///< make the selected admin a member
    RemoveDevice,      ///< remove another device of this device's user
    RemoveThisDevice,  ///< remove this device, which needs the server
    WithdrawInvite,    ///< withdraw the selected open invite
    SaveInvitation     ///< save the invitation of an open invite this device made
};

/// Who has access: the open ward's users, each with their role and devices, and its open
/// invites, rebuilt from the session's document whenever it changes, keeping the selection.
/// It offers each action on the selected row only where AccessRules allows it for this device,
/// and runs each change through DocumentManager's commit step. Below the list it offers Add a
/// user, or to a member the admins to ask, Add a device and Leave. A ward that is no longer this
/// device's shows the list with no action offered and those three disabled.
class WhoHasAccessDialog : public QDialog
{
    Q_OBJECT

public:
    /// session is the open ward's, and settings say whether the server is switched on.
    WhoHasAccessDialog(WardSession& session, const AppSettings& settings, QWidget* parent);

    /// The actions offered on the selected row, in AccessAction's order; none with nothing
    /// selected, or while the ward is no longer this device's.
    QList<AccessAction> offeredActions() const;

    /// What a member sees in place of Add a user; empty when Add a user is offered.
    QString addUserNotice() const;

    /// Makes an invite for the user named label, an admin when admin is set: its fingerprint
    /// when the invite applied; otherwise empty, and a blank label makes nothing.
    QByteArray addUser(const QString& label, bool admin);

private slots:
    /// Lists the session's access list again, keeping the selected row and the scroll.
    void rebuild();

    /// Shows the buttons offeredActions and addUserNotice allow.
    void updateActions();

    void onAddUser();
    void onAddDevice();
    void onLeave();
    void onRemoveThisDevice();
    void onSaveInvitation();

    /// Runs the command each of these makes from the selected row.
    void onRemove();
    void onMakeAdmin();
    void onMakeMember();
    void onRemoveDevice();
    void onWithdrawInvite();

private:
    /// What a row of the list is.
    enum class RowKind
    {
        Heading,  ///< the heading over the open invites, which cannot be selected
        User,     ///< its key is the user's id
        Device,   ///< its key is the device's fingerprint
        Invite    ///< its key is the invite's fingerprint
    };

    /// Adds a row under parent, or at the top level when parent is null.
    QTreeWidgetItem* addRow(QTreeWidgetItem* parent, const QString& text, RowKind kind, const QByteArray& key);

    /// Adds to layout a button for action that runs slot, shown while action is offered.
    void addActionButton(QBoxLayout* layout, AccessAction action, const QString& text,
                         void (WhoHasAccessDialog::*slot)());

    /// The selected row, or null when no row is selected.
    QTreeWidgetItem* selectedRow() const;

    /// The user of the selected row, which is a user's.
    AccessUser selectedUser() const;

    /// Writes the invitation of invite, one of invitesMadeHere, where the user chooses, shows
    /// the message to send with it, and then shows it in its folder.
    void saveInvitation(const QByteArray& invite);

    WardSession& m_session;
    const AppSettings& m_settings;

    QTreeWidget* m_list;
    QMap<AccessAction, QPushButton*> m_actionButtons;
    QWidget* m_wardActions;  ///< Add a user, Add a device and Leave, disabled while the ward is no longer this device's
    QPushButton* m_addUserButton;
    QLabel* m_addUserNotice;
};
