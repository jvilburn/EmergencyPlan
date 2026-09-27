#include "FormatVectorsTest.h"

#include "DocumentCrypto.h"
#include "DocumentHeader.h"
#include "Pairing.h"
#include "ServerContract.h"

#include <QCryptographicHash>
#include <QDateTime>
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

PublicKey writerPublicKey()
{
    return PublicKey{run(0x01, PublicKey::Size)};
}

VerifyKey writerVerifyKey()
{
    return VerifyKey{run(0x21, VerifyKey::Size)};
}

MemberId writerMember()
{
    return MemberId{run(0x10, MemberId::Size)};
}

/// One recipient, who wrote it and is its admin, with every other field fixed.
DocumentHeader vectorHeader()
{
    RecipientEntry entry;
    entry.kind = RecipientKind::X25519Sealed;
    entry.member = writerMember();
    entry.publicKey = writerPublicKey();
    entry.verifyKey = writerVerifyKey();
    entry.fingerprint = DocumentCrypto::fingerprintOf(entry.publicKey, entry.verifyKey);
    entry.sealedKey = QByteArray(DocumentCrypto::SealedDataKeySize, '\xaa');

    DocumentHeader header;
    header.documentId = "00112233445566778899aabbccddeeff";
    header.keyGeneration = 3;
    header.saveCounter = 42;
    header.lastWriter = entry.fingerprint;
    header.writerPublicKey = entry.publicKey;
    header.writerVerifyKey = entry.verifyKey;
    header.lastWriteTime = QDateTime::fromString("2026-09-27T12:34:56.789Z", Qt::ISODateWithMs);
    header.nonce = QByteArray(DocumentCrypto::NonceSize, '\x55');
    header.recipients = {entry};
    header.admins = {entry.member};
    header.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x77');
    return header;
}

QByteArray sha256Hex(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

const QByteArray FingerprintVector = "a8be42168ae4429c3b61d6a9107588e3";
const QByteArray PayloadHashVector =
    "d669c108b6b3d6d3a10956ce1d7ac55cde6223ab30012d8151de02810e129397";
const QByteArray AssociatedDataSha256Vector =
    "7e1e845c0169276ddbd095bb40fe7dde96dd96f02311c5a73a54ce5c80f0bec4";
const QByteArray SignedBytesSha256Vector =
    "49e4c528c9e44dc27999837e4e2239a0330b86960204627023dbda7668bffdc4";
const QByteArray PairingCommitVector =
    "9bff7eea1d0bb18e4a9dd8a8c7a5afb8432635beb852c6f3167f93e402aebe19";
const QString PairingNumberVector = "764 409";

const QByteArray VectorCiphertext = "ciphertext bytes for the vector";

/// The message a request to take the lease on the vector document is signed over:
/// the route, the document id, and the wp-request BLAKE2b of the empty body, each
/// length-prefixed.
const QByteArray TakeLeaseMessageVector =
    "0000001a504f5354202f646f63756d656e74732f7b69647d2f6c65617365"
    "000000203030313132323333343435353636373738383939616162626363646465656666"
    "0000002077436f4937074fe7317986d86ed2f6364d05492034d327545b1899b2bb124e72";

} // namespace

void FormatVectorsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void FormatVectorsTest::fingerprintMatchesTheVector()
{
    const Fingerprint fingerprint = DocumentCrypto::fingerprintOf(writerPublicKey(), writerVerifyKey());
    QCOMPARE(fingerprint.bytes.toHex(), FingerprintVector);
}

void FormatVectorsTest::payloadHashMatchesTheVector()
{
    QCOMPARE(DocumentCrypto::payloadHash(VectorCiphertext).toHex(), PayloadHashVector);
}

void FormatVectorsTest::associatedDataAndSignedBytesMatchTheVectors()
{
    const DocumentHeader header = vectorHeader();

    const QByteArray associated = header.associatedData();
    QCOMPARE(associated.size(), qsizetype(679));
    QCOMPARE(sha256Hex(associated), AssociatedDataSha256Vector);
    QCOMPARE(sha256Hex(header.signedBytes()), SignedBytesSha256Vector);

    // The first field as it appears: a four-byte big-endian length, then "2".
    QCOMPARE(associated.left(5).toHex(), QByteArray("0000000132"));
}

void FormatVectorsTest::pairingCommitAndNumberMatchTheVectors()
{
    const DeviceKeys initiator{writerPublicKey(), writerVerifyKey()};
    const DeviceKeys responder{PublicKey{run(0x41, PublicKey::Size)}, VerifyKey{run(0x61, VerifyKey::Size)}};
    const QByteArray nonce(32, '\x33');

    QCOMPARE(Pairing::commit(initiator, nonce).toHex(), PairingCommitVector);
    QCOMPARE(Pairing::comparisonNumber(initiator, responder, nonce), PairingNumberVector);
}

