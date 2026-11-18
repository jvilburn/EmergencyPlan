#pragma once

#include <QDialog>
#include <QString>

/// An invitation link ready to send: what the inviting user does with it, then the message to
/// pass on, set apart in a box since it is for someone else to read, with a button that copies
/// it and Done.
class SendLinkDialog : public QDialog
{
    Q_OBJECT

public:
    /// instructions speak to the inviting user, and message is what the button named copyLabel
    /// copies: Copy Message for a message to pass on, Copy Link for a link alone.
    SendLinkDialog(const QString& instructions, const QString& message, const QString& copyLabel, QWidget* parent);

    /// What the inviting user is told for an invitation to invitee, a new user.
    static QString userInstructions(const QString& invitee);

    /// The message for a new user: who invited them to which ward, and the link to click.
    static QString userMessage(const QString& inviter, const QString& wardName, const QString& link);

    /// What the inviting user is told for an invitation to another of their own devices.
    static QString deviceInstructions();

private slots:
    void onCopyMessage();

private:
    QString m_message;
};
