#pragma once

#include <QObject>

/// Known-answer vectors for the parts of the format the companion server and any
/// later reader must reproduce byte for byte: the device fingerprint, the payload
/// hash, a header's associated data, the former-key hash, and what a write signs
/// and how.
class FormatVectorsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void fingerprintMatchesTheVector();
    void payloadHashMatchesTheVector();
    void associatedDataMatchesTheVector();
    void formerKeyHashMatchesTheVector();
    void signedBytesMatchTheVector();
    void aSignatureIsPlainEd25519OverTheSignedBytes();
    void eachOracleRejectsACorruptedByte();
};
