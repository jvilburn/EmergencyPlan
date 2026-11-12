#include "SendUpdateDialogTest.h"
#include "AccessList.h"
#include "Identity.h"
#include "SendUpdateDialog.h"

#include <QTest>

namespace
{

AccessDevice deviceNamed(const QString& name, const Identity& identity)
{
    return AccessDevice(name, identity.publicKey(), identity.verifyKey());
}

/// Four users in this order: Alice and Carol with an email, Bob, holding bobsDevices, and Dave
/// without one.
AccessList accessWithBobsDevices(const QList<AccessDevice>& bobsDevices)
{
    return AccessList({AccessUser(UserId::generate(), {"Alice", "alice@example.com"}, AccessRole::Admin,
                                  {deviceNamed("Laptop", Identity::generate())}),
                       AccessUser(UserId::generate(), {"Bob", "bob@example.com"}, AccessRole::Member, bobsDevices),
                       AccessUser(UserId::generate(), {"Carol", "carol@example.com"}, AccessRole::Member,
                                  {deviceNamed("Desktop", Identity::generate())}),
                       AccessUser(UserId::generate(), {"Dave", ""}, AccessRole::Member,
                                  {deviceNamed("Phone", Identity::generate())})},
                      {}, {});
}

}  // namespace

SendUpdateDialogTest::SendUpdateDialogTest(QObject* parent)
    : QObject(parent)
{
}

void SendUpdateDialogTest::listsTheOtherMembersAndCopiesTheirEmails()
{
    Identity bobsPhone = Identity::generate();
    AccessList access = accessWithBobsDevices({deviceNamed("Phone", bobsPhone)});

    OtherMembers members = SendUpdateDialog::otherMembers(access, bobsPhone.fingerprint());

    QCOMPARE(members.lines, (QStringList{"Alice (alice@example.com)", "Carol (carol@example.com)", "Dave"}));
    QCOMPARE(members.emails, QString("alice@example.com, carol@example.com"));
}

void SendUpdateDialogTest::remindsToOpenTheUpdateOnTheUsersOtherDevices()
{
    Identity bobsPhone = Identity::generate();
    AccessList access = accessWithBobsDevices({deviceNamed("Laptop", Identity::generate()),
                                               deviceNamed("Phone", bobsPhone),
                                               deviceNamed("Tablet", Identity::generate())});

    QCOMPARE(SendUpdateDialog::otherDevicesReminder(access, bobsPhone.fingerprint()),
             QString("Also open this update on your other devices: Laptop, Tablet."));
}

void SendUpdateDialogTest::remindsOfNothingWithoutAnotherDevice()
{
    Identity bobsPhone = Identity::generate();
    AccessList access = accessWithBobsDevices({deviceNamed("Phone", bobsPhone)});

    QCOMPARE(SendUpdateDialog::otherDevicesReminder(access, bobsPhone.fingerprint()), QString());
}
