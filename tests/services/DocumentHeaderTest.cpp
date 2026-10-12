#include "DocumentHeaderTest.h"

#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QJsonArray>
#include <QtTest>

namespace
{

QString newDocumentId()
{
    return QString::fromLatin1(DocumentCrypto::randomBytes(DocumentHeader::DocumentIdLength / 2).toHex());
}

RecipientEntry newRecipient(const DataKey& dataKey)
{
    const Keypair sealing = DocumentCrypto::generateKeypair();
    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();
    return RecipientEntry{DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey),
                          DocumentCrypto::sealDataKey(dataKey, sealing.publicKey)};
}

/// Two recipients, so order and counts are exercised everywhere.
DocumentHeader sampleHeader()
{
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    DocumentHeader header;
    header.documentId = newDocumentId();
    header.saveCounter = 17;
    header.nonce = QByteArray(DocumentCrypto::NonceSize, '\x11');
    header.recipients = {newRecipient(dataKey), newRecipient(dataKey)};
    return header;
}

HeaderParse parse(const QJsonObject& json)
{
    DocumentHeader out;
    return DocumentHeader::parse(json, out);
}

QJsonObject withRecipient(const QJsonObject& json, int index, const QString& key, const QJsonValue& value)
{
    QJsonObject changed = json;
    QJsonArray recipients = json["recipients"].toArray();
    QJsonObject entry = recipients.at(index).toObject();
    if (value.isUndefined())
    {
        entry.remove(key);
    }
    else
    {
        entry[key] = value;
    }
    recipients[index] = entry;
    changed["recipients"] = recipients;
    return changed;
}

} // namespace

void DocumentHeaderTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentHeaderTest::roundTripsThroughJson()
{
    DocumentHeader header = sampleHeader();
    header.originDocumentId = newDocumentId();

    DocumentHeader read;
    QCOMPARE(DocumentHeader::parse(header.toJson(), read), HeaderParse::Ok);
    QVERIFY(read == header);
    QCOMPARE(read.formatVersion, 3);
}

void DocumentHeaderTest::roundTripKeepsRecipientOrder()
{
    const DocumentHeader header = sampleHeader();
    DocumentHeader read;
    QCOMPARE(DocumentHeader::parse(header.toJson(), read), HeaderParse::Ok);
    QCOMPARE(read.recipients.first().fingerprint, header.recipients.first().fingerprint);
    QCOMPARE(read.recipients.last().fingerprint, header.recipients.last().fingerprint);
}

void DocumentHeaderTest::omitsOriginDocumentIdWhenAbsent()
{
    const QJsonObject json = sampleHeader().toJson();
    QVERIFY(!json.contains("originDocumentId"));
    QCOMPARE(json.keys(), (QStringList{"documentId", "formatVersion", "nonce", "recipients", "saveCounter"}));
}

