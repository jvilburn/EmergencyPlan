#include "DocumentHeaderTest.h"

#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QJsonArray>
#include <QtTest>

#include <algorithm>

namespace
{

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::generateDataKey().bytes.left(MemberId::Size);
    return member;
}

QString newDocumentId()
{
    return QString::fromLatin1(
        DocumentCrypto::generateDataKey().bytes.left(DocumentHeader::DocumentIdLength / 2).toHex());
}

RecipientEntry newRecipient(const MemberId& member, const DataKey& dataKey)
{
    const Keypair sealing = DocumentCrypto::generateKeypair();
    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();

    RecipientEntry entry;
    entry.member = member;
    entry.publicKey = sealing.publicKey;
    entry.verifyKey = signing.verifyKey;
    entry.fingerprint = DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
    entry.sealedKey = DocumentCrypto::sealDataKey(dataKey, sealing.publicKey);
    return entry;
}

/// Makes the given recipient the header's writer, keys included.
void setWriter(DocumentHeader& header, const RecipientEntry& writer)
{
    header.lastWriter = writer.fingerprint;
    header.writerPublicKey = writer.publicKey;
    header.writerVerifyKey = writer.verifyKey;
}

/// The admin list in the one order parse accepts.
QList<MemberId> sortedAdmins(QList<MemberId> admins)
{
    std::sort(admins.begin(),
              admins.end(),
              [](const MemberId& left, const MemberId& right)
              {
                  return left.toHex() < right.toHex();
              });
    return admins;
}

/// Replaces the admins array in a header's JSON with the given hex strings.
HeaderParse parseWithAdmins(const DocumentHeader& header, const QStringList& admins)
{
    QJsonObject json = header.toJson();
    json[QLatin1String("admins")] = QJsonArray::fromStringList(admins);

    DocumentHeader out;
    return DocumentHeader::parse(json, out);
}

/// Two recipients under different people, both admins, so order and counts are
/// exercised everywhere.
DocumentHeader sampleHeader()
{
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    DocumentHeader header;
    header.documentId = newDocumentId();
    header.keyGeneration = 4;
    header.saveCounter = 17;
    header.lastWriteTime = QDateTime::currentDateTimeUtc();
    header.nonce = QByteArray(DocumentCrypto::NonceSize, '\x11');
    header.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x22');
    header.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\x33');

    header.recipients.append(newRecipient(newMember(), dataKey));
    header.recipients.append(newRecipient(newMember(), dataKey));
    setWriter(header, header.recipients.first());
    header.admins = sortedAdmins({header.recipients.at(0).member, header.recipients.at(1).member});

    return header;
}

DocumentHeader reparsed(const DocumentHeader& header)
{
    DocumentHeader out;
    if (DocumentHeader::parse(header.toJson(), out) != HeaderParse::Ok)
    {
        qFatal("reparsed: a header this test built did not parse");
    }
    return out;
}

/// Replaces one value in a header's JSON and parses the result.
HeaderParse parseWith(const DocumentHeader& header, const char* key, const QJsonValue& value)
{
    QJsonObject json = header.toJson();
    json[QLatin1String(key)] = value;

    DocumentHeader out;
    return DocumentHeader::parse(json, out);
}

/// Replaces one value inside the first recipient and parses the result.
HeaderParse parseWithRecipient(const DocumentHeader& header,
                               const char* key,
                               const QJsonValue& value)
{
    QJsonObject json = header.toJson();
    QJsonArray recipients = json.value(QLatin1String("recipients")).toArray();

    QJsonObject entry = recipients.at(0).toObject();
    entry[QLatin1String(key)] = value;
    recipients.replace(0, entry);
    json[QLatin1String("recipients")] = recipients;

    DocumentHeader out;
    return DocumentHeader::parse(json, out);
}

} // namespace

void DocumentHeaderTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentHeaderTest::roundTripsThroughJson()
{
    const DocumentHeader header = sampleHeader();

    DocumentHeader out;
    QCOMPARE(DocumentHeader::parse(header.toJson(), out), HeaderParse::Ok);
    QVERIFY(out == header);
}

