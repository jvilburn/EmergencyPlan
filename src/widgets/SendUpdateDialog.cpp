#include "SendUpdateDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>

SendUpdateDialog::SendUpdateDialog(const AccessList& access, const QByteArray& self, const QString& path,
                                   QWidget* parent)
    : SendFileDialog(tr("Send Update"),
                     tr("Your update is ready. Attach it to an email or text to the other members so they get your "
                        "changes."),
                     path, parent)
{
    OtherMembers members = otherMembers(access, self);
    if (!members.lines.isEmpty())
    {
        QLabel* list = new QLabel(members.lines.join("\n"), this);
        list->setTextFormat(Qt::PlainText);
        list->setTextInteractionFlags(Qt::TextSelectableByMouse);
        addAboveFile(list);
    }
    if (!members.emails.isEmpty())
    {
        QPushButton* copyEmails = new QPushButton(tr("Copy Emails"), this);
        addAboveFile(copyEmails);
        QString emails = members.emails;
        connect(copyEmails, &QPushButton::clicked, this, [emails]() { QGuiApplication::clipboard()->setText(emails); });
    }
    QString reminder = otherDevicesReminder(access, self);
    if (!reminder.isEmpty())
    {
        QLabel* reminderLabel = new QLabel(reminder, this);
        reminderLabel->setTextFormat(Qt::PlainText);
        addAboveFile(reminderLabel);
    }
}

OtherMembers SendUpdateDialog::otherMembers(const AccessList& access, const QByteArray& self)
{
    UserId own = access.userOfDevice(self)->id();
    OtherMembers members;
    QStringList emails;
    for (const AccessUser& user : access.users())
    {
        if (user.id() == own)
        {
            continue;
        }
        if (user.email().isEmpty())
        {
            members.lines.append(user.name());
            continue;
        }
        members.lines.append(tr("%1 (%2)").arg(user.name(), user.email()));
        emails.append(user.email());
    }
    members.emails = emails.join(", ");
    return members;
}

QString SendUpdateDialog::otherDevicesReminder(const AccessList& access, const QByteArray& self)
{
    QStringList names;
    for (const AccessDevice& device : access.userOfDevice(self)->devices())
    {
        if (device.fingerprint() != self)
        {
            names.append(device.name());
        }
    }
    if (names.isEmpty())
    {
        return QString();
    }
    return tr("Also open this update on your other devices: %1.").arg(names.join(", "));
}
