#pragma once

#include <QObject>

class DocumentCryptoTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // 1.2 — key generation, signing, and fingerprints
    void generatesDistinctKeypairs();
    void generatesDistinctDataKeys();
    void hashIsStableAndSeparatesDomains();
    void hashRefusesAnOutputSizeOutsideBlake2bRange();
    void fingerprintIsStableAndDistinct();
    void fingerprintRefusesInvalidKeys();
    // 1.3 — sealing the data key
    void theHolderUnsealsTheDataKey();
    void anotherKeypairDoesNotUnseal();
    void twoSealsOfTheSameKeyDiffer();
    void unsealRefusesAMalformedSealedKey();

    void signatureVerifiesAndRejectsTampering();
    void signatureUnderOneTagFailsUnderEveryOther();
    void signatureFailsWithAnotherDevicesKey();
    void signingRefusesAnInvalidKey();
};
