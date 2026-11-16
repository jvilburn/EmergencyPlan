#pragma once

#include "SendFileDialog.h"

#include <QString>

/// The invitation for a new user, just saved and ready to send: what the inviting user does
/// with it, then the message to pass on to the person invited, set apart in a box with Copy
/// Message, then the file to drag into an email.
class SendInvitationDialog : public SendFileDialog
{
    Q_OBJECT

public:
    /// invitee is who the invitation is for, as the inviting user named them; inviter is that
    /// user's own name, wardName the ward's, and path the invitation file.
    SendInvitationDialog(const QString& invitee, const QString& inviter, const QString& wardName,
                         const QString& path, QWidget* parent);

    /// What the dialog tells the inviting user to do, naming invitee.
    static QString instructions(const QString& invitee);

    /// The message for the person invited, which Copy Message copies.
    static QString message(const QString& inviter, const QString& wardName);

private slots:
    void onCopyMessage();

private:
    QString m_message;
};
