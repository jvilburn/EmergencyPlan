#include "AccessDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

using ServerContract::Device;
using ServerContract::Member;
using ServerContract::Role;

namespace
{

constexpr int MemberRole = Qt::UserRole;
constexpr int DeviceRole = Qt::UserRole + 1;

/// "2 people are named Bob Wilson" for each name more than one person shares, as a
/// reader would compare them: case and spacing aside.
QStringList sharedNames(const QList<Member>& members)
{
    QHash<QString, int> counts;
    QHash<QString, QString> spelling;
    for (const Member& member : members)
    {
        const QString key = member.person.displayName.simplified().toCaseFolded();
        counts[key] += 1;
        spelling.insert(key, member.person.displayName.simplified());
    }

    QStringList warnings;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
    {
        if (it.value() > 1)
        {
            warnings.append(QObject::tr("%n people are named %1.", "", it.value()).arg(spelling.value(it.key())));
        }
    }
    warnings.sort();
    return warnings;
}

/// Case-insensitive by name, then by member id, so two people sharing a name keep
/// one order.
bool sortsBefore(const Member& left, const Member& right)
{
    const int byName = QString::compare(left.person.displayName, right.person.displayName, Qt::CaseInsensitive);
    return byName != 0 ? byName < 0 : left.person.member.toHex() < right.person.member.toHex();
}

} // namespace

