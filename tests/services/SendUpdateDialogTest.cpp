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

}  // namespace

SendUpdateDialogTest::SendUpdateDialogTest(QObject* parent)
    : QObject(parent)
{
}

void SendUpdateDialogTest::listsTheOtherMembersAndCopiesTheirEmails()
{
    Identity alicesLaptop = Identity::generate();
    Identity bobsPhone = Identity::generate();
    Identity carolsDesktop = Identity::generate();
    AccessList access({AccessUser(UserId::generate(), {"Alice", "alice@example.com"}, AccessRole::Admin,
                                  {deviceNamed("Laptop", alicesLaptop)}),
                       AccessUser(UserId::generate(), {"Bob", "bob@example.com"}, AccessRole::Member,
                                  {deviceNamed("Phone", bobsPhone)}),
                       AccessUser(UserId::generate(), {"Carol", ""}, AccessRole::Member,
                                  {deviceNamed("Desktop", carolsDesktop)})},
                      {}, {});

    OtherMembers members = SendUpdateDialog::otherMembers(access, alicesLaptop.fingerprint());

    QCOMPARE(members.lines, (QStringList{"Bob (bob@example.com)", "Carol"}));
    QCOMPARE(members.emails, QString("bob@example.com"));
}
