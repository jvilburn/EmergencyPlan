#include "FormatVectorsTest.h"

#include "DocumentCrypto.h"
#include "DocumentHeader.h"
#include "SealingSet.h"
#include "ServerContract.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QtTest>

#include <sodium.h>

namespace
{

/// Bytes first, first + 1, ... - fixed inputs anyone can reproduce.
QByteArray run(int first, int count)
{
    QByteArray bytes;
    for (int index = 0; index < count; ++index)
    {
        bytes.append(char(first + index));
    }
    return bytes;
}

PublicKey vectorPublicKey()
{
    return PublicKey{run(0x01, PublicKey::Size)};
}

VerifyKey vectorVerifyKey()
{
    return VerifyKey{run(0x21, VerifyKey::Size)};
}

/// One recipient, with every field fixed.
DocumentHeader vectorHeader()
{
    DocumentHeader header;
    header.documentId = "00112233445566778899aabbccddeeff";
    header.saveCounter = 42;
    header.nonce = QByteArray(DocumentCrypto::NonceSize, '\x55');
    header.recipients = {RecipientEntry{DocumentCrypto::fingerprintOf(vectorPublicKey(), vectorVerifyKey()),
                                        QByteArray(DocumentCrypto::SealedDataKeySize, '\xaa')}};
    return header;
}

QByteArray sha256Hex(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

/// The former-key hash as the sealing set writes it: wp-former-key BLAKE2b, 32 bytes.
QString formerKeyHashOf(const DataKey& key)
{
    MemberId person;
    person.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    const DeviceKeys keys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
    SealingSet set = SealingSet::of(person, "Alice", NamedDevice{"Laptop", keys});
    set.addFormerKey(key);
    return set.toJson()["formerKeys"].toArray().first().toString();
}

// Unchanged since format 2.
const QByteArray FingerprintVector = "a8be42168ae4429c3b61d6a9107588e3";
const QByteArray PayloadHashVector = "d669c108b6b3d6d3a10956ce1d7ac55cde6223ab30012d8151de02810e129397";

// Computed independently from the format description with Python's hashlib (BLAKE2b
// with the personalisation, and each field length-prefixed), and matched by this
// build: what a reimplementation must reproduce.
const int AssociatedDataLength = 256;
const QByteArray AssociatedDataSha256Vector = "b919c0e9cf84b737673ca0a9deedc6c1be126c26981b0762e79987eadccfd378";
const QString FormerKeyHashVector = "36d056d33e83c4b4103e2fc65f56b4b227db717d529a430782316f9c1a8aa255";

const QByteArray VectorCiphertext = "ciphertext bytes for the vector";

// What an upload of vectorHeader() and VectorCiphertext on base version 7 signs: the
// call, the id, the base version, then BLAKE2b-256 under "wp-signed" of the
// header's compact JSON with sorted keys and of the blob, as hex, each line ending in
// a newline. Computed independently with Python's hashlib and json.
const QByteArray SignedUploadVector = "upload\n00112233445566778899aabbccddeeff\n7\n"
                                      "15db911a9fc4a6657d9852a7f980ea46269e6eec379cf3ba479a29da57c3f162\n"
                                      "28be0d934451700e87f0fc3d0d0e2582aeefe2eeeead9ca25531bd544ff63402\n";
const QByteArray SignedLeaseVector = "takeLease\n00112233445566778899aabbccddeeff\n";
const DataKey VectorDataKey{run(0x41, DataKey::Size)};

} // namespace

void FormatVectorsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void FormatVectorsTest::fingerprintMatchesTheVector()
{
    const Fingerprint fingerprint = DocumentCrypto::fingerprintOf(vectorPublicKey(), vectorVerifyKey());
    QCOMPARE(fingerprint.bytes.toHex(), FingerprintVector);
}

void FormatVectorsTest::payloadHashMatchesTheVector()
{
    QCOMPARE(DocumentCrypto::payloadHash(VectorCiphertext).toHex(), PayloadHashVector);
}

void FormatVectorsTest::associatedDataMatchesTheVector()
{
    const QByteArray associated = vectorHeader().associatedData();
    QCOMPARE(associated.size(), qsizetype(AssociatedDataLength));
    QCOMPARE(sha256Hex(associated), AssociatedDataSha256Vector);

    // The first field as it appears: a four-byte big-endian length, then "4".
    QCOMPARE(associated.left(5).toHex(), QByteArray("0000000134"));
}

void FormatVectorsTest::formerKeyHashMatchesTheVector()
{
    QCOMPARE(formerKeyHashOf(VectorDataKey), FormerKeyHashVector);
    QCOMPARE(FormerKeyHashVector,
             QString::fromLatin1(DocumentCrypto::hash(HashDomain::FormerKey, VectorDataKey.bytes, 32).toHex()));
}

void FormatVectorsTest::signedBytesMatchTheVector()
{
    const ServerContract::Upload upload{vectorHeader(), VectorCiphertext};
    QCOMPARE(ServerContract::signedBytes(ServerContract::CallUpload, vectorHeader().documentId, 7, upload),
             SignedUploadVector);
    QCOMPARE(ServerContract::signedBytes(ServerContract::CallTakeLease, vectorHeader().documentId), SignedLeaseVector);
}

void FormatVectorsTest::aSignatureIsPlainEd25519OverTheSignedBytes()
{
    // Plain Ed25519 over the signed bytes, nothing prepended: what the server checks.
    const SigningKeypair keys = DocumentCrypto::signingKeypairFromSeed(run(0x61, DocumentCrypto::SigningSeedSize));
    const QByteArray signature = DocumentCrypto::sign(SignedUploadVector, keys.signingKey);

    QCOMPARE(signature.size(), qsizetype(64));
    QCOMPARE(crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(signature.constData()),
                                         reinterpret_cast<const unsigned char*>(SignedUploadVector.constData()),
                                         quint64(SignedUploadVector.size()),
                                         reinterpret_cast<const unsigned char*>(keys.verifyKey.bytes.constData())),
             0);
}