AccessDialog::AccessDialog(const QList<Member>& members, const DeviceIdentity& self, bool shared, QWidget* parent)
    : QDialog(parent)
    , m_members(members)
    , m_self(self)
    , m_shared(shared)
{
    setWindowTitle(tr("Who Has Access"));
    resize(620, 440);

    QVBoxLayout* layout = new QVBoxLayout(this);

    if (!m_shared)
    {
        QHBoxLayout* top = new QHBoxLayout();
        QLabel* privateNote = new QLabel(tr("Only you can open this ward."), this);
        QPushButton* share = new QPushButton(tr("Share..."), this);
        top->addWidget(privateNote, 1);
        top->addWidget(share);
        layout->addLayout(top);
        connect(share, &QPushButton::clicked, this, &AccessDialog::shareRequested);
    }
    else
    {
        QHBoxLayout* top = new QHBoxLayout();
        top->addStretch(1);
        m_inviteLink = new QPushButton(tr("Invite Link..."), this);
        top->addWidget(m_inviteLink);
        layout->addLayout(top);
        connect(m_inviteLink, &QPushButton::clicked, this, &AccessDialog::inviteLinkRequested);
    }

    m_offline = new QLabel(tr("Who has access shows once the server answers."), this);
    m_offline->setWordWrap(true);
    layout->addWidget(m_offline);

    m_sharedNames = new QLabel(this);
    m_sharedNames->setWordWrap(true);
    layout->addWidget(m_sharedNames);

    m_askAdmins = new QLabel(this);
    m_askAdmins->setWordWrap(true);
    layout->addWidget(m_askAdmins);

    QHBoxLayout* body = new QHBoxLayout();
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Person or device"), tr("Role, or linked by")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    body->addWidget(m_tree, 1);

    QVBoxLayout* buttons = new QVBoxLayout();
    m_link = new QPushButton(tr("Link a Device..."), this);
    m_unlink = new QPushButton(tr("Unlink"), this);
    m_role = new QPushButton(tr("Make Admin"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_leave = new QPushButton(tr("Leave..."), this);
    m_details = new QPushButton(tr("Details"), this);
    for (QPushButton* button : {m_link, m_unlink, m_role, m_remove, m_leave, m_details})
    {
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    body->addLayout(buttons);
    layout->addLayout(body, 1);

    // Requests waiting, under the people who have access.
    m_requestsTitle = new QLabel(tr("Asking for access"), this);
    layout->addWidget(m_requestsTitle);
    QHBoxLayout* requestRow = new QHBoxLayout();
    m_requestList = new QTreeWidget(this);
    m_requestList->setColumnCount(3);
    m_requestList->setHeaderLabels({tr("Person"), tr("Device"), tr("Asked")});
    m_requestList->setRootIsDecorated(false);
    m_requestList->setMaximumHeight(120);
    requestRow->addWidget(m_requestList, 1);
    QVBoxLayout* requestButtons = new QVBoxLayout();
    m_grant = new QPushButton(tr("Grant"), this);
    m_decline = new QPushButton(tr("Decline"), this);
    requestButtons->addWidget(m_grant);
    requestButtons->addWidget(m_decline);
    requestButtons->addStretch(1);
    requestRow->addLayout(requestButtons);
    layout->addLayout(requestRow);
    m_requestWarning = new QLabel(this);
    m_requestWarning->setWordWrap(true);
    layout->addWidget(m_requestWarning);

    QDialogButtonBox* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(close);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &AccessDialog::onSelectionChanged);
    connect(m_link, &QPushButton::clicked, this, &AccessDialog::linkRequested);
    connect(m_leave, &QPushButton::clicked, this, &AccessDialog::leaveRequested);
    connect(m_unlink, &QPushButton::clicked, this, &AccessDialog::onUnlink);
    connect(m_role, &QPushButton::clicked, this, &AccessDialog::onRole);
    connect(m_remove, &QPushButton::clicked, this, &AccessDialog::onRemove);
    connect(m_details, &QPushButton::clicked, this, &AccessDialog::onDetails);
    connect(m_requestList, &QTreeWidget::itemSelectionChanged, this, &AccessDialog::updateRequestButtons);
    connect(m_grant, &QPushButton::clicked, this, &AccessDialog::onGrant);
    connect(m_decline, &QPushButton::clicked, this, &AccessDialog::onDecline);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fillTree();
    setRequests({});
}

void AccessDialog::setRequests(const QList<ServerContract::AccessRequest>& requests)
{
    // Kept across the refill by the request's index.
    const QList<QTreeWidgetItem*> selected = m_requestList->selectedItems();
    const QVariant selectedIndex = selected.isEmpty() ? QVariant() : selected.first()->data(0, Qt::UserRole);

    m_requests = requests;
    m_requestList->clear();
    for (const ServerContract::AccessRequest& request : requests)
    {
        const QString device = request.person.devices.isEmpty() ? QString() : request.person.devices.first().name;
        const QString asked = QLocale().toString(request.when.toLocalTime(), QLocale::ShortFormat);
        QTreeWidgetItem* row = new QTreeWidgetItem(m_requestList, {request.person.displayName, device, asked});
        row->setData(0, Qt::UserRole, request.index);
        row->setSelected(selectedIndex.isValid() && selectedIndex.toLongLong() == request.index);
    }

    const bool any = !requests.isEmpty();
    m_requestsTitle->setVisible(any);
    m_requestList->setVisible(any);
    m_grant->setVisible(any && selfIsAdmin());
    m_decline->setVisible(any && selfIsAdmin());
    updateRequestButtons();
}

void AccessDialog::setMembers(const QList<Member>& members)
{
    // Kept across the refill by what it names, not by row.
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    const QString member = selected.isEmpty() ? QString() : selected.first()->data(0, MemberRole).toString();
    const QString device = selected.isEmpty() ? QString() : selected.first()->data(0, DeviceRole).toString();

    m_members = members;
    fillTree();
    setRequests(m_requests);

    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        if ((*it)->data(0, MemberRole).toString() == member && (*it)->data(0, DeviceRole).toString() == device)
        {
            (*it)->setSelected(true);
        }
    }
}

void AccessDialog::fillTree()
{
    const bool known = !m_members.isEmpty();
    m_offline->setVisible(m_shared && !known);

    const QStringList warnings = sharedNames(m_members);
    m_sharedNames->setText(tr("%1 Check each is who you expect.").arg(warnings.join(" ")));
    m_sharedNames->setVisible(!warnings.isEmpty());

    QStringList admins;
    for (const Member& member : m_members)
    {
        if (member.isAdmin())
        {
            admins.append(member.person.displayName);
        }
    }
    m_askAdmins->setText(tr("To add or remove people, ask an admin: %1.").arg(QLocale().createSeparatedList(admins)));
    m_askAdmins->setVisible(known && !selfIsAdmin());

    // Linking and leaving belong to a shared ward the server has answered for; an
    // unshared one has only you.
    m_link->setVisible(m_shared && known);
    m_leave->setVisible(m_shared && known);
    m_unlink->setVisible(m_shared && known);
    m_remove->setVisible(selfIsAdmin());
    m_role->setVisible(selfIsAdmin());
    if (m_inviteLink != nullptr)
    {
        m_inviteLink->setVisible(selfIsAdmin());
    }

    QList<Member> sorted = m_members;
    std::sort(sorted.begin(), sorted.end(), &sortsBefore);

    m_tree->clear();
    for (const Member& member : sorted)
    {
        const ServerContract::Person& person = member.person;
        const bool isSelf = person.member == m_self.member;
        const QString name = isSelf ? tr("%1 (you)").arg(person.displayName) : person.displayName;
        QTreeWidgetItem* row = new QTreeWidgetItem(m_tree, {name, member.isAdmin() ? tr("Admin") : tr("Member")});
        row->setData(0, MemberRole, person.member.toHex());

        for (const Device& device : person.devices)
        {
            const bool thisDevice = device.fingerprint == m_self.fingerprint();
            const QString deviceName = thisDevice ? tr("%1 (this device)").arg(device.name) : device.name;
            QTreeWidgetItem* deviceRow = new QTreeWidgetItem(row, {deviceName, linkedText(person, device)});
            deviceRow->setData(0, MemberRole, person.member.toHex());
            deviceRow->setData(0, DeviceRole, device.fingerprint.toHex());
        }
    }
    m_tree->expandAll();
    onSelectionChanged();
}

QString AccessDialog::linkedText(const ServerContract::Person& person, const Device& device) const
{
    if (!device.linkedBy.has_value())
    {
        return tr("First device");
    }

    const QString when = QLocale().toString(device.linkedAt.toLocalTime().date(), QLocale::ShortFormat);
    for (const Device& linker : person.devices)
    {
        if (linker.fingerprint == device.linkedBy.value())
        {
            return tr("Linked by %1, %2").arg(linker.name, when);
        }
    }
    return tr("Linked by a former device, %1").arg(when);
}

std::optional<Member> AccessDialog::member(const MemberId& id) const
{
    for (const Member& member : m_members)
    {
        if (member.person.member == id)
        {
            return member;
        }
    }
    return std::nullopt;
}

std::optional<Member> AccessDialog::selectedMember() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty())
    {
        return std::nullopt;
    }
    return member(MemberId::fromHex(selected.first()->data(0, MemberRole).toString()));
}

