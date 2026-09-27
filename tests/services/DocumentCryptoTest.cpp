#include "DocumentCryptoTest.h"

#include "DocumentCrypto.h"

#include <QtTest>

namespace
{

/// Every tag, so a test can assert across all of them and fail when a new tag is
/// added without being considered here.
const QList<SignatureTag> AllTags = {
    SignatureTag::Auth,
    SignatureTag::Request,
    SignatureTag::Header,
    SignatureTag::Pairing
};

} // namespace

void DocumentCryptoTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentCryptoTest::generatesDistinctKeypairs()
{
    const Keypair first = DocumentCrypto::generateKeypair();
    const Keypair second = DocumentCrypto::generateKeypair();

    QVERIFY(first.publicKey.isValid());
    QVERIFY(first.secretKey.isValid());
    QVERIFY(!(first.publicKey == second.publicKey));
    QVERIFY(!(first.secretKey == second.secretKey));

    const SigningKeypair firstSigning = DocumentCrypto::generateSigningKeypair();
    const SigningKeypair secondSigning = DocumentCrypto::generateSigningKeypair();

    QVERIFY(firstSigning.verifyKey.isValid());
    QVERIFY(firstSigning.signingKey.isValid());
    QVERIFY(!(firstSigning.verifyKey == secondSigning.verifyKey));
}

void DocumentCryptoTest::generatesDistinctDataKeys()
{
    const DataKey first = DocumentCrypto::generateDataKey();
    const DataKey second = DocumentCrypto::generateDataKey();

    QVERIFY(first.isValid());
    QVERIFY(!(first == second));
}

void DocumentCryptoTest::hashIsStableAndSeparatesDomains()
{
    const QByteArray material = "the same bytes in every domain";

    const QList<HashDomain> domains = {
        HashDomain::Fingerprint,
        HashDomain::Payload,
        HashDomain::PairingCommit,
        HashDomain::PairingComparison
    };

    // Same length as well as same material, so the digests are told apart by the
    // personalisation alone and not by BLAKE2b's output-length field.
    const int outputSize = 32;

    QList<QByteArray> digests;
    for (const HashDomain domain : domains)
    {
        const QByteArray digest = DocumentCrypto::hash(domain, material, outputSize);

        QCOMPARE(digest.size(), qsizetype(outputSize));
        QCOMPARE(digest, DocumentCrypto::hash(domain, material, outputSize));
        QVERIFY(!digests.contains(digest));

        digests.append(digest);
    }

    QCOMPARE(digests.size(), domains.size());
}

void DocumentCryptoTest::hashRefusesAnOutputSizeOutsideBlake2bRange()
{
    // BLAKE2b accepts 16 through 64 bytes.
    const QByteArray material = "material";

    QVERIFY(DocumentCrypto::hash(HashDomain::Payload, material, 15).isEmpty());
    QVERIFY(DocumentCrypto::hash(HashDomain::Payload, material, 65).isEmpty());
    QVERIFY(DocumentCrypto::hash(HashDomain::Payload, material, 0).isEmpty());

    QVERIFY(!DocumentCrypto::hash(HashDomain::Payload, material, 16).isEmpty());
    QVERIFY(!DocumentCrypto::hash(HashDomain::Payload, material, 64).isEmpty());
}

void DocumentCryptoTest::fingerprintIsStableAndDistinct()
{
    const Keypair first = DocumentCrypto::generateKeypair();
    const Keypair second = DocumentCrypto::generateKeypair();

    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();
    const SigningKeypair otherSigning = DocumentCrypto::generateSigningKeypair();

    QVERIFY(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey)
            == DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey));

    // Either key changing changes the fingerprint, which is what binds a device's
    // two public keys to one identity.
    QVERIFY(!(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey)
              == DocumentCrypto::fingerprintOf(second.publicKey, signing.verifyKey)));
    QVERIFY(!(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey)
              == DocumentCrypto::fingerprintOf(first.publicKey, otherSigning.verifyKey)));

    QCOMPARE(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey).bytes.size(),
             qsizetype(Fingerprint::Size));
}

void DocumentCryptoTest::fingerprintRefusesInvalidKeys()
{
    const Keypair keys = DocumentCrypto::generateKeypair();
    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();

    QVERIFY(!DocumentCrypto::fingerprintOf(PublicKey(), signing.verifyKey).isValid());
    QVERIFY(!DocumentCrypto::fingerprintOf(keys.publicKey, VerifyKey()).isValid());
}

void DocumentCryptoTest::theHolderUnsealsTheDataKey()
{
    const Keypair keys = DocumentCrypto::generateKeypair();
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    const QByteArray sealed = DocumentCrypto::sealDataKey(dataKey, keys.publicKey);
    QCOMPARE(sealed.size(), qsizetype(DocumentCrypto::SealedDataKeySize));

    const std::optional<DataKey> opened = DocumentCrypto::unsealDataKey(sealed, keys);
    QVERIFY(opened.has_value());
    QVERIFY(opened.value() == dataKey);
}

