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

namespace
{

constexpr int PersonRole = Qt::UserRole;
constexpr int DeviceRole = Qt::UserRole + 1;
constexpr int InviteRole = Qt::UserRole + 2;

/// "2 people are named Bob Wilson" for each name more than one person shares, as a
/// reader would compare them: case and spacing aside.
QStringList sharedNames(const QList<Holder>& people)
{
    QHash<QString, int> counts;
    QHash<QString, QString> spelling;
    for (const Holder& person : people)
    {
        const QString key = person.name.simplified().toCaseFolded();
        counts[key] += 1;
        spelling.insert(key, person.name.simplified());
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

/// Case-insensitive by name, then by id, so two people sharing a name keep one order.
bool sortsBefore(const Holder& left, const Holder& right)
{
    const int byName = QString::compare(left.name, right.name, Qt::CaseInsensitive);
    return byName != 0 ? byName < 0 : left.id.toHex() < right.id.toHex();
}

QString roleText(Role role)
{
    return role == Role::Admin ? QObject::tr("Admin") : QObject::tr("Member");
}

} // namespace

AccessDialog::AccessDialog(const SealingSet& roster, const DeviceIdentity& self, bool shared, QWidget* parent)
    : QDialog(parent)
    , m_roster(roster)
    , m_self(self)
    , m_shared(shared)
{
    setWindowTitle(tr("Who Has Access"));
    resize(620, 440);

    QVBoxLayout* layout = new QVBoxLayout(this);

    if (!m_shared)
    {
        QHBoxLayout* top = new QHBoxLayout();
        QLabel* privateNote = new QLabel(tr("Only you can open this ward. Share it to add people."), this);
        QPushButton* share = new QPushButton(tr("Share..."), this);
        top->addWidget(privateNote, 1);
        top->addWidget(share);
        layout->addLayout(top);
        connect(share, &QPushButton::clicked, this, &AccessDialog::shareRequested);
    }

    m_sharedNames = new QLabel(this);
    m_sharedNames->setWordWrap(true);
    layout->addWidget(m_sharedNames);

    m_askAdmins = new QLabel(this);
    m_askAdmins->setWordWrap(true);
    layout->addWidget(m_askAdmins);

    QHBoxLayout* body = new QHBoxLayout();
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Person, device, or invite"), tr("Role")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    body->addWidget(m_tree, 1);

    QVBoxLayout* buttons = new QVBoxLayout();
    m_addDevice = new QPushButton(tr("Add a Device..."), this);
    m_addUser = new QPushButton(tr("Add a User..."), this);
    m_role = new QPushButton(tr("Make Admin"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_unlink = new QPushButton(tr("Unlink"), this);
    m_withdraw = new QPushButton(tr("Withdraw Invite"), this);
    m_leave = new QPushButton(tr("Leave..."), this);
    m_details = new QPushButton(tr("Details"), this);
    for (QPushButton* button : {m_addDevice, m_addUser, m_role, m_remove, m_unlink, m_withdraw, m_leave, m_details})
    {
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    body->addLayout(buttons);
    layout->addLayout(body, 1);

    QDialogButtonBox* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(close);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &AccessDialog::onSelectionChanged);
    connect(m_addDevice, &QPushButton::clicked, this, &AccessDialog::addDeviceRequested);
    connect(m_addUser, &QPushButton::clicked, this, &AccessDialog::addUserRequested);
    connect(m_leave, &QPushButton::clicked, this, &AccessDialog::leaveRequested);
    connect(m_unlink, &QPushButton::clicked, this, &AccessDialog::onUnlink);
    connect(m_withdraw, &QPushButton::clicked, this, &AccessDialog::onWithdraw);
    connect(m_role, &QPushButton::clicked, this, &AccessDialog::onRole);
    connect(m_remove, &QPushButton::clicked, this, &AccessDialog::onRemove);
    connect(m_details, &QPushButton::clicked, this, &AccessDialog::onDetails);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fillTree();
}

void AccessDialog::setRoster(const SealingSet& roster)
{
    // Kept across the refill by what it names, not by row.
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    const QString person = selected.isEmpty() ? QString() : selected.first()->data(0, PersonRole).toString();
    const QString device = selected.isEmpty() ? QString() : selected.first()->data(0, DeviceRole).toString();
    const QString invite = selected.isEmpty() ? QString() : selected.first()->data(0, InviteRole).toString();

    m_roster = roster;
    fillTree();

    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        if ((*it)->data(0, PersonRole).toString() == person && (*it)->data(0, DeviceRole).toString() == device
            && (*it)->data(0, InviteRole).toString() == invite)
        {
            (*it)->setSelected(true);
        }
    }
}

void AccessDialog::fillTree()
{
    const QList<Holder> people = m_roster.people();
    const QStringList warnings = sharedNames(people);
    m_sharedNames->setText(tr("%1 Check each is who you expect.").arg(warnings.join(" ")));
    m_sharedNames->setVisible(!warnings.isEmpty());

    QStringList admins;
    for (const Holder& person : people)
    {
        if (person.isAdmin())
        {
            admins.append(person.name);
        }
    }
    m_askAdmins->setText(tr("To add or remove people, ask an admin: %1.").arg(QLocale().createSeparatedList(admins)));
    m_askAdmins->setVisible(m_shared && !selfIsAdmin());

    // Inviting and leaving belong to a shared ward; an unshared one has only you.
    m_addDevice->setVisible(m_shared);
    m_addUser->setVisible(m_shared && selfIsAdmin());
    m_leave->setVisible(m_shared);
    m_remove->setVisible(m_shared && selfIsAdmin());
    m_role->setVisible(m_shared && selfIsAdmin());
    m_unlink->setVisible(m_shared);
    m_withdraw->setVisible(m_shared);

    QList<Holder> sorted = people;
    std::sort(sorted.begin(), sorted.end(), &sortsBefore);

    m_tree->clear();
    for (const Holder& person : sorted)
    {
        const bool isSelf = self().has_value() && self()->id == person.id;
        const QString name = isSelf ? tr("%1 (you)").arg(person.name) : person.name;
        QTreeWidgetItem* row = new QTreeWidgetItem(m_tree, {name, roleText(person.role)});
        row->setData(0, PersonRole, person.id.toHex());

        for (const NamedDevice& device : person.devices)
        {
            const bool thisDevice = device.fingerprint() == m_self.fingerprint();
            const QString deviceName = thisDevice ? tr("%1 (this device)").arg(device.name) : device.name;
            QTreeWidgetItem* deviceRow = new QTreeWidgetItem(row, {deviceName, QString()});
            deviceRow->setData(0, PersonRole, person.id.toHex());
            deviceRow->setData(0, DeviceRole, device.fingerprint().toHex());
        }

        // Under its maker, with the role a new user joins as.
        for (const Invite& invite : m_roster.invites())
        {
            if (invite.maker != person.id)
            {
                continue;
            }
            const bool forDevice = invite.purpose == Invite::For::Device;
            const QString what = forDevice ? tr("Invite for a device") : tr("Invite for a new user");
            QTreeWidgetItem* inviteRow =
                new QTreeWidgetItem(row, {tr("%1: %2").arg(what, invite.label), forDevice ? tr("Open") : roleText(invite.role)});
            inviteRow->setData(0, InviteRole, invite.fingerprint().toHex());
        }
    }

    m_tree->expandAll();
    onSelectionChanged();
}

std::optional<Holder> AccessDialog::self() const
{
    return m_roster.personOf(m_self.fingerprint());
}

bool AccessDialog::selfIsAdmin() const
{
    const std::optional<Holder> me = self();
    return me.has_value() && me->isAdmin();
}

std::optional<Holder> AccessDialog::selectedPerson() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty())
    {
        return std::nullopt;
    }
    return m_roster.person(MemberId::fromHex(selected.first()->data(0, PersonRole).toString()));
}

std::optional<NamedDevice> AccessDialog::selectedDevice() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty() || selected.first()->data(0, DeviceRole).toString().isEmpty())
    {
        return std::nullopt;
    }
    return m_roster.device(Fingerprint::fromHex(selected.first()->data(0, DeviceRole).toString()));
}

