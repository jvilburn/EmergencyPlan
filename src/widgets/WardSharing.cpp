#include "WardSharing.h"
#include "AppSettings.h"
#include "InvitationFile.h"
#include "SendFileDialog.h"
#include "UserFolder.h"
#include "WardSession.h"

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

QByteArray WardSharing::inviteUser(WardSession& session, const QString& label, bool admin)
{
    QString name = label.trimmed();
    if (name.isEmpty())
    {
        return QByteArray();
    }
    return session.invite(InvitedUser{admin ? AccessRole::Admin : AccessRole::Member, name}).invite;
}

void WardSharing::addUser(WardSession& session, const AppSettings& settings, QWidget* parent)
{
    if (settings.useServer())
    {
        QMessageBox::information(parent, tr("Add a User"), tr("Connect to the internet to invite someone."));
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
        saveInvitation(session, invite, parent);
    }
}

void WardSharing::addDevice(WardSession& session, const AppSettings& settings, QWidget* parent)
{
    if (settings.useServer())
    {
        QMessageBox::information(parent, tr("Add a Device"), tr("Connect to the internet to invite someone."));
        return;
    }
    saveInvitation(session, session.invite(std::nullopt).invite, parent);
}

void WardSharing::leaveWard(const AppSettings& settings, QWidget* parent)
{
    QString message = settings.useServer()
        ? tr("Connect to the internet to leave this ward.")
        : tr("Leaving a ward needs the Ward Planning server, which is turned off in Settings.");
    QMessageBox::information(parent, tr("Leave Ward"), message);
}

void WardSharing::saveInvitation(WardSession& session, const QByteArray& invite, QWidget* parent)
{
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
    if (!file.open(QIODevice::WriteOnly) || file.write(session.writeInvitation(invite)) < 0 || !file.commit())
    {
        QMessageBox::warning(parent, tr("Save Invitation"),
                             tr("The invitation could not be saved: %1.").arg(file.errorString()));
        return;
    }
    QString message = entry.invitedUser().has_value()
        ? tr("%1 has invited you to %2. Install Ward Planning, then open the attached invitation.")
              .arg(session.ownUser().name(), wardName)
        : tr("Send this invitation to yourself and open it on your other device.");
    SendFileDialog dialog(tr("Save Invitation"), message, path, parent);
    dialog.exec();
}
