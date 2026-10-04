#pragma once

#include <QObject>

/// The device's secret in the OS keychain. The keychain tests use a real keychain
/// under their own account, and skip where none is available.
class KeyStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    // The record, with no keychain
    void aRecordRoundTripsWithAndWithoutAPerson();
    void aRecordWithAKeyOfTheWrongSizeIsRefused();
    void aRecordWithAnUnknownFieldIsRefused();

    // The keychain
    void aStoredSecretLoadsBack();
    void aMissingSecretIsNotFound();
    void anIdentityRecordRoundTripsThroughTheKeychain();
    void removingAMissingSecretStillAnswers();
};
