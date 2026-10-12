#pragma once

#include <QObject>

/// Sealing every ward to the devices this device approved, on the server's own
/// version, with the open ward handed to its session.
class SealingQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aLinkSealsTheNewDeviceIntoEveryWard();
    void aWardGrantedAfterALinkIsSealedTooOnTheNextPass();
    void aGrantSealsTheDeviceThatAskedAndLeavesTheRecord();
    void aLeaseHeldElsewhereHoldsNothingOff();
    void anUnreachableServerIsRetriedUntilEveryWardIsDone();
    void aConflictStartsTheWardAgainOnTheNewerVersion();
    void aWardThisDeviceLostIsForgotten();
    void aWardOwingNothingIsLeftUntouched();
};
