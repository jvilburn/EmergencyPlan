#pragma once

#include "AccessEvents.h"
#include "AccessRoster.h"
#include "DeviceIdentity.h"

#include <QDialog>
#include <QHash>

#include <optional>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/// Who has access to the open ward: each person with their role, this person
/// marked, each expandable to their devices by name, and which device added each
/// one and when. Every action is a visible button, enabled only where it applies:
///
/// - on another person, for an admin: Remove, Make Admin or Make Member, and Unlink
///   on each of their devices but the last
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
    AccessDialog(const AccessRoster& roster, const DeviceIdentity& self, bool shared, QWidget* parent);

    /// Which device added each device, once the kept headers have been read; when
    /// they could not be checked, the dialog says so instead.
    void setOrigins(const QHash<QString, AccessEvents::Origin>& origins, bool checked);

    /// The roster after a sync while the dialog is open; the selection is kept.
    void setRoster(const AccessRoster& roster);

signals:
    void shareRequested();
    void inviteRequested();
    void linkRequested();
    void leaveRequested();
    void removeRequested(const MemberId& member);
    void roleRequested(const MemberId& member, RosterPerson::Role role);
    void unlinkRequested(const MemberId& member, const Fingerprint& device);

private slots:
    void onSelectionChanged();
    void onRemove();
    void onRole();
    void onUnlink();
    void onDetails();

private:
    void fillTree();

    /// The person a row is about, and the device when it is a device's row.
    std::optional<RosterPerson> selectedPerson() const;
    std::optional<RosterDevice> selectedDevice() const;

    bool selfIsAdmin() const;
    bool isOnlyAdmin() const;
    void updateAddedColumn();
    QString addedText(const RosterDevice& device) const;

    AccessRoster m_roster;
    DeviceIdentity m_self;
    bool m_shared;
    QHash<QString, AccessEvents::Origin> m_origins;

    QTreeWidget* m_tree = nullptr;
    QLabel* m_sharedNames = nullptr;
    QLabel* m_originsUnchecked = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_role = nullptr;
    QPushButton* m_unlink = nullptr;
    QPushButton* m_details = nullptr;
    QPushButton* m_link = nullptr;
    QPushButton* m_leave = nullptr;
};
