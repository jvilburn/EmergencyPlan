#pragma once

#include "DeviceIdentity.h"
#include "ServerContract.h"

#include <QDialog>
#include <QList>

#include <optional>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/// Who has access to the open ward, as the server last said: each person with their
/// role, this person marked, each expandable to their devices by name, and which
/// device linked each one and when. Every action is a visible button, enabled only
/// where it applies:
///
/// - for an admin of a shared ward: Invite Link, the link to send someone, and Grant
///   or Decline on each request waiting; anyone sees the requests
/// - a request whose name matches a member is flagged: it is more likely that
///   member's new device, which they should link instead
/// - on another person, for an admin: Remove, and Make Admin or Make Member
/// - on this person: Link a Device, Unlink on each of their devices but this one,
///   Make Member for an admin stepping down (not the last admin while anyone else
///   remains), and Leave
/// - Details for anyone, revealing device fingerprints
///
/// A member sees whom to ask: the ward's admins. People sharing a display name are
/// flagged, since the admin who added the second may be who the warning is for.
///
/// The dialog decides nothing itself: each action is a signal.
class AccessDialog : public QDialog
{
    Q_OBJECT

public:
    /// members is empty for a ward not shared, or one whose server has not answered.
    AccessDialog(const QList<ServerContract::Member>& members, const DeviceIdentity& self, bool shared, QWidget* parent);

    /// The members after a head check while the dialog is open; the selection is kept.
    void setMembers(const QList<ServerContract::Member>& members);

    /// The ward's pending access requests, each with Grant and Decline for an admin.
    void setRequests(const QList<ServerContract::AccessRequest>& requests);

signals:
    void shareRequested();
    void inviteLinkRequested();
    void linkRequested();
    void leaveRequested();
    void removeRequested(const MemberId& member);
    void roleRequested(const MemberId& member, ServerContract::Role role);
    void unlinkRequested(const Fingerprint& device);
    void grantRequested(const ServerContract::AccessRequest& request);
    void declineRequested(const ServerContract::AccessRequest& request);

private slots:
    void onSelectionChanged();
    void onRemove();
    void onRole();
    void onUnlink();
    void onDetails();
    void onGrant();
    void onDecline();

private:
    void fillTree();

    /// The person a row is about, and the device when it is a device's row.
    std::optional<ServerContract::Member> selectedMember() const;
    std::optional<ServerContract::Device> selectedDevice() const;
    std::optional<ServerContract::Member> member(const MemberId& id) const;

    bool selfIsAdmin() const;
    bool isOnlyAdmin() const;
    QString linkedText(const ServerContract::Person& person, const ServerContract::Device& device) const;
    std::optional<ServerContract::AccessRequest> selectedRequest() const;
    void updateRequestButtons();

    QList<ServerContract::Member> m_members;
    QList<ServerContract::AccessRequest> m_requests;
    DeviceIdentity m_self;
    bool m_shared;

    QTreeWidget* m_tree = nullptr;
    QLabel* m_sharedNames = nullptr;
    QLabel* m_askAdmins = nullptr;
    QLabel* m_offline = nullptr;
    QPushButton* m_inviteLink = nullptr;
    QLabel* m_requestsTitle = nullptr;
    QTreeWidget* m_requestList = nullptr;
    QLabel* m_requestWarning = nullptr;
    QPushButton* m_grant = nullptr;
    QPushButton* m_decline = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_role = nullptr;
    QPushButton* m_unlink = nullptr;
    QPushButton* m_details = nullptr;
    QPushButton* m_link = nullptr;
    QPushButton* m_leave = nullptr;
};
