#pragma once

#include "DeviceIdentity.h"
#include "SealingSet.h"

#include <QDialog>

#include <optional>

class QLabel;
class QPushButton;
class QTreeWidget;

/// Who has access to the open ward, read from the ward itself: each person with
/// their role, this person marked, each expandable to their devices by name, and
/// each invite still open. Every action is a visible button, enabled only where it
/// applies:
///
/// - Add a Device, for anyone: an invite that joins this person
/// - Add a User, for an admin: an invite that makes a new member or admin
/// - Leave, for anyone
/// - on another person, for an admin: Remove, and Make Admin or Make Member
/// - on this person: Make Member for an admin stepping down (never the last admin)
/// - on one of this person's own devices but this one: Unlink
/// - on an open invite: Withdraw
/// - Details for anyone, revealing device fingerprints
///
/// A member sees whom to ask: the ward's admins. The role is the app's rule, kept by
/// every honest copy of it; the server never sees it. People sharing a display name
/// are flagged, since the admin who added the second may be who the warning is for.
///
/// The dialog decides nothing itself: each action is a signal.
class AccessDialog : public QDialog
{
    Q_OBJECT

public:
    AccessDialog(const SealingSet& roster, const DeviceIdentity& self, bool shared, QWidget* parent);

    /// The roster after a change while the dialog is open; the selection is kept.
    void setRoster(const SealingSet& roster);

signals:
    void shareRequested();
    void addDeviceRequested();
    void addUserRequested();
    void leaveRequested();
    void removeRequested(const MemberId& person);
    void roleRequested(const MemberId& person, Role role);
    void unlinkRequested(const Fingerprint& device);
    void withdrawRequested(const Fingerprint& invite);

private slots:
    void onSelectionChanged();
    void onRemove();
    void onRole();
    void onUnlink();
    void onWithdraw();
    void onDetails();

private:
    void fillTree();

    /// The person a person's or device's row is about, the device when it is a
    /// device's row, and the invite when it is an invite's row.
    std::optional<Holder> selectedPerson() const;
    std::optional<NamedDevice> selectedDevice() const;
    std::optional<Invite> selectedInvite() const;

    std::optional<Holder> self() const;
    bool selfIsAdmin() const;

    SealingSet m_roster;
    DeviceIdentity m_self;
    bool m_shared;

    QTreeWidget* m_tree = nullptr;
    QLabel* m_sharedNames = nullptr;
    QLabel* m_askAdmins = nullptr;
    QPushButton* m_addDevice = nullptr;
    QPushButton* m_addUser = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_role = nullptr;
    QPushButton* m_unlink = nullptr;
    QPushButton* m_withdraw = nullptr;
    QPushButton* m_details = nullptr;
    QPushButton* m_leave = nullptr;
};
