#include "WhoHasAccessDialog.h"
#include "AccessCommands.h"
#include "AccessRules.h"
#include "AppSettings.h"
#include "DocumentManager.h"
#include "InvitationFile.h"
#include "SendFileDialog.h"
#include "UserFolder.h"
#include "WardSession.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace
{

constexpr int KindRole = Qt::UserRole;
constexpr int KeyRole = Qt::UserRole + 1;

}  // namespace

WhoHasAccessDialog::WhoHasAccessDialog(WardSession& session, const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_settings(settings)
{
    setWindowTitle(tr("Who Has Access"));
    resize(480, 360);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* listRow = new QHBoxLayout();
    m_list = new QTreeWidget(this);
    m_list->setHeaderHidden(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    listRow->addWidget(m_list, 1);

    QVBoxLayout* rowActions = new QVBoxLayout();
    addActionButton(rowActions, AccessAction::Remove, tr("Remove"), &WhoHasAccessDialog::onRemove);
    addActionButton(rowActions, AccessAction::MakeAdmin, tr("Make admin"), &WhoHasAccessDialog::onMakeAdmin);
    addActionButton(rowActions, AccessAction::MakeMember, tr("Make member"), &WhoHasAccessDialog::onMakeMember);
    addActionButton(rowActions, AccessAction::RemoveDevice, tr("Remove device"), &WhoHasAccessDialog::onRemoveDevice);
    addActionButton(rowActions, AccessAction::RemoveThisDevice, tr("Remove this device"),
                    &WhoHasAccessDialog::onRemoveThisDevice);
    addActionButton(rowActions, AccessAction::WithdrawInvite, tr("Withdraw invite"),
                    &WhoHasAccessDialog::onWithdrawInvite);
    addActionButton(rowActions, AccessAction::SaveInvitation, tr("Save invitation"),
                    &WhoHasAccessDialog::onSaveInvitation);
    rowActions->addStretch();
    listRow->addLayout(rowActions);
    layout->addLayout(listRow);

    layout->addWidget(new QLabel(tr("Names change in Settings."), this));

    m_wardActions = new QWidget(this);
    QHBoxLayout* wardRow = new QHBoxLayout(m_wardActions);
    wardRow->setContentsMargins(0, 0, 0, 0);
    m_addUserButton = new QPushButton(tr("Add a user"), m_wardActions);
    m_addUserNotice = new QLabel(m_wardActions);
    m_addUserNotice->setWordWrap(true);
    QPushButton* addDeviceButton = new QPushButton(tr("Add a device"), m_wardActions);
    QPushButton* leaveButton = new QPushButton(tr("Leave"), m_wardActions);
    wardRow->addWidget(m_addUserButton);
    wardRow->addWidget(m_addUserNotice, 1);
    wardRow->addWidget(addDeviceButton);
    wardRow->addWidget(leaveButton);
    layout->addWidget(m_wardActions);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_addUserButton, &QPushButton::clicked, this, &WhoHasAccessDialog::onAddUser);
    connect(addDeviceButton, &QPushButton::clicked, this, &WhoHasAccessDialog::onAddDevice);
    connect(leaveButton, &QPushButton::clicked, this, &WhoHasAccessDialog::onLeave);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &WhoHasAccessDialog::updateActions);
    connect(&m_session, &WardSession::documentChanged, this, &WhoHasAccessDialog::rebuild);

    rebuild();
}

QList<AccessAction> WhoHasAccessDialog::offeredActions() const
{
    QList<AccessAction> actions;
    QTreeWidgetItem* row = selectedRow();
    if (m_session.isNoLongerShared() || !row)
    {
        return actions;
    }

    const AccessList& access = m_session.document().access();
    const QByteArray& actor = m_session.deviceFingerprint();
    QByteArray key = row->data(0, KeyRole).toByteArray();
    switch (static_cast<RowKind>(row->data(0, KindRole).toInt()))
    {
    case RowKind::Heading:
        break;
    case RowKind::User:
    {
        AccessUser user = selectedUser();
        if (AccessRules::removeUser(access, actor, user.id()).isEmpty())
        {
            actions.append(AccessAction::Remove);
        }
        if (user.role() == AccessRole::Member
            && AccessRules::setRole(access, actor, user.id(), AccessRole::Admin).isEmpty())
        {
            actions.append(AccessAction::MakeAdmin);
        }
        if (user.role() == AccessRole::Admin
            && AccessRules::setRole(access, actor, user.id(), AccessRole::Member).isEmpty())
        {
            actions.append(AccessAction::MakeMember);
        }
        break;
    }
    case RowKind::Device:
        if (key == actor)
        {
            actions.append(AccessAction::RemoveThisDevice);
        }
        else if (AccessRules::removeDevice(access, actor, key).isEmpty())
        {
            actions.append(AccessAction::RemoveDevice);
        }
        break;
    case RowKind::Invite:
        if (AccessRules::withdrawInvite(access, actor, key).isEmpty())
        {
            actions.append(AccessAction::WithdrawInvite);
        }
        if (m_session.invitesMadeHere().contains(key))
        {
            actions.append(AccessAction::SaveInvitation);
        }
        break;
    }
    return actions;
}

