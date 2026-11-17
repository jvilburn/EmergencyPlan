#include "SendLinkDialog.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SendLinkDialog::SendLinkDialog(const QString& instructions, const QString& message, QWidget* parent)
    : QDialog(parent)
    , m_message(message)
{
    setWindowTitle(tr("Invitation"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* instructionsLabel = new QLabel(instructions, this);
    instructionsLabel->setTextFormat(Qt::PlainText);
    instructionsLabel->setWordWrap(true);
    layout->addWidget(instructionsLabel);

    // The message is someone else's to read, so it sits in a box of its own.
    QFrame* box = new QFrame(this);
    box->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* boxLayout = new QVBoxLayout(box);
    QLabel* messageLabel = new QLabel(m_message, box);
    messageLabel->setTextFormat(Qt::PlainText);
    messageLabel->setWordWrap(true);
    messageLabel->setMinimumWidth(420);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    boxLayout->addWidget(messageLabel);
    layout->addWidget(box);

    QPushButton* copyMessage = new QPushButton(tr("Copy Message"), this);
    QPushButton* done = new QPushButton(tr("Done"), this);
    copyMessage->setDefault(true);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(copyMessage);
    buttons->addStretch(1);
    buttons->addWidget(done);
    layout->addLayout(buttons);
    connect(copyMessage, &QPushButton::clicked, this, &SendLinkDialog::onCopyMessage);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
}

QString SendLinkDialog::userInstructions(const QString& invitee)
{
    return tr("Your invitation for %1 is ready. Send %1 this message, by email or text:").arg(invitee);
}

QString SendLinkDialog::userMessage(const QString& inviter, const QString& wardName, const QString& link)
{
    return tr("%1 has invited you to %2 in Ward Planning. Click to join:\n%3").arg(inviter, wardName, link);
}

QString SendLinkDialog::deviceInstructions()
{
    return tr("Your invitation is ready. Open this link on your other device:");
}

void SendLinkDialog::onCopyMessage()
{
    QGuiApplication::clipboard()->setText(m_message);
}
