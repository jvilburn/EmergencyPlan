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

namespace
{

constexpr int MemberRole = Qt::UserRole;
constexpr int DeviceRole = Qt::UserRole + 1;

/// "2 people are named Bob Wilson" for each name more than one person shares, as a
/// reader would compare them: case and spacing aside.
QStringList sharedNames(const QList<RosterPerson>& people)
{
    QHash<QString, int> counts;
    QHash<QString, QString> spelling;
    for (const RosterPerson& person : people)
    {
        const QString key = person.displayName().simplified().toCaseFolded();
        counts[key] += 1;
        spelling.insert(key, person.displayName().simplified());
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

} // namespace

AccessDialog::AccessDialog(const AccessRoster& roster, const DeviceIdentity& self, bool shared, QWidget* parent)
    : QDialog(parent)
    , m_roster(roster)
    , m_self(self)
    , m_shared(shared)
{
    setWindowTitle(tr("Who Has Access"));
    resize(620, 440);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // At the top: sharing a ward that is not yet shared, or inviting, for admins.
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
    else if (selfIsAdmin())
    {
        QHBoxLayout* top = new QHBoxLayout();
        top->addStretch(1);
        QPushButton* invite = new QPushButton(tr("Invite Someone..."), this);
        top->addWidget(invite);
        layout->addLayout(top);
        connect(invite, &QPushButton::clicked, this, &AccessDialog::inviteRequested);
    }

    m_sharedNames = new QLabel(this);
    m_sharedNames->setWordWrap(true);
    layout->addWidget(m_sharedNames);

    if (!selfIsAdmin())
    {
        QStringList admins;
        for (const MemberId& admin : m_roster.admins())
        {
            const std::optional<RosterPerson> person = m_roster.person(admin);
            if (person.has_value())
            {
                admins.append(person.value().displayName());
            }
        }
        QLabel* askAdmins = new QLabel(tr("To add or remove people, ask an admin: %1.")
                                           .arg(QLocale().createSeparatedList(admins)),
                                       this);
        askAdmins->setWordWrap(true);
        layout->addWidget(askAdmins);
    }

    QHBoxLayout* body = new QHBoxLayout();
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Person or device"), tr("Role, or added by")});
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

    m_originsUnchecked = new QLabel(tr("Who added each device could not be checked just now."), this);
    m_originsUnchecked->hide();
    layout->addWidget(m_originsUnchecked);

    QDialogButtonBox* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(close);

    // Linking and leaving belong to a shared ward; an unshared one has only you.
    m_link->setVisible(m_shared);
    m_leave->setVisible(m_shared);
    m_remove->setVisible(selfIsAdmin());
    m_role->setVisible(selfIsAdmin());

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &AccessDialog::onSelectionChanged);
    connect(m_link, &QPushButton::clicked, this, &AccessDialog::linkRequested);
    connect(m_leave, &QPushButton::clicked, this, &AccessDialog::leaveRequested);
    connect(m_unlink, &QPushButton::clicked, this, &AccessDialog::onUnlink);
    connect(m_role, &QPushButton::clicked, this, &AccessDialog::onRole);
    connect(m_remove, &QPushButton::clicked, this, &AccessDialog::onRemove);
    connect(m_details, &QPushButton::clicked, this, &AccessDialog::onDetails);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fillTree();
    onSelectionChanged();
}

void AccessDialog::setOrigins(const QHash<QString, AccessEvents::Origin>& origins, bool checked)
{
    m_origins = origins;
    m_originsUnchecked->setVisible(!checked);
    updateAddedColumn();
}

void AccessDialog::setRoster(const AccessRoster& roster)
{
    // Kept across the refill by what it names, not by row.
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    const QString member = selected.isEmpty() ? QString() : selected.first()->data(0, MemberRole).toString();
    const QString device = selected.isEmpty() ? QString() : selected.first()->data(0, DeviceRole).toString();

    m_roster = roster;
    fillTree();

    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        if ((*it)->data(0, MemberRole).toString() == member && (*it)->data(0, DeviceRole).toString() == device)
        {
            (*it)->setSelected(true);
        }
    }
}

void AccessDialog::updateAddedColumn()
{
    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        const std::optional<RosterDevice> device =
            m_roster.deviceFor(Fingerprint::fromHex((*it)->data(0, DeviceRole).toString()));
        if (device.has_value())
        {
            (*it)->setText(1, addedText(device.value()));
        }
    }
}

void AccessDialog::fillTree()
{
    const QStringList warnings = sharedNames(m_roster.people());
    m_sharedNames->setText(warnings.join(" ") + " " + tr("Check each is who you expect."));
    m_sharedNames->setVisible(!warnings.isEmpty());

    m_tree->clear();
    for (const RosterPerson& person : m_roster.people())
    {
        const bool isSelf = person.member() == m_self.member;
        const QString name = isSelf ? tr("%1 (you)").arg(person.displayName()) : person.displayName();
        QTreeWidgetItem* row = new QTreeWidgetItem(m_tree, {name, person.isAdmin() ? tr("Admin") : tr("Member")});
        row->setData(0, MemberRole, person.member().toHex());

        for (const RosterDevice& device : person.devices())
        {
            const bool thisDevice = device.fingerprint() == m_self.fingerprint();
            const QString deviceName = thisDevice ? tr("%1 (this device)").arg(device.deviceName()) : device.deviceName();
            QTreeWidgetItem* deviceRow = new QTreeWidgetItem(row, {deviceName, addedText(device)});
            deviceRow->setData(0, MemberRole, person.member().toHex());
            deviceRow->setData(0, DeviceRole, device.fingerprint().toHex());
        }
    }
    m_tree->expandAll();
    onSelectionChanged();
}

