#pragma once

#include <QObject>

/// The invite link that joins a ward: its text, its keys, and the message it is sent in.
class InviteLinkTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void anInviteLinkRoundTripsThroughItsText();
    void aMalformedInviteLinkIsNotOne_data();
    void aMalformedInviteLinkIsNotOne();
    void theInviteMessageSaysWhoWhereAndHow();
};
