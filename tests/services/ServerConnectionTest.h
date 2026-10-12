#pragma once

#include <QObject>

/// One device's way to the server: writes signed as this device, or as the invite
/// it holds while joining, and replies dropped with their context.
class ServerConnectionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void writesAreSignedByThisDevice();
    void aDeviceSigningWithAnotherKeyIsRefused();
    void uploadAsSignsAsTheWriterGiven();
    void aReplyWhoseContextWasDestroyedIsDropped();
};
