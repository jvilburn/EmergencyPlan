#pragma once

#include <QObject>

/// Who has access offers each change only where the rules allow it for this device, Save
/// invitation only for invites this device made, and Add a user only to admins.
class WhoHasAccessDialogTest : public QObject
{
    Q_OBJECT

public:
    explicit WhoHasAccessDialogTest(QObject* parent);

private slots:
    void memberSeesAskAnAdminInPlaceOfAddAUser();
    void removeAndRolesAreOfferedOnlyWhereTheRulesAllow();
    void saveInvitationIsOfferedOnlyForInvitesThisDeviceMade();
    void addAUserWithNoAnswerMakesNoInvite();
};