void FormatVectorsTest::eachOracleRejectsACorruptedByte()
{
    // Each comparison above must be able to fail: change one byte of each input and
    // the result moves off its vector.
    const PublicKey otherKey{run(0x02, PublicKey::Size)};
    QVERIFY(DocumentCrypto::fingerprintOf(otherKey, vectorVerifyKey()).bytes.toHex() != FingerprintVector);

    QByteArray otherCiphertext = VectorCiphertext;
    otherCiphertext[0] = char(otherCiphertext.at(0) ^ 0x01);
    QVERIFY(DocumentCrypto::payloadHash(otherCiphertext).toHex() != PayloadHashVector);

    DocumentHeader otherHeader = vectorHeader();
    otherHeader.saveCounter = 43;
    QVERIFY(sha256Hex(otherHeader.associatedData()) != AssociatedDataSha256Vector);

    DataKey otherKeyValue = VectorDataKey;
    otherKeyValue.bytes[0] = char(otherKeyValue.bytes.at(0) ^ 0x01);
    QVERIFY(formerKeyHashOf(otherKeyValue) != FormerKeyHashVector);

    ServerContract::Upload otherUpload{vectorHeader(), VectorCiphertext};
    otherUpload.blob[0] = char(otherUpload.blob.at(0) ^ 0x01);
    QVERIFY(ServerContract::signedBytes(ServerContract::CallUpload, vectorHeader().documentId, 7, otherUpload)
            != SignedUploadVector);
    otherUpload = ServerContract::Upload{otherHeader, VectorCiphertext};
    QVERIFY(ServerContract::signedBytes(ServerContract::CallUpload, vectorHeader().documentId, 7, otherUpload)
            != SignedUploadVector);

    const SigningKeypair keys = DocumentCrypto::signingKeypairFromSeed(run(0x61, DocumentCrypto::SigningSeedSize));
    QByteArray signedBytes = SignedUploadVector;
    const QByteArray signature = DocumentCrypto::sign(signedBytes, keys.signingKey);
    QVERIFY(DocumentCrypto::verifies(signedBytes, signature, keys.verifyKey));
    signedBytes[0] = char(signedBytes.at(0) ^ 0x01);
    QVERIFY(!DocumentCrypto::verifies(signedBytes, signature, keys.verifyKey));
}
