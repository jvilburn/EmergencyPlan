#include "DocumentCryptoTest.h"

#include "DocumentCrypto.h"

#include <QtTest>

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
        HashDomain::FormerKey,
        HashDomain::SignedWrite,
        HashDomain::InviteSealing,
        HashDomain::InviteSigning
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

void DocumentCryptoTest::publicHalvesDeriveFromTheSecretHalves()
{
    const Keypair sealing = DocumentCrypto::generateKeypair();
    const Keypair derived = DocumentCrypto::keypairFromSecret(sealing.secretKey);
    QVERIFY(derived.publicKey == sealing.publicKey);
    QVERIFY(derived.secretKey == sealing.secretKey);

    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();
    const QByteArray seed = DocumentCrypto::seedOf(signing.signingKey);
    QCOMPARE(seed.size(), qsizetype(DocumentCrypto::SigningSeedSize));
    const SigningKeypair fromSeed = DocumentCrypto::signingKeypairFromSeed(seed);
    QVERIFY(fromSeed.verifyKey == signing.verifyKey);
    QVERIFY(fromSeed.signingKey == signing.signingKey);

    // A different secret derives a different public key.
    QVERIFY(DocumentCrypto::keypairFromSecret(DocumentCrypto::generateKeypair().secretKey).publicKey
            != sealing.publicKey);
}

void DocumentCryptoTest::aSealingKeypairComesFromItsSeed()
{
    const QByteArray seed = DocumentCrypto::randomBytes(DocumentCrypto::SealingSeedSize);
    const Keypair fromSeed = DocumentCrypto::keypairFromSeed(seed);
    QVERIFY(fromSeed.publicKey.isValid());
    QVERIFY(fromSeed.secretKey.isValid());

    // The same seed makes the same keypair, its halves belonging together.
    const Keypair again = DocumentCrypto::keypairFromSeed(seed);
    QVERIFY(again.publicKey == fromSeed.publicKey);
    QVERIFY(again.secretKey == fromSeed.secretKey);
    QVERIFY(DocumentCrypto::keypairFromSecret(fromSeed.secretKey).publicKey == fromSeed.publicKey);

    // A data key sealed to it opens with it, and not with another seed's keypair.
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const QByteArray sealed = DocumentCrypto::sealDataKey(dataKey, fromSeed.publicKey);
    QVERIFY(DocumentCrypto::unsealDataKey(sealed, again) == dataKey);
    QByteArray otherSeed = seed;
    otherSeed[0] = char(otherSeed.at(0) ^ 0x01);
    const Keypair other = DocumentCrypto::keypairFromSeed(otherSeed);
    QVERIFY(other.publicKey != fromSeed.publicKey);
    QVERIFY(!DocumentCrypto::unsealDataKey(sealed, other).has_value());
}

void DocumentCryptoTest::derivingRefusesInvalidInput()
{
    QVERIFY(!DocumentCrypto::keypairFromSeed(QByteArray(5, 'x')).publicKey.isValid());
    QVERIFY(!DocumentCrypto::keypairFromSeed(QByteArray()).secretKey.isValid());
    QVERIFY(!DocumentCrypto::keypairFromSecret(SecretKey()).publicKey.isValid());
    QVERIFY(DocumentCrypto::seedOf(SigningKey()).isEmpty());
    QVERIFY(!DocumentCrypto::signingKeypairFromSeed(QByteArray(5, 'x')).signingKey.isValid());
    QVERIFY(!DocumentCrypto::signingKeypairFromSeed(QByteArray()).verifyKey.isValid());
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
    const QByteArray bytes = "what a write covers";
    const QByteArray signature = DocumentCrypto::sign(bytes, keys.signingKey);

    QCOMPARE(signature.size(), qsizetype(DocumentCrypto::SignatureSize));
    QVERIFY(DocumentCrypto::verifies(bytes, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verifies("what a write coverz", signature, keys.verifyKey));

    QByteArray flipped = signature;
    flipped[0] = char(flipped.at(0) ^ 0x01);
    QVERIFY(!DocumentCrypto::verifies(bytes, flipped, keys.verifyKey));

    // A truncated or lengthened signature is refused before it reaches libsodium.
    QVERIFY(!DocumentCrypto::verifies(bytes, signature.left(signature.size() - 1), keys.verifyKey));
    QVERIFY(!DocumentCrypto::verifies(bytes, signature + QByteArray(1, 'x'), keys.verifyKey));
}

void DocumentCryptoTest::signatureFailsWithAnotherDevicesKey()
{
    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    const SigningKeypair other = DocumentCrypto::generateSigningKeypair();
    const QByteArray bytes = "what a write covers";

    const QByteArray signature = DocumentCrypto::sign(bytes, keys.signingKey);

    QVERIFY(DocumentCrypto::verifies(bytes, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verifies(bytes, signature, other.verifyKey));
    QVERIFY(!DocumentCrypto::verifies(bytes, signature, VerifyKey()));
}

void DocumentCryptoTest::signingRefusesAnInvalidKeyOrNoBytes()
{
    // A 32-byte value is a plausible mistake here, since every other key is 32.
    SigningKey tooShort;
    tooShort.bytes = QByteArray(32, 'k');

    QVERIFY(DocumentCrypto::sign("what a write covers", tooShort).isEmpty());
    QVERIFY(DocumentCrypto::sign("what a write covers", SigningKey()).isEmpty());

    const SigningKeypair keys = DocumentCrypto::generateSigningKeypair();
    QVERIFY(DocumentCrypto::sign(QByteArray(), keys.signingKey).isEmpty());
    QVERIFY(!DocumentCrypto::sign("what a write covers", keys.signingKey).isEmpty());
}
