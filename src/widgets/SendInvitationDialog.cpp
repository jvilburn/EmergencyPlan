#include "SendInvitationDialog.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SendInvitationDialog::SendInvitationDialog(const QString& invitee, const QString& inviter, const QString& wardName,
                                           const QString& path, QWidget* parent)
    : SendFileDialog(tr("Save Invitation"), instructions(invitee), path, parent)
    , m_message(message(inviter, wardName))
{
    // The message is someone else's to read, so it sits in a box of its own.
    QFrame* box = new QFrame(this);
    box->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* boxLayout = new QVBoxLayout(box);
    QLabel* messageLabel = new QLabel(m_message, box);
    messageLabel->setTextFormat(Qt::PlainText);
    messageLabel->setWordWrap(true);
    messageLabel->setMinimumWidth(320);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    boxLayout->addWidget(messageLabel);
    addAboveFile(box);

    QPushButton* copyMessage = new QPushButton(tr("Copy Message"), this);
    addAboveFile(copyMessage);
    connect(copyMessage, &QPushButton::clicked, this, &SendInvitationDialog::onCopyMessage);
}

QString SendInvitationDialog::instructions(const QString& invitee)
{
    return tr("Your invitation for %1 is ready. Attach it to an email or text to %1, with this message:").arg(invitee);
}

QString SendInvitationDialog::message(const QString& inviter, const QString& wardName)
{
    return tr("%1 has invited you to %2. Install Ward Planning, then open the attached invitation.")
        .arg(inviter, wardName);
}

void SendInvitationDialog::onCopyMessage()
{
    QGuiApplication::clipboard()->setText(m_message);
}
