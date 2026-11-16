#include "WardSharing.h"

#include "AppSettings.h"
#include "InvitationFile.h"
#include "SendFileDialog.h"
#include "SendInvitationDialog.h"
#include "UserFolder.h"
#include "WardSession.h"
#include "WardSync.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSaveFile>
#include <QVBoxLayout>
#include <optional>

QString WardSharing::whatStopsSharing(const WardSync& sync)
{
    switch (sync.problem())
    {
    case SyncProblem::NewerFormat:
        return tr("This ward was saved by a newer version of Ward Planning. Update to see the latest changes and "
                  "share your work.");
    case SyncProblem::Unreadable:
        return tr("The ward on the server could not be read. Your copy is safe and you can keep working; sharing "
                  "resumes when the server has a readable copy.");
    case SyncProblem::PushFault:
        return tr("Your changes could not be shared right now. They are saved here.");
    case SyncProblem::Unreachable:
        return tr("Working offline. Your changes are saved here and will go up when the connection returns.");
    case SyncProblem::None:
        break;
    }
    return QString();
}

QString WardSharing::whatStopsInviting(const WardSession& session, const WardSync& sync)
{
    if (session.isNoLongerShared())
    {
        return tr("This ward is no longer shared with this device.");
    }
    if (session.savePending())
    {
        return tr("Your latest change could not be saved, so nothing can be shared yet.");
    }
    if (!sync.isConnected() || sync.problem() == SyncProblem::Unreachable)
    {
        return tr("Connect to the internet to invite someone.");
    }
    return whatStopsSharing(sync);
}

QByteArray WardSharing::inviteUser(WardSession& session, const QString& label, bool admin)
{
    QString name = label.trimmed();
    if (name.isEmpty())
    {
        return QByteArray();
    }
    return session.invite(InvitedUser{admin ? AccessRole::Admin : AccessRole::Member, name}).invite;
}

void WardSharing::addUser(WardSession& session, const AppSettings& settings, WardSync& sync, QWidget* parent)
{
    QString stopped = settings.useServer() ? whatStopsInviting(session, sync) : QString();
    if (!stopped.isEmpty())
    {
        QMessageBox::information(parent, tr("Add a User"), stopped);
        return;
    }

    QDialog ask(parent);
    ask.setWindowTitle(tr("Add a User"));
    QVBoxLayout* layout = new QVBoxLayout(&ask);
    layout->addWidget(new QLabel(tr("Who is this invitation for?"), &ask));
    QLineEdit* label = new QLineEdit(&ask);
    layout->addWidget(label);
    QCheckBox* admin = new QCheckBox(tr("Make them an admin"), &ask);
    layout->addWidget(admin);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &ask);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &ask, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &ask, &QDialog::reject);
    if (ask.exec() != QDialog::Accepted)
    {
        return;
    }

    QByteArray invite = inviteUser(session, label->text(), admin->isChecked());
    if (!invite.isEmpty())
    {
        saveInvitation(session, settings, sync, invite, parent);
    }
}

void WardSharing::addDevice(WardSession& session, const AppSettings& settings, WardSync& sync, QWidget* parent)
{
    QString stopped = settings.useServer() ? whatStopsInviting(session, sync) : QString();
    if (!stopped.isEmpty())
    {
        QMessageBox::information(parent, tr("Add a Device"), stopped);
        return;
    }
    QByteArray invite = session.invite(std::nullopt).invite;
    if (!invite.isEmpty())
    {
        saveInvitation(session, settings, sync, invite, parent);
    }
}

void WardSharing::leaveWard(const AppSettings& settings, QWidget* parent)
{
    QString message = settings.useServer()
        ? tr("Connect to the internet to leave this ward.")
        : tr("Leaving a ward needs the Ward Planning server, which is turned off in Settings.");
    QMessageBox::information(parent, tr("Leave Ward"), message);
}

void WardSharing::saveInvitation(WardSession& session, const AppSettings& settings, WardSync& sync,
                                 const QByteArray& invite, QWidget* parent)
{
    QByteArray invitation = settings.useServer()
        ? fetchedInvitation(session, sync, invite, parent)
        : session.writeInvitation(invite);
    if (invitation.isEmpty())
    {
        return;
    }

    const Document& document = session.document();
    QString wardName = document.wardName();
    AccessInvite entry = *document.access().findInvite(invite);
    QString folder = UserFolder::standard().path();
    QDir().mkpath(folder);
    QString suggested = QDir(folder).filePath(InvitationFile::fileName(wardName, document.access().describeInvite(entry)));
    QString path = QFileDialog::getSaveFileName(parent, tr("Save Invitation"), suggested,
                                                tr("Ward Planning invitations (*.wardinvite)"));
    if (path.isEmpty())
    {
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(invitation) < 0 || !file.commit())
    {
        QMessageBox::warning(parent, tr("Save Invitation"),
                             tr("The invitation could not be saved: %1.").arg(file.errorString()));
        return;
    }
    if (entry.invitedUser().has_value())
    {
        SendInvitationDialog dialog(entry.invitedUser()->label, session.ownUser().name(), wardName, path, parent);
        dialog.exec();
        return;
    }
    SendFileDialog dialog(tr("Save Invitation"),
                          tr("Your invitation is ready. Send it to yourself and open it on your other device."), path,
                          parent);
    dialog.exec();
}

QByteArray WardSharing::fetchedInvitation(WardSession& session, WardSync& sync, const QByteArray& invite,
                                          QWidget* parent)
{
    InvitationWait wait(invite, parent);
    QObject::connect(&sync, &WardSync::invitationFetched, &wait, &InvitationWait::onInvitationFetched);
    sync.fetchInvitation(invite);
    // The answer has come already when something stops the sharing; otherwise the dialog
    // waits for it, and closing the dialog gives up on it.
    if (!wait.isAnswered() && wait.exec() != QDialog::Accepted)
    {
        return QByteArray();
    }
    if (!wait.invitation().isEmpty())
    {
        return wait.invitation();
    }

    QString stopped = whatStopsInviting(session, sync);
    if (!session.document().access().findInvite(invite).has_value())
    {
        stopped = tr("Your invite could not be created, because the ward changed first. Try again.");
    }
    else if (stopped.isEmpty())
    {
        stopped = tr("The invitation could not be created right now. Try again.");
    }
    QMessageBox::information(parent, tr("Save Invitation"), stopped);
    return QByteArray();
}

InvitationWait::InvitationWait(const QByteArray& invite, QWidget* parent)
    : QDialog(parent)
    , m_invite(invite)
{
    setWindowTitle(tr("Save Invitation"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Creating the invitation..."), this));
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void InvitationWait::onInvitationFetched(const QByteArray& invite, const QByteArray& invitation)
{
    if (invite != m_invite)
    {
        return;
    }
    m_answered = true;
    m_invitation = invitation;
    accept();
}
