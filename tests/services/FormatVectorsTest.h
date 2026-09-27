#pragma once

#include <QObject>

/// Known-answer vectors for the document format.
///
/// Every other test compares the code with itself, so a reordered associated-data
/// field or a renamed personalisation string would change both sides and stay
/// green while every existing file and signature became unreadable. These pin the
/// bytes. The expected values were computed by an independent implementation of
/// the format (Python's hashlib and struct), not by this code.
class FormatVectorsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void fingerprintMatchesTheVector();
    void payloadHashMatchesTheVector();
    void associatedDataAndSignedBytesMatchTheVectors();
    void pairingCommitAndNumberMatchTheVectors();
    void signaturesAreMadeOverTheTagThenAZeroByte();
    void requestSignaturesCoverTheRouteDocumentAndBodyHash();
    void eachOracleRejectsACorruptedByte();
};
