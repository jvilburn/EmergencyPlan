#pragma once

#include <QObject>

/// Send Update lists every user but this device's own, with their email where they have one,
/// and Copy Emails copies exactly the listed emails.
class SendUpdateDialogTest : public QObject
{
    Q_OBJECT

public:
    explicit SendUpdateDialogTest(QObject* parent);

private slots:
    void listsTheOtherMembersAndCopiesTheirEmails();
};
