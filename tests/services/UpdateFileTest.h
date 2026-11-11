#pragma once

#include <QObject>

/// Wards shared through update files with no server: two and three devices, each a full
/// TestClient, exchanging the update files their sessions write.
class UpdateFileTest : public QObject
{
    Q_OBJECT

public:
    explicit UpdateFileTest(QObject* parent);

private slots:
    void updateFileIsTheSignedNextVersion();
    void fileNameIsTheWardAndTheDay();
    void changesOnEachSideExchangedBothWaysConverge();
    void threeDevicesConverge();
    void crossedUpdatesConvergeOnTheHigherWriter();
    void updateNotAboveTheHeldVersionChangesNothing();
    void updateFromADeviceNotOnTheAccessListIsRefused();
    void updateWhoseSignatureDoesNotVerifyIsRefused();
    void conflictDuringTheReplayIsHandedBack();
    void landedMarkDropsWhatLandedAndKeepsWhatCameAfter();
    void takingAVersionRaisesTheCounterToThisDevicesMark();
    void writtenVersionDropsTheMarksOfDevicesNoLongerListed();
    void versionUnderAnotherKeyMustShareAKeyWithTheHeldOne();
    void versionThatLeavesThisDeviceOutEndsItsShare();
    void updateOfAnotherWardOrFormatOrNotAnUpdateChangesNothing();
};
