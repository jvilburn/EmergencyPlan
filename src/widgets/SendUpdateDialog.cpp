#include "SendUpdateDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SendUpdateDialog::SendUpdateDialog(const AccessList& access, const QByteArray& self, const QString& path,
                                   QWidget* parent)
    : SendFileDialog(tr("Send Update"),
                     tr("Your update is ready. Attach it to an email or text to the other members so they get your "
                        "changes."),
                     path, parent)
{
    OtherMembers members = otherMembers(access, self);
    if (members.lines.isEmpty())
    {
        return;
    }

    QWidget* section = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel* list = new QLabel(members.lines.join("\n"), section);
    list->setTextFormat(Qt::PlainText);
    list->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(list);
    if (!members.emails.isEmpty())
    {
        QPushButton* copyEmails = new QPushButton(tr("Copy Emails"), section);
        layout->addWidget(copyEmails, 0, Qt::AlignLeft);
        QString emails = members.emails;
        connect(copyEmails, &QPushButton::clicked, this, [emails]() { QGuiApplication::clipboard()->setText(emails); });
    }
    addAboveFile(section);
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
