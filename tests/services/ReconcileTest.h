#pragma once

#include <QObject>

/// Whether a version may become this device's local file.
class ReconcileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Accept
    void aValidVersionIsAccepted();
    void aVersionUnderATrustedKeyIsAccepted();
    void aVersionNamingATrustedKeyAmongItsFormerKeysIsAccepted();
    void withNothingTrustedTheFirstVersionIsTakenAsItStands();

    // Each failure is a ServerFault with nothing else set
    void aVersionNotDescendedFromATrustedKeyIsAServerFault();
    void aVersionNotSealedToThisDeviceIsAServerFault();
    void anEntryThisDeviceCannotUnsealIsAServerFault();
    void aTamperedHeaderIsAServerFault();
    void aVersionOfAnotherDocumentIsAServerFault();
    void aHeaderDisagreeingWithItsSealingSetIsAServerFault();
    void anUnreadablePayloadIsAServerFault();

    // Newer versions
    void aNewerHeaderFormatIsNeedsNewerVersion();
    void aMalformedHeaderIsAServerFault();
};