QString AccessDialog::addedText(const RosterDevice& device) const
{
    const auto origin = m_origins.constFind(device.fingerprint().toHex());
    if (origin == m_origins.constEnd())
    {
        return QString();
    }

    // A device listed by its own header is where the chain begins: no date the
    // header gives is when the ward began.
    if (origin.value().addedBy == device.fingerprint())
    {
        return tr("Created the ward");
    }
    const QString when = QLocale().toString(origin.value().addedAt.toLocalTime().date(), QLocale::ShortFormat);

    const std::optional<RosterPerson> person = m_roster.personFor(origin.value().addedBy);
    const std::optional<RosterDevice> adder = m_roster.deviceFor(origin.value().addedBy);
    if (!person.has_value() || !adder.has_value())
    {
        return tr("Added by a former device, %1").arg(when);
    }
    return tr("Added by %1's %2, %3").arg(person.value().displayName(), adder.value().deviceName(), when);
}

std::optional<RosterPerson> AccessDialog::selectedPerson() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty())
    {
        return std::nullopt;
    }
    return m_roster.person(MemberId::fromHex(selected.first()->data(0, MemberRole).toString()));
}

std::optional<RosterDevice> AccessDialog::selectedDevice() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty() || selected.first()->data(0, DeviceRole).toString().isEmpty())
    {
        return std::nullopt;
    }
    return m_roster.deviceFor(Fingerprint::fromHex(selected.first()->data(0, DeviceRole).toString()));
}

bool AccessDialog::selfIsAdmin() const
{
    const std::optional<RosterPerson> self = m_roster.person(m_self.member);
    return self.has_value() && self.value().isAdmin();
}

bool AccessDialog::isOnlyAdmin() const
{
    return selfIsAdmin() && m_roster.admins().size() == 1;
}

void AccessDialog::onSelectionChanged()
{
    const std::optional<RosterPerson> person = selectedPerson();
    const std::optional<RosterDevice> device = selectedDevice();
    const bool isSelf = person.has_value() && person.value().member() == m_self.member;
    const bool admin = selfIsAdmin();

    m_details->setEnabled(person.has_value());

    // Removing and changing roles are an admin's, on someone else.
    m_remove->setEnabled(admin && person.has_value() && !isSelf);

    // Make Member on yourself steps down - never as the only admin, since a ward
    // always has one.
    m_role->setToolTip(QString());
    if (person.has_value() && !device.has_value() && (isSelf ? person.value().isAdmin() : admin))
    {
        m_role->setText(person.value().isAdmin() ? tr("Make Member") : tr("Make Admin"));
        const bool blocked = isSelf && isOnlyAdmin();
        m_role->setEnabled(!blocked);
        if (blocked)
        {
            m_role->setToolTip(tr("A ward needs an admin. Make someone else an admin first."));
        }
    }
    else
    {
        m_role->setText(person.has_value() && person.value().isAdmin() ? tr("Make Member") : tr("Make Admin"));
        m_role->setEnabled(false);
    }

    // Unlink: your own devices but this one; another person's, as an admin, but their last.
    bool mayUnlink = false;
    if (device.has_value())
    {
        mayUnlink = isSelf ? device.value().fingerprint() != m_self.fingerprint()
                           : admin && person.value().deviceCount() > 1;
    }
    m_unlink->setEnabled(mayUnlink);
}

void AccessDialog::onRemove()
{
    const std::optional<RosterPerson> person = selectedPerson();
    if (person.has_value())
    {
        emit removeRequested(person.value().member());
    }
}

void AccessDialog::onRole()
{
    const std::optional<RosterPerson> person = selectedPerson();
    if (person.has_value())
    {
        emit roleRequested(person.value().member(),
                           person.value().isAdmin() ? RosterPerson::Role::Member : RosterPerson::Role::Admin);
    }
}

void AccessDialog::onUnlink()
{
    const std::optional<RosterPerson> person = selectedPerson();
    const std::optional<RosterDevice> device = selectedDevice();
    if (person.has_value() && device.has_value())
    {
        emit unlinkRequested(person.value().member(), device.value().fingerprint());
    }
}

void AccessDialog::onDetails()
{
    const std::optional<RosterPerson> person = selectedPerson();
    if (!person.has_value())
    {
        return;
    }

    QStringList lines;
    lines.append(tr("%1 (%2)").arg(person.value().displayName(),
                                   person.value().isAdmin() ? tr("Admin") : tr("Member")));
    for (const RosterDevice& device : person.value().devices())
    {
        lines.append(tr("%1: %2").arg(device.deviceName(), device.fingerprint().toGroupedHex()));
    }
    QMessageBox::information(this, tr("Details"), lines.join("\n"));
}