void DocumentHeaderTest::roundTripKeepsRecipientAndAdminOrder()
{
    DocumentHeader header = sampleHeader();
    header.recipients.append(newRecipient(newMember(), DocumentCrypto::generateDataKey()));
    header.admins = sortedAdmins(header.admins + QList<MemberId>{header.recipients.last().member});
    QCOMPARE(header.admins.size(), 3);

    const DocumentHeader out = reparsed(header);

    QCOMPARE(out.recipients.size(), header.recipients.size());
    QCOMPARE(out.admins.size(), header.admins.size());
    for (qsizetype index = 0; index < header.recipients.size(); ++index)
    {
        QVERIFY(out.recipients.at(index) == header.recipients.at(index));
    }
    for (qsizetype index = 0; index < header.admins.size(); ++index)
    {
        QVERIFY(out.admins.at(index) == header.admins.at(index));
    }
}

void DocumentHeaderTest::isAdminFindsOnlyListedMembers()
{
    DocumentHeader header = sampleHeader();
    header.admins = {header.recipients.at(0).member};

    QVERIFY(header.isAdmin(header.recipients.at(0).member));
    QVERIFY(!header.isAdmin(header.recipients.at(1).member));
    QVERIFY(!header.isAdmin(newMember()));
}

void DocumentHeaderTest::parseRefusesNonCanonicalAdmins()
{
    const DocumentHeader header = sampleHeader();
    const QString low = header.admins.at(0).toHex();
    const QString high = header.admins.at(1).toHex();

    // The canonical spelling parses, and so does an empty set.
    QCOMPARE(parseWithAdmins(header, {low, high}), HeaderParse::Ok);
    QCOMPARE(parseWithAdmins(header, {}), HeaderParse::Ok);

    // One admin set has one spelling, so it has one associated data.
    QCOMPARE(parseWithAdmins(header, {high, low}), HeaderParse::Malformed);
    QCOMPARE(parseWithAdmins(header, {low, low}), HeaderParse::Malformed);
    QCOMPARE(parseWithAdmins(header, {low.toUpper(), high.toUpper()}), HeaderParse::Malformed);

    QCOMPARE(parseWithAdmins(header, {QLatin1String("0011")}), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "admins", QJsonValue(QJsonArray{42})), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesWriterKeysThatAreNotTheLastWriters()
{
    const DocumentHeader header = sampleHeader();
    const RecipientEntry other = header.recipients.at(1);

    // Well-formed keys, but someone else's: a header must not vouch for itself
    // with keys its named writer does not hold.
    QCOMPARE(parseWith(header,
                       "writerPublicKey",
                       QJsonValue(QString::fromLatin1(other.publicKey.bytes.toHex()))),
             HeaderParse::Malformed);
    QCOMPARE(parseWith(header,
                       "writerVerifyKey",
                       QJsonValue(QString::fromLatin1(other.verifyKey.bytes.toHex()))),
             HeaderParse::Malformed);

    // Both keys consistent with a different writer, while lastWriter still
    // names the first.
    QJsonObject json = header.toJson();
    json[QLatin1String("writerPublicKey")] = QString::fromLatin1(other.publicKey.bytes.toHex());
    json[QLatin1String("writerVerifyKey")] = QString::fromLatin1(other.verifyKey.bytes.toHex());
    DocumentHeader out;
    QCOMPARE(DocumentHeader::parse(json, out), HeaderParse::Malformed);

    // And the positive: swapping all three together is a consistent header.
    json[QLatin1String("lastWriter")] = other.fingerprint.toHex();
    QCOMPARE(DocumentHeader::parse(json, out), HeaderParse::Ok);
    QVERIFY(out.lastWriter == other.fingerprint);
}

void DocumentHeaderTest::omitsOriginDocumentIdWhenAbsent()
{
    DocumentHeader header = sampleHeader();
    QVERIFY(header.originDocumentId.isEmpty());
    QVERIFY(!header.toJson().contains(QLatin1String("originDocumentId")));

    header.originDocumentId = newDocumentId();
    QVERIFY(header.toJson().contains(QLatin1String("originDocumentId")));
    QCOMPARE(reparsed(header).originDocumentId, header.originDocumentId);
}

void DocumentHeaderTest::parseRefusesMissingOrIllFormedFields()
{
    const DocumentHeader header = sampleHeader();
    const QJsonObject complete = header.toJson();

    // Every field is required. Removing any one of them is a refusal, not a
    // default.
    for (const QString& key : complete.keys())
    {
        if (key == QLatin1String("originDocumentId"))
        {
            continue;
        }

        QJsonObject json = complete;
        json.remove(key);

        DocumentHeader out;
        QCOMPARE(DocumentHeader::parse(json, out), HeaderParse::Malformed);
    }

    // Wrong JSON types, and values outside their permitted range.
    QCOMPARE(parseWith(header, "formatVersion", QJsonValue(QLatin1String("2"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "keyGeneration", QJsonValue(0)), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "saveCounter", QJsonValue(-1)), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "recipients", QJsonValue(QJsonObject())), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "admins", QJsonValue(42)), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "documentId", QJsonValue(QLatin1String("not-hex"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "lastWriteTime", QJsonValue(QLatin1String("yesterday"))),
             HeaderParse::Malformed);

    // Right length, wrong alphabet.
    QCOMPARE(parseWith(header, "documentId", QJsonValue(QString(32, QLatin1Char('z')))),
             HeaderParse::Malformed);

    // Right length, right alphabet, wrong case. documentId is the one hex field
    // carried as a string rather than decoded to bytes, and it is compared as a
    // string in verifyLink, so two spellings would be two different documents.
    QCOMPARE(parseWith(header, "documentId", QJsonValue(QString(32, QLatin1Char('A')))),
             HeaderParse::Malformed);

    // A sealed key that is valid base64 but the wrong length, and one that is not
    // base64 at all.
    QCOMPARE(parseWithRecipient(header, "sealedKey", QJsonValue(QLatin1String("AAAA"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWithRecipient(header, "sealedKey", QJsonValue(QLatin1String("not base64!"))),
             HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizeNonce()
{
    const DocumentHeader header = sampleHeader();
    const QString shortNonce =
        QString::fromLatin1(QByteArray(DocumentCrypto::NonceSize - 1, '\x11').toHex());
    const QString longNonce =
        QString::fromLatin1(QByteArray(DocumentCrypto::NonceSize + 1, '\x11').toHex());

    QCOMPARE(parseWith(header, "nonce", QJsonValue(shortNonce)), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "nonce", QJsonValue(longNonce)), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizeSignature()
{
    const DocumentHeader header = sampleHeader();
    const QString shortSignature =
        QString::fromLatin1(QByteArray(DocumentCrypto::SignatureSize - 1, '\x33').toHex());

    QCOMPARE(parseWith(header, "writerSignature", QJsonValue(shortSignature)),
             HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizePayloadHash()
{
    const DocumentHeader header = sampleHeader();
    const QString shortHash =
        QString::fromLatin1(QByteArray(DocumentCrypto::PayloadHashSize - 1, '\x22').toHex());

    QCOMPARE(parseWith(header, "payloadHash", QJsonValue(shortHash)), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizeRecipientVerifyKey()
{
    const DocumentHeader header = sampleHeader();
    const QString shortKey =
        QString::fromLatin1(QByteArray(VerifyKey::Size - 1, '\x44').toHex());

    QCOMPARE(parseWithRecipient(header, "verifyKey", QJsonValue(shortKey)),
             HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAMalformedFingerprint()
{
    const DocumentHeader header = sampleHeader();

    QCOMPARE(parseWith(header, "lastWriter", QJsonValue(QLatin1String("0011"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWithRecipient(header,
                                "fingerprint",
                                QJsonValue(QString(Fingerprint::Size * 2, QLatin1Char('q')))),
             HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesASaveCounterAboveTheCap()
{
    DocumentHeader header = sampleHeader();

    header.saveCounter = DocumentHeader::MaxSaveCounter;
    QCOMPARE(reparsed(header).saveCounter, DocumentHeader::MaxSaveCounter);

    // 2^53 is exact as a double, so this is the value above the cap and not a
    // rounding of it.
    QCOMPARE(parseWith(header, "saveCounter", QJsonValue(9007199254740992.0)), HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "saveCounter", QJsonValue(9.0e18)), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAnUnknownKey()
{
    const DocumentHeader header = sampleHeader();

    // A field a newer writer added without a version bump would otherwise be
    // read past, and outside the associated data nothing authenticates it.
    QCOMPARE(parseWith(header, "comment", QJsonValue(QLatin1String("hello"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWithRecipient(header, "comment", QJsonValue(QLatin1String("hello"))),
             HeaderParse::Malformed);

    // The same key in a later format is that format's business.
    QJsonObject later = header.toJson();
    later["formatVersion"] = DocumentHeader::CurrentFormatVersion + 1;
    later["comment"] = QLatin1String("hello");
    DocumentHeader out;
    QCOMPARE(DocumentHeader::parse(later, out), HeaderParse::NeedsNewerVersion);
}

void DocumentHeaderTest::parseAcceptsAHeaderListingNoOne()
{
    // The last person's leave lists no one, and must still be readable so their
    // other devices can walk the chain to it.
    DocumentHeader header = sampleHeader();
    header.recipients.clear();
    header.admins.clear();

    DocumentHeader out;
    QCOMPARE(DocumentHeader::parse(header.toJson(), out), HeaderParse::Ok);
    QVERIFY(out.recipients.isEmpty());
}

void DocumentHeaderTest::parseRefusesAnEarlierFormatVersion()
{
    const DocumentHeader header = sampleHeader();

    // There is no earlier format to read: anything below current is damage.
    QCOMPARE(parseWith(header,
                       "formatVersion",
                       QJsonValue(DocumentHeader::CurrentFormatVersion - 1)),
             HeaderParse::Malformed);
    QCOMPARE(parseWith(header, "formatVersion", QJsonValue(0)), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesANonObjectRecipient()
{
    const DocumentHeader header = sampleHeader();

    QJsonObject json = header.toJson();
    QJsonArray recipients = json.value(QLatin1String("recipients")).toArray();
    recipients.replace(0, QJsonValue(QLatin1String("a recipient")));
    json[QLatin1String("recipients")] = recipients;

    DocumentHeader out;
    QCOMPARE(DocumentHeader::parse(json, out), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesANonCanonicalTimestamp()
{
    const DocumentHeader header = sampleHeader();

    // Same instant, different spelling. Accepting it would mean a reader
    // computing associated data from a string the writer never wrote, and every
    // file would fail to authenticate.
    QCOMPARE(parseWith(header,
                       "lastWriteTime",
                       QJsonValue(QLatin1String("2026-09-20T12:00:00.000+00:00"))),
             HeaderParse::Malformed);
    QCOMPARE(parseWith(header,
                       "lastWriteTime",
                       QJsonValue(QLatin1String("2026-09-20T07:00:00.000-05:00"))),
             HeaderParse::Malformed);

    // Whatever spelling the writer does produce must be accepted, and must come
    // back as the same instant.
    const DocumentHeader out = reparsed(header);
    QCOMPARE(out.lastWriteTime.toUTC(), header.lastWriteTime.toUTC());
}

void DocumentHeaderTest::parseNeedsNewerVersionForALaterFormat()
{
    const DocumentHeader header = sampleHeader();

    QCOMPARE(parseWith(header,
                       "formatVersion",
                       QJsonValue(DocumentHeader::CurrentFormatVersion + 1)),
             HeaderParse::NeedsNewerVersion);
}

void DocumentHeaderTest::associatedDataDiffersWhenAnyCoveredFieldDiffers()
{
    const DocumentHeader header = sampleHeader();
    const QByteArray original = header.associatedData();

    QList<DocumentHeader> variants;

    DocumentHeader otherDocument = header;
    otherDocument.documentId = newDocumentId();
    variants.append(otherDocument);

    DocumentHeader withOrigin = header;
    withOrigin.originDocumentId = newDocumentId();
    variants.append(withOrigin);

    DocumentHeader otherGeneration = header;
    otherGeneration.keyGeneration += 1;
    variants.append(otherGeneration);

    DocumentHeader otherCounter = header;
    otherCounter.saveCounter += 1;
    variants.append(otherCounter);

    DocumentHeader otherWriter = header;
    otherWriter.lastWriter = header.recipients.last().fingerprint;
    variants.append(otherWriter);

    DocumentHeader otherTime = header;
    otherTime.lastWriteTime = header.lastWriteTime.addMSecs(1);
    variants.append(otherTime);

    DocumentHeader otherNonce = header;
    otherNonce.nonce = QByteArray(DocumentCrypto::NonceSize, '\x99');
    variants.append(otherNonce);

    DocumentHeader droppedRecipient = header;
    droppedRecipient.recipients.removeLast();
    variants.append(droppedRecipient);

    DocumentHeader otherWriterPublicKey = header;
    otherWriterPublicKey.writerPublicKey = header.recipients.at(1).publicKey;
    variants.append(otherWriterPublicKey);

    DocumentHeader otherWriterVerifyKey = header;
    otherWriterVerifyKey.writerVerifyKey = header.recipients.at(1).verifyKey;
    variants.append(otherWriterVerifyKey);

    // The admin set, which is what a removal is checked against.
    DocumentHeader droppedAdmin = header;
    droppedAdmin.admins.removeLast();
    variants.append(droppedAdmin);

    DocumentHeader noAdmins = header;
    noAdmins.admins.clear();
    variants.append(noAdmins);

    DocumentHeader otherAdmin = header;
    otherAdmin.admins[0] = newMember();
    variants.append(otherAdmin);

    DocumentHeader otherSealedKey = header;
    otherSealedKey.recipients[0].sealedKey =
        DocumentCrypto::sealDataKey(DocumentCrypto::generateDataKey(),
                                    header.recipients.at(0).publicKey);
    variants.append(otherSealedKey);

    DocumentHeader otherFormat = header;
    otherFormat.formatVersion += 1;
    variants.append(otherFormat);

    // Every field of a recipient entry, because a recipient is the whole of what
    // a header says about who may open the document.
    DocumentHeader otherRecipientMember = header;
    otherRecipientMember.recipients[0].member = newMember();
    variants.append(otherRecipientMember);

    DocumentHeader otherRecipientFingerprint = header;
    otherRecipientFingerprint.recipients[0].fingerprint =
        header.recipients.at(1).fingerprint;
    variants.append(otherRecipientFingerprint);

    DocumentHeader otherRecipientPublicKey = header;
    otherRecipientPublicKey.recipients[0].publicKey =
        DocumentCrypto::generateKeypair().publicKey;
    variants.append(otherRecipientPublicKey);

    DocumentHeader otherRecipientVerifyKey = header;
    otherRecipientVerifyKey.recipients[0].verifyKey =
        DocumentCrypto::generateSigningKeypair().verifyKey;
    variants.append(otherRecipientVerifyKey);

    for (const DocumentHeader& variant : variants)
    {
        QVERIFY(variant.associatedData() != original);
    }
}

void DocumentHeaderTest::associatedDataIgnoresPayloadHashAndSignature()
{
    const DocumentHeader header = sampleHeader();

    // Both are derived from the associated data, so covering them would be
    // circular.
    DocumentHeader otherHash = header;
    otherHash.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x77');
    QCOMPARE(otherHash.associatedData(), header.associatedData());

    DocumentHeader otherSignature = header;
    otherSignature.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\x77');
    QCOMPARE(otherSignature.associatedData(), header.associatedData());
}

void DocumentHeaderTest::associatedDataDiffersOnRecipientOrder()
{
    const DocumentHeader header = sampleHeader();

    DocumentHeader reordered = header;
    std::swap(reordered.recipients[0], reordered.recipients[1]);
    QVERIFY(reordered.associatedData() != header.associatedData());
}

void DocumentHeaderTest::associatedDataSurvivesAJsonRoundTrip()
{
    // The test that catches the timestamp trap. Two constructions of the same
    // in-memory header being byte-identical proves nothing; what matters is that
    // a reader computes what the writer computed, and the reader's header has
    // been through QDateTime::fromString.
    const DocumentHeader header = sampleHeader();
    const DocumentHeader out = reparsed(header);

    QCOMPARE(out.associatedData(), header.associatedData());
    QCOMPARE(out.signedBytes(), header.signedBytes());
}

void DocumentHeaderTest::signedBytesDiffersWhenPayloadHashDiffers()
{
    const DocumentHeader header = sampleHeader();

    DocumentHeader otherHash = header;
    otherHash.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x77');

    QVERIFY(otherHash.signedBytes() != header.signedBytes());
    QVERIFY(header.signedBytes() != header.associatedData());
}

void DocumentHeaderTest::entryForFindsOnlyItsOwnFingerprint()
{
    const DocumentHeader header = sampleHeader();

    const std::optional<RecipientEntry> found =
        header.entryFor(header.recipients.at(1).fingerprint);
    QVERIFY(found.has_value());
    QVERIFY(found.value() == header.recipients.at(1));

    const Keypair stranger = DocumentCrypto::generateKeypair();
    const SigningKeypair strangerSigning = DocumentCrypto::generateSigningKeypair();
    QVERIFY(!header
                 .entryFor(DocumentCrypto::fingerprintOf(stranger.publicKey,
                                                         strangerSigning.verifyKey))
                 .has_value());

    QVERIFY(!header.entryFor(Fingerprint()).has_value());
}