std::optional<Device> AccessDialog::selectedDevice() const
{
    const std::optional<Member> person = selectedMember();
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (!person.has_value() || selected.first()->data(0, DeviceRole).toString().isEmpty())
    {
        return std::nullopt;
    }

    const QString fingerprint = selected.first()->data(0, DeviceRole).toString();
    for (const Device& device : person.value().person.devices)
    {
        if (device.fingerprint.toHex() == fingerprint)
        {
            return device;
        }
    }
    return std::nullopt;
}

bool AccessDialog::selfIsAdmin() const
{
    const std::optional<Member> self = member(m_self.member);
    return self.has_value() && self.value().isAdmin();
}

bool AccessDialog::isOnlyAdmin() const
{
    return selfIsAdmin()
        && std::count_if(m_members.cbegin(), m_members.cend(), [](const Member& member) { return member.isAdmin(); }) == 1;
}

void AccessDialog::onSelectionChanged()
{
    const std::optional<Member> person = selectedMember();
    const std::optional<Device> device = selectedDevice();
    const bool isSelf = person.has_value() && person.value().person.member == m_self.member;
    const bool admin = selfIsAdmin();

    m_details->setEnabled(person.has_value());

    // Removing and changing roles are an admin's, on someone else.
    m_remove->setEnabled(admin && person.has_value() && !isSelf);

    // Make Member on yourself steps down - never as the only admin, since a ward
    // always has one.
    m_role->setToolTip(QString());
    const bool personRow = person.has_value() && !device.has_value();
    m_role->setText(person.has_value() && person.value().isAdmin() ? tr("Make Member") : tr("Make Admin"));
    if (personRow && (isSelf ? person.value().isAdmin() : admin))
    {
        const bool blocked = isSelf && isOnlyAdmin();
        m_role->setEnabled(!blocked);
        if (blocked)
        {
            m_role->setToolTip(tr("A ward needs an admin. Make someone else an admin first."));
        }
    }
    else
    {
        m_role->setEnabled(false);
    }

    // Unlink: your own devices but this one. Another person's lost device is theirs
    // to unlink, or an admin removes the person.
    m_unlink->setEnabled(isSelf && device.has_value() && device.value().fingerprint != m_self.fingerprint());
}