QString WhoHasAccessDialog::addUserNotice() const
{
    return AccessRules::invite(m_session.document().access(), m_session.deviceFingerprint(), true);
}

QByteArray WhoHasAccessDialog::addUser(const QString& label, bool admin)
{
    QString name = label.trimmed();
    if (name.isEmpty())
    {
        return QByteArray();
    }
    return m_session.invite(InvitedUser{admin ? AccessRole::Admin : AccessRole::Member, name}).invite;
}

void WhoHasAccessDialog::rebuild()
{
    QTreeWidgetItem* selected = selectedRow();
    QVariant kind = selected ? selected->data(0, KindRole) : QVariant();
    QVariant key = selected ? selected->data(0, KeyRole) : QVariant();
    int scroll = m_list->verticalScrollBar()->value();
    m_list->clear();

    const AccessList& access = m_session.document().access();
    for (const AccessUser& user : access.users())
    {
        QString role = user.role() == AccessRole::Admin ? tr("admin") : tr("member");
        QTreeWidgetItem* userRow = addRow(nullptr, tr("%1 (%2)").arg(user.name(), role), RowKind::User,
                                          user.id().toString().toUtf8());
        for (const AccessDevice& device : user.devices())
        {
            QString text = device.fingerprint() == m_session.deviceFingerprint()
                ? tr("%1 (this device)").arg(device.name())
                : device.name();
            addRow(userRow, text, RowKind::Device, device.fingerprint());
        }
    }
    if (!access.invites().isEmpty())
    {
        QTreeWidgetItem* heading = addRow(nullptr, tr("Open invites"), RowKind::Heading, QByteArray());
        heading->setFlags(Qt::ItemIsEnabled);
        for (const AccessInvite& invite : access.invites())
        {
            addRow(heading, access.describeInvite(invite), RowKind::Invite, invite.fingerprint());
        }
    }
    m_list->expandAll();

    for (QTreeWidgetItemIterator it(m_list); *it; ++it)
    {
        if ((*it)->data(0, KindRole) == kind && (*it)->data(0, KeyRole) == key)
        {
            m_list->setCurrentItem(*it);
            break;
        }
    }
    m_list->verticalScrollBar()->setValue(scroll);
    updateActions();
}

void WhoHasAccessDialog::updateActions()
{
    QList<AccessAction> offered = offeredActions();
    for (auto it = m_actionButtons.cbegin(); it != m_actionButtons.cend(); ++it)
    {
        it.value()->setVisible(offered.contains(it.key()));
    }
    QString notice = addUserNotice();
    m_addUserNotice->setText(notice);
    m_addUserNotice->setVisible(!notice.isEmpty());
    m_addUserButton->setVisible(notice.isEmpty());
    m_wardActions->setEnabled(!m_session.isNoLongerShared());
}

void WhoHasAccessDialog::onAddUser()
{
    if (m_settings.useServer())
    {
        QMessageBox::information(this, tr("Add a User"), tr("Connect to the internet to invite someone."));
        return;
    }

    QDialog ask(this);
    ask.setWindowTitle(tr("Add a User"));
    QVBoxLayout* layout = new QVBoxLayout(&ask);
    layout->addWidget(new QLabel(tr("Who is this invitation for?"), &ask));
    QLineEdit* label = new QLineEdit(&ask);
    layout->addWidget(label);
    QCheckBox* admin = new QCheckBox(tr("Make them an admin"), &ask);
    layout->addWidget(admin);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &ask);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &ask, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &ask, &QDialog::reject);
    if (ask.exec() != QDialog::Accepted)
    {
        return;
    }

    QByteArray invite = addUser(label->text(), admin->isChecked());
    if (!invite.isEmpty())
    {
        saveInvitation(invite);
    }
}

void WhoHasAccessDialog::onAddDevice()
{
    if (m_settings.useServer())
    {
        QMessageBox::information(this, tr("Add a Device"), tr("Connect to the internet to invite someone."));
        return;
    }
    saveInvitation(m_session.invite(std::nullopt).invite);
}

void WhoHasAccessDialog::onLeave()
{
    QString message = m_settings.useServer()
        ? tr("Connect to the internet to leave this ward.")
        : tr("Leaving a ward needs the Ward Planning server, which is turned off in Settings.");
    QMessageBox::information(this, tr("Leave"), message);
}

