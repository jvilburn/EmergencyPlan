#include "WhoHasAccessDialogTest.h"
#include "AppSettings.h"
#include "TestSharing.h"
#include "WardSharing.h"
#include "WhoHasAccessDialog.h"

#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

namespace
{

/// Selects the one row of dialog's list whose text is text; false when there is not exactly one.
bool select(WhoHasAccessDialog& dialog, const QString& text)
{
    QTreeWidget* list = dialog.findChild<QTreeWidget*>();
    QList<QTreeWidgetItem*> found = list->findItems(text, Qt::MatchExactly | Qt::MatchRecursive);
    if (found.size() != 1)
    {
        return false;
    }
    list->setCurrentItem(found.first());
    return true;
}

}  // namespace

WhoHasAccessDialogTest::WhoHasAccessDialogTest(QObject* parent)
    : QObject(parent)
{
}

void WhoHasAccessDialogTest::memberSeesAskAnAdminInPlaceOfAddAUser()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath("settings.json"));
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));

    WhoHasAccessDialog carols(carol.session(), settings, nullptr);
    WhoHasAccessDialog alices(alice.session(), settings, nullptr);

    QCOMPARE(carols.addUserNotice(), QString("To add or remove users, ask an admin: Alice, Bob"));
    QCOMPARE(alices.addUserNotice(), QString());
}

void WhoHasAccessDialogTest::removeAndRolesAreOfferedOnlyWhereTheRulesAllow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath("settings.json"));
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    WhoHasAccessDialog alices(alice.session(), settings, nullptr);
    WhoHasAccessDialog carols(carol.session(), settings, nullptr);

    QVERIFY(alices.offeredActions().isEmpty());
    QVERIFY(select(alices, "Carol (member)"));
    QVERIFY(alices.offeredActions() == (QList<AccessAction>{AccessAction::Remove, AccessAction::MakeAdmin}));
    QVERIFY(select(alices, "Alice (admin)"));
    QVERIFY(alices.offeredActions().isEmpty());
    QVERIFY(select(alices, "Alice (this device)"));
    QVERIFY(alices.offeredActions() == QList<AccessAction>{AccessAction::RemoveThisDevice});
    // Carol's device row, under her user row "Carol (member)".
    QVERIFY(select(alices, "Carol"));
    QCOMPARE(alices.findChild<QTreeWidget*>()->currentItem()->parent()->text(0), QString("Carol (member)"));
    QVERIFY(alices.offeredActions().isEmpty());

    QVERIFY(select(carols, "Alice (admin)"));
    QVERIFY(carols.offeredActions().isEmpty());
}

void WhoHasAccessDialogTest::saveInvitationIsOfferedOnlyForInvitesThisDeviceMade()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath("settings.json"));
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));
    QVERIFY(bob.session().invite(InvitedUser{AccessRole::Member, "Dan"}).commit.status == CommitStatus::Applied);
    QVERIFY(TestSharing::send(bob, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});
    WhoHasAccessDialog alices(alice.session(), settings, nullptr);

    // Made after the dialog opened: the list rebuilds and shows it.
    QVERIFY(alice.session().invite(InvitedUser{AccessRole::Member, "Eve"}).commit.status == CommitStatus::Applied);

    QVERIFY(select(alices, "for Eve"));
    QVERIFY(alices.offeredActions()
            == (QList<AccessAction>{AccessAction::WithdrawInvite, AccessAction::SaveInvitation}));
    QVERIFY(select(alices, "for Dan"));
    QVERIFY(alices.offeredActions() == QList<AccessAction>{AccessAction::WithdrawInvite});
}

void WhoHasAccessDialogTest::addAUserWithNoAnswerMakesNoInvite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath("settings.json"));
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    WhoHasAccessDialog alices(alice.session(), settings, nullptr);

    QVERIFY(WardSharing::inviteUser(alice.session(), QString(), false).isEmpty());
    QVERIFY(WardSharing::inviteUser(alice.session(), "   ", true).isEmpty());
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(alice.session().invitesMadeHere().isEmpty());

    QByteArray invite = WardSharing::inviteUser(alice.session(), " Bob ", false);

    QVERIFY(!invite.isEmpty());
    QCOMPARE(alice.session().journal().entries().size(), qsizetype(1));
    QVERIFY(alice.session().invitesMadeHere() == QSet<QByteArray>{invite});
    QVERIFY(*alice.session().document().access().findInvite(invite)->invitedUser()
            == (InvitedUser{AccessRole::Member, "Bob"}));
    QVERIFY(select(alices, "for Bob"));
}