void AccessDialog::onRemove()
{
    const std::optional<Member> person = selectedMember();
    if (person.has_value())
    {
        emit removeRequested(person.value().person.member);
    }
}

void AccessDialog::onRole()
{
    const std::optional<Member> person = selectedMember();
    if (person.has_value())
    {
        emit roleRequested(person.value().person.member, person.value().isAdmin() ? Role::Member : Role::Admin);
    }
}

void AccessDialog::onUnlink()
{
    const std::optional<Device> device = selectedDevice();
    if (device.has_value())
    {
        emit unlinkRequested(device.value().fingerprint);
    }
}

std::optional<ServerContract::AccessRequest> AccessDialog::selectedRequest() const
{
    const QList<QTreeWidgetItem*> selected = m_requestList->selectedItems();
    if (selected.isEmpty())
    {
        return std::nullopt;
    }
    const qint64 index = selected.first()->data(0, Qt::UserRole).toLongLong();
    for (const ServerContract::AccessRequest& request : m_requests)
    {
        if (request.index == index)
        {
            return request;
        }
    }
    return std::nullopt;
}

void AccessDialog::updateRequestButtons()
{
    const std::optional<ServerContract::AccessRequest> request = selectedRequest();
    m_grant->setEnabled(request.has_value());
    m_decline->setEnabled(request.has_value());

    // Granting a second Bob gives him none of the first Bob's devices and a role of
    // his own.
    QString warning;
    if (request.has_value())
    {
        const QString asked = request->person.displayName.simplified().toCaseFolded();
        for (const Member& member : m_members)
        {
            if (member.person.displayName.simplified().toCaseFolded() == asked)
            {
                warning = tr("%1 already has access. If this is %1's new device, %1 should link it from their "
                             "other device instead.")
                              .arg(member.person.displayName);
                break;
            }
        }
    }
    m_requestWarning->setText(warning);
    m_requestWarning->setVisible(!warning.isEmpty());
}

void AccessDialog::onGrant()
{
    const std::optional<ServerContract::AccessRequest> request = selectedRequest();
    if (request.has_value())
    {
        emit grantRequested(request.value());
    }
}

void AccessDialog::onDecline()
{
    const std::optional<ServerContract::AccessRequest> request = selectedRequest();
    if (request.has_value())
    {
        emit declineRequested(request.value());
    }
}

void AccessDialog::onDetails()
{
    const std::optional<Member> person = selectedMember();
    if (!person.has_value())
    {
        return;
    }

    QStringList lines;
    lines.append(tr("%1 (%2)").arg(person.value().person.displayName,
                                   person.value().isAdmin() ? tr("Admin") : tr("Member")));
    for (const Device& device : person.value().person.devices)
    {
        lines.append(tr("%1: %2").arg(device.name, device.fingerprint.toGroupedHex()));
    }
    QMessageBox::information(this, tr("Details"), lines.join("\n"));
}
