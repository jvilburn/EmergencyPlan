#pragma once

#include <QObject>

/// Where a .wardplanning file opened by path goes, by its header, and the invite
/// links that join a ward.
class FileOpenRouterTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void eachFileTakesItsRoute_data();
    void eachFileTakesItsRoute();
    void anInviteLinkRoundTripsThroughItsText();
    void aMalformedInviteLinkIsNotOne_data();
    void aMalformedInviteLinkIsNotOne();
    void theInviteMessageSaysWhoWhereAndHow();
};
