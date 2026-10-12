#pragma once

#include <QObject>

class DocumentCryptoTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Keys, hashing, and fingerprints
    void generatesDistinctKeypairs();
    void generatesDistinctDataKeys();
    void hashIsStableAndSeparatesDomains();
    void hashRefusesAnOutputSizeOutsideBlake2bRange();
    void fingerprintIsStableAndDistinct();
    void fingerprintRefusesInvalidKeys();
    void publicHalvesDeriveFromTheSecretHalves();
    void derivingRefusesInvalidInput();
    // Sealing the data key
    void theHolderUnsealsTheDataKey();
    void anotherKeypairDoesNotUnseal();
    void twoSealsOfTheSameKeyDiffer();
    void unsealRefusesAMalformedSealedKey();

    // Signing the login challenge
    void signatureVerifiesAndRejectsTampering();
    void signatureFailsWithAnotherDevicesKey();
    void signingRefusesAnInvalidKeyOrAnEmptyChallenge();
};