void DocumentCryptoTest::anotherKeypairDoesNotUnseal()
{
    const Keypair keys = DocumentCrypto::generateKeypair();
    const Keypair other = DocumentCrypto::generateKeypair();
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    const QByteArray sealed = DocumentCrypto::sealDataKey(dataKey, keys.publicKey);

    // This is what a reader hits on every recipient entry but its own, so it is
    // an ordinary answer rather than a failure.
    QVERIFY(!DocumentCrypto::unsealDataKey(sealed, other).has_value());
}

void DocumentCryptoTest::twoSealsOfTheSameKeyDiffer()
{
    const Keypair keys = DocumentCrypto::generateKeypair();
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    const QByteArray first = DocumentCrypto::sealDataKey(dataKey, keys.publicKey);
    const QByteArray second = DocumentCrypto::sealDataKey(dataKey, keys.publicKey);

    // A fresh ephemeral sender keypair each time, so resealing an unchanged key
    // never reveals that it is unchanged.
    QVERIFY(first != second);
    QVERIFY(DocumentCrypto::unsealDataKey(first, keys).value() == dataKey);
    QVERIFY(DocumentCrypto::unsealDataKey(second, keys).value() == dataKey);
}

void DocumentCryptoTest::unsealRefusesAMalformedSealedKey()
{
    const Keypair keys = DocumentCrypto::generateKeypair();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const QByteArray sealed = DocumentCrypto::sealDataKey(dataKey, keys.publicKey);

    QVERIFY(!DocumentCrypto::unsealDataKey(QByteArray(), keys).has_value());
    QVERIFY(!DocumentCrypto::unsealDataKey(sealed.left(sealed.size() - 1), keys).has_value());
    QVERIFY(!DocumentCrypto::unsealDataKey(sealed + QByteArray(1, '\0'), keys).has_value());

    QByteArray corrupted = sealed;
    corrupted[0] = static_cast<char>(corrupted.at(0) ^ 0xff);
    QVERIFY(!DocumentCrypto::unsealDataKey(corrupted, keys).has_value());

    // Sealing needs a real recipient key.
    QVERIFY(DocumentCrypto::sealDataKey(dataKey, PublicKey()).isEmpty());
    QVERIFY(DocumentCrypto::sealDataKey(DataKey(), keys.publicKey).isEmpty());
}

void DocumentCryptoTest::signatureVerifiesAndRejectsTampering()
{
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray message = "challenge";
    const QByteArray signature = DocumentCrypto::sign(SignatureTag::Auth, message, keys.signingKey);

    QCOMPARE(signature.size(), qsizetype(DocumentCrypto::SignatureSize));
    QVERIFY(DocumentCrypto::verify(SignatureTag::Auth, message, signature, keys.verifyKey));

    QVERIFY(!DocumentCrypto::verify(SignatureTag::Header, message, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Auth, "challenqe", signature, keys.verifyKey));

    // A truncated or lengthened signature is refused before it reaches libsodium.
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Auth,
                                    message,
                                    signature.left(signature.size() - 1),
                                    keys.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Auth,
                                    message,
                                    signature + QByteArray(1, '\0'),
                                    keys.verifyKey));
}

void DocumentCryptoTest::signatureUnderOneTagFailsUnderEveryOther()
{
    // A signature the server can influence (Auth) must never verify as document
    // evidence (Header), and no tag may stand in for another.
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray message = "the same bytes under every tag";

    for (const SignatureTag signedUnder : AllTags)
    {
        const QByteArray signature = DocumentCrypto::sign(signedUnder, message, keys.signingKey);
        QVERIFY(!signature.isEmpty());

        for (const SignatureTag checkedUnder : AllTags)
        {
            const bool verified =
                DocumentCrypto::verify(checkedUnder, message, signature, keys.verifyKey);
            QCOMPARE(verified, signedUnder == checkedUnder);
        }
    }
}

void DocumentCryptoTest::signatureFailsWithAnotherDevicesKey()
{
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const SigningKeypair other = DocumentCrypto::generateSigningKeypair();
    const QByteArray message = "a header";

    const QByteArray signature =
        DocumentCrypto::sign(SignatureTag::Header, message, keys.signingKey);

    QVERIFY(DocumentCrypto::verify(SignatureTag::Header, message, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Header, message, signature, other.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Header, message, signature, VerifyKey()));
}

void DocumentCryptoTest::signingRefusesAnInvalidKey()
{
    // A 32-byte value is a plausible mistake here, since every other key is 32.
    SigningKey tooShort;
    tooShort.bytes = QByteArray(32, 'k');

    QVERIFY(DocumentCrypto::sign(SignatureTag::Header, "message", tooShort).isEmpty());
    QVERIFY(DocumentCrypto::sign(SignatureTag::Header, "message", SigningKey()).isEmpty());
}
