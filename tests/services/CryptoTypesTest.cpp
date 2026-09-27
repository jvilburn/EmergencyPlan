#include "CryptoTypesTest.h"

#include "CryptoTypes.h"

#include <QtTest>

namespace
{

const QByteArray SampleBytes = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
const QString SampleHex = "000102030405060708090a0b0c0d0e0f";
const QString SampleGrouped = "0001-0203-0405-0607-0809-0a0b-0c0d-0e0f";

Fingerprint sampleFingerprint()
{
    Fingerprint fingerprint;
    fingerprint.bytes = SampleBytes;
    return fingerprint;
}

} // namespace

void CryptoTypesTest::defaultConstructedValuesAreInvalid()
{
    QVERIFY(!PublicKey().isValid());
    QVERIFY(!SecretKey().isValid());
    QVERIFY(!VerifyKey().isValid());
    QVERIFY(!SigningKey().isValid());
    QVERIFY(!DataKey().isValid());
    QVERIFY(!Fingerprint().isValid());
    QVERIFY(!MemberId().isValid());
}

void CryptoTypesTest::aValueIsValidOnlyAtItsOwnSize()
{
    // The sizes differ, so a value of one type's length is not automatically
    // valid as another's. SigningKey in particular is 64, not 32.
    QCOMPARE(SigningKey::Size, 64);

    SigningKey signing;
    signing.bytes = QByteArray(PublicKey::Size, '\0');
    QVERIFY(!signing.isValid());

    signing.bytes = QByteArray(SigningKey::Size, '\0');
    QVERIFY(signing.isValid());

    Fingerprint fingerprint;
    fingerprint.bytes = QByteArray(Fingerprint::Size - 1, '\0');
    QVERIFY(!fingerprint.isValid());

    fingerprint.bytes = QByteArray(Fingerprint::Size + 1, '\0');
    QVERIFY(!fingerprint.isValid());
}

void CryptoTypesTest::equalityComparesTheBytes()
{
    PublicKey first;
    first.bytes = QByteArray(PublicKey::Size, 'a');

    PublicKey same;
    same.bytes = QByteArray(PublicKey::Size, 'a');

    PublicKey different;
    different.bytes = QByteArray(PublicKey::Size, 'b');

    QVERIFY(first == same);
    QVERIFY(!(first == different));
    QVERIFY(first != different);
}

void CryptoTypesTest::fingerprintHexRoundTrips()
{
    const Fingerprint fingerprint = sampleFingerprint();

    QCOMPARE(fingerprint.toHex(), SampleHex);
    QVERIFY(Fingerprint::fromHex(fingerprint.toHex()) == fingerprint);
}

void CryptoTypesTest::groupedHexIsFourCharacterGroups()
{
    const QString grouped = sampleFingerprint().toGroupedHex();

    QCOMPARE(grouped, SampleGrouped);

    const QStringList groups = grouped.split(QLatin1Char('-'));
    QCOMPARE(groups.size(), qsizetype(Fingerprint::Size * 2 / 4));
    for (const QString& group : groups)
    {
        QCOMPARE(group.size(), qsizetype(4));
    }
}

void CryptoTypesTest::fingerprintFromHexAcceptsTheGroupedForm()
{
    // Someone reading a fingerprint aloud is looking at the grouped form, so
    // that is what gets typed back in.
    QVERIFY(Fingerprint::fromHex(SampleGrouped) == sampleFingerprint());
}

void CryptoTypesTest::fingerprintFromHexRefusesMalformedInput()
{
    QVERIFY(!Fingerprint::fromHex(QString()).isValid());
    QVERIFY(!Fingerprint::fromHex("").isValid());

    // One character short, and one too long.
    QVERIFY(!Fingerprint::fromHex(SampleHex.left(SampleHex.size() - 1)).isValid());
    QVERIFY(!Fingerprint::fromHex(SampleHex + QLatin1Char('0')).isValid());

    // Right length, but not hex. QByteArray::fromHex would skip these and
    // return a short array rather than refusing.
    QString nonHex = SampleHex;
    nonHex.replace(0, 2, QString("zz"));
    QCOMPARE(nonHex.size(), SampleHex.size());
    QVERIFY(!Fingerprint::fromHex(nonHex).isValid());

    // A separator in the wrong place does not rescue a short value.
    QVERIFY(!Fingerprint::fromHex("0001-0203").isValid());
}

void CryptoTypesTest::memberIdHexRoundTrips()
{
    MemberId member;
    member.bytes = SampleBytes;

    QCOMPARE(member.toHex(), SampleHex);
    QVERIFY(MemberId::fromHex(member.toHex()) == member);
}

void CryptoTypesTest::memberIdFromHexRefusesMalformedInput()
{
    QVERIFY(!MemberId::fromHex(QString()).isValid());
    QVERIFY(!MemberId::fromHex(SampleHex.left(SampleHex.size() - 1)).isValid());
    QVERIFY(!MemberId::fromHex(SampleHex + QLatin1Char('0')).isValid());

    QString nonHex = SampleHex;
    nonHex.replace(0, 2, QString("zz"));
    QVERIFY(!MemberId::fromHex(nonHex).isValid());
}