void WhoHasAccessDialog::onRemoveThisDevice()
{
    QString message = m_settings.useServer()
        ? tr("Connect to the internet to remove this device.")
        : tr("Removing this device needs the Ward Planning server, which is turned off in Settings.");
    QMessageBox::information(this, tr("Remove This Device"), message);
}

void WhoHasAccessDialog::onSaveInvitation()
{
    saveInvitation(selectedRow()->data(0, KeyRole).toByteArray());
}

void WhoHasAccessDialog::onRemove()
{
    AccessUser user = selectedUser();
    QMessageBox ask(QMessageBox::Question, tr("Remove"),
                    tr("Remove %1 from %2?").arg(user.name(), m_session.document().wardName()), QMessageBox::Cancel,
                    this);
    QPushButton* remove = ask.addButton(tr("Remove"), QMessageBox::DestructiveRole);
    ask.exec();
    if (ask.clickedButton() != remove)
    {
        return;
    }
    DocumentManager::instance()->executeCommand(RemoveUserCommand::create(m_session.deviceFingerprint(), user));
}

void WhoHasAccessDialog::onMakeAdmin()
{
    DocumentManager::instance()->executeCommand(
        SetRoleCommand::create(m_session.deviceFingerprint(), selectedUser(), AccessRole::Admin));
}

void WhoHasAccessDialog::onMakeMember()
{
    DocumentManager::instance()->executeCommand(
        SetRoleCommand::create(m_session.deviceFingerprint(), selectedUser(), AccessRole::Member));
}

void WhoHasAccessDialog::onRemoveDevice()
{
    AccessDevice device = *m_session.document().access().findDevice(selectedRow()->data(0, KeyRole).toByteArray());
    DocumentManager::instance()->executeCommand(RemoveDeviceCommand::create(m_session.deviceFingerprint(), device));
}

void WhoHasAccessDialog::onWithdrawInvite()
{
    const AccessList& access = m_session.document().access();
    AccessInvite invite = *access.findInvite(selectedRow()->data(0, KeyRole).toByteArray());
    DocumentManager::instance()->executeCommand(
        WithdrawInviteCommand::create(m_session.deviceFingerprint(), invite, access));
}

QTreeWidgetItem* WhoHasAccessDialog::addRow(QTreeWidgetItem* parent, const QString& text, RowKind kind,
                                            const QByteArray& key)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_list);
    item->setText(0, text);
    item->setData(0, KindRole, static_cast<int>(kind));
    item->setData(0, KeyRole, key);
    return item;
}

void WhoHasAccessDialog::addActionButton(QBoxLayout* layout, AccessAction action, const QString& text,
                                         void (WhoHasAccessDialog::*slot)())
{
    QPushButton* button = new QPushButton(text, this);
    layout->addWidget(button);
    m_actionButtons.insert(action, button);
    connect(button, &QPushButton::clicked, this, slot);
}

QTreeWidgetItem* WhoHasAccessDialog::selectedRow() const
{
    QList<QTreeWidgetItem*> selected = m_list->selectedItems();
    return selected.isEmpty() ? nullptr : selected.first();
}

AccessUser WhoHasAccessDialog::selectedUser() const
{
    QString id = QString::fromUtf8(selectedRow()->data(0, KeyRole).toByteArray());
    return *m_session.document().access().findUser(UserId::fromString(id));
}

void WhoHasAccessDialog::saveInvitation(const QByteArray& invite)
{
    const Document& document = m_session.document();
    QString wardName = document.wardName();
    AccessInvite entry = *document.access().findInvite(invite);
    QString folder = UserFolder::standard().path();
    QDir().mkpath(folder);
    QString suggested = QDir(folder).filePath(InvitationFile::fileName(wardName, document.access().describeInvite(entry)));
    QString path = QFileDialog::getSaveFileName(this, tr("Save Invitation"), suggested,
                                                tr("Ward Planning invitations (*.wardinvite)"));
    if (path.isEmpty())
    {
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(m_session.writeInvitation(invite)) < 0 || !file.commit())
    {
        QMessageBox::warning(this, tr("Save Invitation"),
                             tr("The invitation could not be saved: %1.").arg(file.errorString()));
        return;
    }
    QString message = entry.invitedUser().has_value()
        ? tr("%1 has invited you to %2. Install Ward Planning, then open the attached invitation.")
              .arg(m_session.ownUser().name(), wardName)
        : tr("Send this invitation to yourself and open it on your other device.");
    SendFileDialog dialog(tr("Save Invitation"), message, path, this);
    dialog.exec();
}
