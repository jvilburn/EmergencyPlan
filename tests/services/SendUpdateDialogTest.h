#pragma once

#include <QObject>

/// Send Update lists every user but this device's own, with their email where they have one,
/// Copy Emails copies exactly the listed emails, and the reminder names this user's other
/// devices, or is empty when there are none.
class SendUpdateDialogTest : public QObject
{
    Q_OBJECT

public:
    explicit SendUpdateDialogTest(QObject* parent);

private slots:
    void listsTheOtherMembersAndCopiesTheirEmails();
    void remindsToOpenTheUpdateOnTheUsersOtherDevices();
    void remindsOfNothingWithoutAnotherDevice();
};
