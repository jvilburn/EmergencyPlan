#pragma once

#include <QObject>

/// The wards on their way to this device: asked for, granted, or listed after a link.
class IncomingWardsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aGrantedRequestArrivesOnceTheWardIsSealedToThisDevice();
    void aDeclinedRequestIsDroppedAndSaysSo();
    void aRequestOutlivesTheApp();
    void aRequestForNoSuchWardFails();
    void aRequestTheServerNoLongerHasIsDropped();
    void aLinkedDevicesWardsArriveOnceItsLinkerSealsThem();
};