void DocumentHeaderTest::parseRefusesMissingOrIllFormedFields()
{
    const QJsonObject good = sampleHeader().toJson();
    QCOMPARE(parse(good), HeaderParse::Ok);

    for (const QString& key : {"formatVersion", "documentId", "saveCounter", "nonce", "recipients"})
    {
        QJsonObject missing = good;
        missing.remove(key);
        QCOMPARE(parse(missing), HeaderParse::Malformed);
    }

    QJsonObject upperCaseId = good;
    upperCaseId["documentId"] = good["documentId"].toString().toUpper();
    QCOMPARE(parse(upperCaseId), HeaderParse::Malformed);

    QJsonObject badOrigin = good;
    badOrigin["originDocumentId"] = "not an id";
    QCOMPARE(parse(badOrigin), HeaderParse::Malformed);

    QJsonObject negative = good;
    negative["saveCounter"] = -1;
    QCOMPARE(parse(negative), HeaderParse::Malformed);

    QCOMPARE(parse(withRecipient(good, 0, "fingerprint", QJsonValue::Undefined)), HeaderParse::Malformed);
    QCOMPARE(parse(withRecipient(good, 0, "sealedKey", QJsonValue::Undefined)), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizeNonce()
{
    QJsonObject json = sampleHeader().toJson();
    json["nonce"] = QString(QByteArray(DocumentCrypto::NonceSize - 1, '\x11').toHex());
    QCOMPARE(parse(json), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAWrongSizeSealedKey()
{
    const QJsonObject json = sampleHeader().toJson();
    const QString shortKey = QString::fromLatin1(QByteArray(DocumentCrypto::SealedDataKeySize - 1, 'k').toBase64());
    QCOMPARE(parse(withRecipient(json, 1, "sealedKey", shortKey)), HeaderParse::Malformed);
    QCOMPARE(parse(withRecipient(json, 1, "sealedKey", "not*base64")), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAMalformedFingerprint()
{
    const QJsonObject json = sampleHeader().toJson();
    const QString fingerprint = json["recipients"].toArray().first().toObject()["fingerprint"].toString();
    QCOMPARE(parse(withRecipient(json, 0, "fingerprint", fingerprint.left(30))), HeaderParse::Malformed);
    QCOMPARE(parse(withRecipient(json, 0, "fingerprint", fingerprint.toUpper())), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesANonObjectRecipient()
{
    QJsonObject json = sampleHeader().toJson();
    QJsonArray recipients = json["recipients"].toArray();
    recipients.append(QString("a device"));
    json["recipients"] = recipients;
    QCOMPARE(parse(json), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesASaveCounterAboveTheCap()
{
    QJsonObject json = sampleHeader().toJson();
    json["saveCounter"] = double(DocumentHeader::MaxSaveCounter);
    QCOMPARE(parse(json), HeaderParse::Ok);
    json["saveCounter"] = double(DocumentHeader::MaxSaveCounter + 1);
    QCOMPARE(parse(json), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAnUnknownKey()
{
    QJsonObject json = sampleHeader().toJson();
    json["keyGeneration"] = 1;
    QCOMPARE(parse(json), HeaderParse::Malformed);

    QCOMPARE(parse(withRecipient(sampleHeader().toJson(), 0, "member", "00")), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAHeaderListingNoOne()
{
    QJsonObject json = sampleHeader().toJson();
    json["recipients"] = QJsonArray();
    QCOMPARE(parse(json), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesARepeatedFingerprint()
{
    DocumentHeader header = sampleHeader();
    header.recipients.append(header.recipients.first());
    QCOMPARE(parse(header.toJson()), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseRefusesAnEarlierFormatVersion()
{
    QJsonObject json = sampleHeader().toJson();
    json["formatVersion"] = DocumentHeader::CurrentFormatVersion - 1;
    QCOMPARE(parse(json), HeaderParse::Malformed);
}

void DocumentHeaderTest::parseNeedsNewerVersionForALaterFormat()
{
    // A later format is refused before anything else in it is read.
    QJsonObject json;
    json["formatVersion"] = DocumentHeader::CurrentFormatVersion + 1;
    json["anything"] = "at all";
    QCOMPARE(parse(json), HeaderParse::NeedsNewerVersion);
}

void DocumentHeaderTest::associatedDataDiffersWhenAnyFieldDiffers()
{
    const DocumentHeader header = sampleHeader();
    const QByteArray original = header.associatedData();

    QList<DocumentHeader> variants;
    DocumentHeader changed = header;
    changed.documentId = newDocumentId();
    variants.append(changed);
    changed = header;
    changed.originDocumentId = newDocumentId();
    variants.append(changed);
    changed = header;
    changed.saveCounter += 1;
    variants.append(changed);
    changed = header;
    changed.nonce[0] = char(changed.nonce.at(0) ^ 0x01);
    variants.append(changed);
    changed = header;
    changed.recipients.first().sealedKey[0] = char(changed.recipients.first().sealedKey.at(0) ^ 0x01);
    variants.append(changed);
    changed = header;
    changed.recipients.first().fingerprint.bytes[0] = char(changed.recipients.first().fingerprint.bytes.at(0) ^ 0x01);
    variants.append(changed);
    changed = header;
    changed.recipients.removeLast();
    variants.append(changed);

    for (const DocumentHeader& variant : variants)
    {
        QVERIFY(variant.associatedData() != original);
    }
    QCOMPARE(header.associatedData(), original);
}

void DocumentHeaderTest::associatedDataDiffersOnRecipientOrder()
{
    DocumentHeader header = sampleHeader();
    const QByteArray original = header.associatedData();
    std::swap(header.recipients[0], header.recipients[1]);
    QVERIFY(header.associatedData() != original);
}

void DocumentHeaderTest::associatedDataSurvivesAJsonRoundTrip()
{
    DocumentHeader header = sampleHeader();
    header.originDocumentId = newDocumentId();
    DocumentHeader read;
    QCOMPARE(DocumentHeader::parse(header.toJson(), read), HeaderParse::Ok);
    QCOMPARE(read.associatedData(), header.associatedData());
}

void DocumentHeaderTest::entryForFindsOnlyItsOwnFingerprint()
{
    const DocumentHeader header = sampleHeader();
    QCOMPARE(header.entryFor(header.recipients.last().fingerprint).value(), header.recipients.last());
    QVERIFY(!header.entryFor(newRecipient(DocumentCrypto::generateDataKey()).fingerprint).has_value());
    QVERIFY(!header.entryFor(Fingerprint()).has_value());
}