std::optional<Invite> AccessDialog::selectedInvite() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty() || selected.first()->data(0, InviteRole).toString().isEmpty())
    {
        return std::nullopt;
    }
    return m_roster.invite(Fingerprint::fromHex(selected.first()->data(0, InviteRole).toString()));
}

void AccessDialog::onSelectionChanged()
{
    const std::optional<Holder> person = selectedPerson();
    const std::optional<NamedDevice> device = selectedDevice();
    const std::optional<Invite> invite = selectedInvite();
    const bool isSelf = person.has_value() && self().has_value() && person->id == self()->id;
    const bool admin = selfIsAdmin();

    m_details->setEnabled(person.has_value());

    // Removing and changing roles are an admin's, on someone else's own row.
    const bool personRow = person.has_value() && !device.has_value();
    m_remove->setEnabled(admin && personRow && !isSelf);

    // Make Member on yourself steps down - never as the only admin, since a ward
    // always has one.
    m_role->setToolTip(QString());
    m_role->setText(person.has_value() && person->isAdmin() ? tr("Make Member") : tr("Make Admin"));
    if (personRow && (isSelf ? person->isAdmin() : admin))
    {
        const bool blocked = isSelf && m_roster.adminCount() == 1;
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
    m_unlink->setEnabled(isSelf && device.has_value() && device->fingerprint() != m_self.fingerprint());

    // An invite is withdrawn by whoever made it, or by an admin.
    m_withdraw->setEnabled(invite.has_value() && (admin || (self().has_value() && invite->maker == self()->id)));
}

void AccessDialog::onRemove()
{
    const std::optional<Holder> person = selectedPerson();
    if (person.has_value())
    {
        emit removeRequested(person->id);
    }
}

void AccessDialog::onRole()
{
    const std::optional<Holder> person = selectedPerson();
    if (person.has_value())
    {
        emit roleRequested(person->id, person->isAdmin() ? Role::Member : Role::Admin);
    }
}

void AccessDialog::onUnlink()
{
    const std::optional<NamedDevice> device = selectedDevice();
    if (device.has_value())
    {
        emit unlinkRequested(device->fingerprint());
    }
}

void AccessDialog::onWithdraw()
{
    const std::optional<Invite> invite = selectedInvite();
    if (invite.has_value())
    {
        emit withdrawRequested(invite->fingerprint());
    }
}

void AccessDialog::onDetails()
{
    const std::optional<Holder> person = selectedPerson();
    if (!person.has_value())
    {
        return;
    }

    QStringList lines;
    lines.append(tr("%1 (%2)").arg(person->name, roleText(person->role)));
    for (const NamedDevice& device : person->devices)
    {
        lines.append(tr("%1: %2").arg(device.name, device.fingerprint().toGroupedHex()));
    }
    QMessageBox::information(this, tr("Details"), lines.join("\n"));
}
