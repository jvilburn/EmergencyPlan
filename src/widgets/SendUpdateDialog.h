#pragma once

#include "AccessList.h"
#include "SendFileDialog.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

/// The other members an update goes to: every user of the ward but this device's own, in the
/// access list's order.
struct OtherMembers
{
    QStringList lines;  ///< one per member: the name, with the email where there is one
    QString emails;     ///< every listed email, comma-separated: what Copy Emails copies
};

/// The update file just written, ready to send: what to do with it, the other members it goes
/// to with Copy Emails while any has an email, a reminder naming this user's other devices
/// when there are any, and the file to drag into an email.
class SendUpdateDialog : public SendFileDialog
{
    Q_OBJECT

public:
    /// access is the open ward's, self this device's fingerprint, and path the update file.
    SendUpdateDialog(const AccessList& access, const QByteArray& self, const QString& path, QWidget* parent);

    /// The users of access other than the one holding the device self, as the dialog lists them.
    static OtherMembers otherMembers(const AccessList& access, const QByteArray& self);

    /// The line reminding the user to open the update on their other devices, naming them
    /// comma-separated in the access list's order; empty when the user holding the device self
    /// has no other device.
    static QString otherDevicesReminder(const AccessList& access, const QByteArray& self);
};