void FormatVectorsTest::signaturesAreMadeOverTheTagThenAZeroByte()
{
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray message = "a message";

    const QList<QPair<SignatureTag, QByteArray>> tags = {
        {SignatureTag::Auth, "auth"},
        {SignatureTag::Request, "request"},
        {SignatureTag::Header, "header"},
        {SignatureTag::Pairing, "pairing"},
    };

    for (const QPair<SignatureTag, QByteArray>& tag : tags)
    {
        const QByteArray signature = DocumentCrypto::sign(tag.first, message, keys.signingKey);
        QCOMPARE(signature.size(), qsizetype(DocumentCrypto::SignatureSize));

        // Checked with libsodium directly, over bytes spelled out here, so the tag
        // text and the separator are pinned rather than recomputed.
        const QByteArray tagged = tag.second + QByteArray(1, '\0') + message;
        QCOMPARE(crypto_sign_verify_detached(
                     reinterpret_cast<const unsigned char*>(signature.constData()),
                     reinterpret_cast<const unsigned char*>(tagged.constData()),
                     quint64(tagged.size()),
                     reinterpret_cast<const unsigned char*>(keys.verifyKey.bytes.constData())),
                 0);
    }
}

void FormatVectorsTest::requestSignaturesCoverTheRouteDocumentAndBodyHash()
{
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray signature = ServerContract::signRequest(ServerContract::Endpoint::TakeLease,
                                                             "00112233445566778899aabbccddeeff",
                                                             ServerContract::emptyBody(),
                                                             keys.signingKey);

    const QByteArray tagged = QByteArray("request") + QByteArray(1, '\0')
        + QByteArray::fromHex(TakeLeaseMessageVector);
    QCOMPARE(crypto_sign_verify_detached(
                 reinterpret_cast<const unsigned char*>(signature.constData()),
                 reinterpret_cast<const unsigned char*>(tagged.constData()),
                 quint64(tagged.size()),
                 reinterpret_cast<const unsigned char*>(keys.verifyKey.bytes.constData())),
             0);

    // The oracle can fail: the same bytes under another route do not verify.
    QByteArray otherRoute = QByteArray::fromHex(TakeLeaseMessageVector);
    otherRoute.replace(4, 4, "PUT ");
    const QByteArray otherTagged = QByteArray("request") + QByteArray(1, '\0') + otherRoute;
    QVERIFY(crypto_sign_verify_detached(
                reinterpret_cast<const unsigned char*>(signature.constData()),
                reinterpret_cast<const unsigned char*>(otherTagged.constData()),
                quint64(otherTagged.size()),
                reinterpret_cast<const unsigned char*>(keys.verifyKey.bytes.constData()))
            != 0);
}

void FormatVectorsTest::eachOracleRejectsACorruptedByte()
{
    // Each comparison above must be able to fail: flip one byte of each input and
    // the result moves off its vector.
    const PublicKey otherKey{run(0x02, PublicKey::Size)};
    QVERIFY(DocumentCrypto::fingerprintOf(otherKey, writerVerifyKey()).bytes.toHex()
            != FingerprintVector);

    QByteArray otherCiphertext = VectorCiphertext;
    otherCiphertext[0] = char(otherCiphertext.at(0) ^ 0x01);
    QVERIFY(DocumentCrypto::payloadHash(otherCiphertext).toHex() != PayloadHashVector);

    DocumentHeader otherHeader = vectorHeader();
    otherHeader.saveCounter = 43;
    QVERIFY(sha256Hex(otherHeader.associatedData()) != AssociatedDataSha256Vector);

    otherHeader = vectorHeader();
    otherHeader.payloadHash[0] = '\x78';
    QVERIFY(sha256Hex(otherHeader.signedBytes()) != SignedBytesSha256Vector);
    QCOMPARE(sha256Hex(otherHeader.associatedData()), AssociatedDataSha256Vector);

    const DeviceKeys initiator{writerPublicKey(), writerVerifyKey()};
    QByteArray otherNonce(32, '\x33');
    otherNonce[31] = '\x34';
    QVERIFY(Pairing::commit(initiator, otherNonce).toHex() != PairingCommitVector);

    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray signature = DocumentCrypto::sign(SignatureTag::Header, "a message", keys.signingKey);
    const QByteArray untagged = "a message";
    QVERIFY(crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(signature.constData()),
                                        reinterpret_cast<const unsigned char*>(untagged.constData()),
                                        quint64(untagged.size()),
                                        reinterpret_cast<const unsigned char*>(keys.verifyKey.bytes.constData()))
            != 0);
}
