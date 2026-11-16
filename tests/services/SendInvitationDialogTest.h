#pragma once

#include <QObject>

/// The dialog shown after an invitation for a new user is saved: its words to the inviting
/// user, and the message it hands them to pass on.
class SendInvitationDialogTest : public QObject
{
    Q_OBJECT

public:
    explicit SendInvitationDialogTest(QObject* parent);

private slots:
    void instructionsAddressTheInvitingUserAndTheMessageTheInvitee();
    void copyMessageCopiesTheMessageAlone();
};
