#include "SendInvitationDialogTest.h"

#include "SendInvitationDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

SendInvitationDialogTest::SendInvitationDialogTest(QObject* parent)
    : QObject(parent)
{
}

void SendInvitationDialogTest::instructionsAddressTheInvitingUserAndTheMessageTheInvitee()
{
    QCOMPARE(SendInvitationDialog::instructions("Paula"),
             QString("Your invitation for Paula is ready. Attach it to an email or text to Paula, with this "
                     "message:"));
    QCOMPARE(SendInvitationDialog::message("John", "Tanglewood Ward"),
             QString("John has invited you to Tanglewood Ward. Install Ward Planning, then open the attached "
                     "invitation."));
}

void SendInvitationDialogTest::copyMessageCopiesTheMessageAlone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SendInvitationDialog dialog("Paula", "John", "Tanglewood Ward",
                                dir.filePath("Tanglewood Ward - for Paula.wardinvite"), nullptr);
    QGuiApplication::clipboard()->setText("something else");

    // Both texts are shown, each in its own label.
    QStringList shown;
    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    for (const QLabel* label : labels)
    {
        shown.append(label->text());
    }
    QVERIFY(shown.contains(SendInvitationDialog::instructions("Paula")));
    QVERIFY(shown.contains(SendInvitationDialog::message("John", "Tanglewood Ward")));

    QPushButton* copyMessage = nullptr;
    const QList<QPushButton*> buttons = dialog.findChildren<QPushButton*>();
    for (QPushButton* button : buttons)
    {
        if (button->text() == "Copy Message")
        {
            copyMessage = button;
        }
    }
    QVERIFY(copyMessage != nullptr);
    copyMessage->click();

    QCOMPARE(QGuiApplication::clipboard()->text(), SendInvitationDialog::message("John", "Tanglewood Ward"));
}
