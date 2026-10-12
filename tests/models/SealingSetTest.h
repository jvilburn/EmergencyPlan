#pragma once

#include <QObject>

/// The devices a document's data key is sealed to, and the rule every write applies
/// to them.
class SealingSetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // The rule
    void keepingEveryListedDeviceKeepsTheKey();
    void droppingADeviceRotates();
    void anApprovedListedDeviceIsAdded();
    void anApprovedDeviceTheServerDoesNotListIsNotAdded();
    void aDeviceOnlyTheServerListsIsNeverAdded();
    void formerKeysCarryOver();

    // The set
    void aDeviceSharingAKeyIsRefused();
    void addingADeviceTwiceKeepsOne();
    void devicesComeInFingerprintOrder();
    void ofInvalidKeysIsEmpty();

    // JSON
    void roundTripsThroughJson();
    void fromJsonRefusesAnythingIllFormed();
};
