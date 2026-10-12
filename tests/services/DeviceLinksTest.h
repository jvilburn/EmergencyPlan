#pragma once

#include <QObject>

/// Approving a new device's link from a device already in use, and unlinking.
class DeviceLinksTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void approvingALinkRemembersTheDeviceAndSealsItEverywhere();
    void anUnknownCodeFails();
    void unlinkingDropsTheDeviceAndCutsItOff();
};
