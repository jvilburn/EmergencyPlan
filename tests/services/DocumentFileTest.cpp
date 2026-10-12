#include "DocumentFileTest.h"

#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "Reconcile.h"
#include "SessionTestSupport.h"

#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace SessionTestSupport;

namespace
{

/// A ward sealed to Alice and Bob, written once.
struct Written
{
    QString documentId;
    DeviceIdentity alice;
    DeviceIdentity bob;
    DataKey dataKey;
    Document document;
    DocumentHeader first;
    DocumentFile::Sealed sealed;
};

Written written()
{
    Written w;
    w.documentId = ServerTestSupport::newDocumentId();
    w.alice = newIdentity("Alice", "Laptop");
    w.bob = newIdentity("Bob", "Phone");
    w.dataKey = DocumentCrypto::generateDataKey();

    SealingSet set = SealingSet::of(w.alice.keys());
    set.add(w.bob.keys());

    w.document = Document::empty();
    w.document.setSealingSet(set);

    w.first.documentId = w.documentId;
    w.first.saveCounter = 0;

    w.sealed = DocumentFile::seal(w.document, w.first, w.dataKey).value();
    return w;
}

Reconcile::DownloadCheck openAs(const Written& w, const DocumentFile::Sealed& sealed, const DeviceIdentity& reader)
{
    return Reconcile::validate(w.documentId, sealed.header, sealed.ciphertext, reader.self(), {});
}

} // namespace

void DocumentFileTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentFileTest::aRoundTripWithTwoRecipientsOpensForEither()
{
    const Written w = written();

    DocumentFile::Sealed read;
    QCOMPARE(DocumentFile::decode(DocumentFile::encode(w.sealed), read), DocumentFile::Read::Ok);
    QVERIFY(read.header == w.sealed.header);
    QCOMPARE(read.ciphertext, w.sealed.ciphertext);

    for (const DeviceIdentity& reader : {w.alice, w.bob})
    {
        const Reconcile::DownloadCheck check = openAs(w, read, reader);
        QCOMPARE(check.verdict, Reconcile::Verdict::Accept);
        QVERIFY(check.document.value() == w.document);
        QVERIFY(check.dataKey.value() == w.dataKey);
    }
}

void DocumentFileTest::aThirdKeypairIsRefused()
{
    const Written w = written();

    // Positive anchor: the members open it, so the refusal below is about Carol.
    QCOMPARE(openAs(w, w.sealed, w.alice).verdict, Reconcile::Verdict::Accept);
    QCOMPARE(openAs(w, w.sealed, newIdentity("Carol", "Tablet")).verdict, Reconcile::Verdict::ServerFault);
}

void DocumentFileTest::aFlippedPayloadByteIsRefused()
{
    const Written w = written();

    DocumentFile::Sealed flipped = w.sealed;
    flipped.ciphertext[flipped.ciphertext.size() / 2] = char(flipped.ciphertext.at(flipped.ciphertext.size() / 2) ^ 0x01);

    QCOMPARE(openAs(w, flipped, w.alice).verdict, Reconcile::Verdict::ServerFault);

    // The ciphertext itself no longer authenticates either.
    QVERIFY(!DocumentCrypto::decryptPayload(flipped.ciphertext, w.dataKey, flipped.header).has_value());
}

void DocumentFileTest::theDocumentIdSurvivesARoundTripAndIsAuthenticated()
{
    const Written w = written();

    DocumentFile::Sealed read;
    QCOMPARE(DocumentFile::decode(DocumentFile::encode(w.sealed), read), DocumentFile::Read::Ok);
    QCOMPARE(read.header.documentId, w.documentId);
    QVERIFY(DocumentCrypto::decryptPayload(read.ciphertext, w.dataKey, read.header).has_value());

    // Another id: it is in the associated data, so the payload no longer opens.
    DocumentFile::Sealed renamed = read;
    renamed.header.documentId = ServerTestSupport::newDocumentId();
    QVERIFY(!DocumentCrypto::decryptPayload(renamed.ciphertext, w.dataKey, renamed.header).has_value());
}

void DocumentFileTest::aSealedKeyMovedToAnotherEntryIsRefused()
{
    const Written w = written();

    // Bob's sealed key copied over Alice's: her entry no longer opens, and the header
    // no longer authenticates the payload for either.
    DocumentFile::Sealed swapped = w.sealed;
    swapped.header.recipients[0].sealedKey = swapped.header.recipients[1].sealedKey;

    QCOMPARE(openAs(w, w.sealed, w.bob).verdict, Reconcile::Verdict::Accept);
    QCOMPARE(openAs(w, swapped, w.alice).verdict, Reconcile::Verdict::ServerFault);
    QCOMPARE(openAs(w, swapped, w.bob).verdict, Reconcile::Verdict::ServerFault);
}

void DocumentFileTest::sealingAdvancesTheSaveCounterByOne()
{
    const Written w = written();
    QCOMPARE(w.sealed.header.saveCounter, qint64(1));

    const DocumentFile::Sealed second = DocumentFile::seal(w.document, w.sealed.header, w.dataKey).value();
    QCOMPARE(second.header.saveCounter, qint64(2));
    QCOMPARE(second.header.documentId, w.documentId);
    QCOMPARE(second.header.recipients.size(), qsizetype(2));

    // The cap is a refusal, not a wrap.
    DocumentHeader atCap = w.sealed.header;
    atCap.saveCounter = DocumentHeader::MaxSaveCounter;
    QVERIFY(!DocumentFile::seal(w.document, atCap, w.dataKey).has_value());
}

void DocumentFileTest::twoConsecutiveSealsUseDifferentNonces()
{
    const Written w = written();
    const DocumentFile::Sealed second = DocumentFile::seal(w.document, w.sealed.header, w.dataKey).value();

    QCOMPARE(second.header.nonce.size(), qsizetype(DocumentCrypto::NonceSize));
    QVERIFY(second.header.nonce != w.sealed.header.nonce);
}

void DocumentFileTest::twoSealsFromTheSamePreviousHeaderUseDifferentNonces()
{
    // Catches a nonce derived from the counter, which the consecutive test passes.
    const Written w = written();
    const DocumentFile::Sealed one = DocumentFile::seal(w.document, w.sealed.header, w.dataKey).value();
    const DocumentFile::Sealed other = DocumentFile::seal(w.document, w.sealed.header, w.dataKey).value();

    QCOMPARE(one.header.saveCounter, other.header.saveCounter);
    QVERIFY(one.header.nonce != other.header.nonce);
}

void DocumentFileTest::newlinesAndCarriageReturnsSurviveWriteAndRead()
{
    QTemporaryDir folder;
    const QString path = folder.filePath("ward.wardplanning");

    QByteArray bytes("{}\n");
    bytes.append("\x0a\x0d\x0d\x0a\x00\x0a\x1a", 7);
    QVERIFY(bytes.contains('\r') && bytes.count('\n') > 1);

    QVERIFY(DocumentFile::write(path, bytes, nullptr));
    QCOMPARE(DocumentFile::readAll(path).value(), bytes);
}

void DocumentFileTest::encodeWritesExactlyOneNewlineBeforeTheCiphertext()
{
    const Written w = written();
    const QByteArray bytes = DocumentFile::encode(w.sealed);
    const QByteArray headerLine = QJsonDocument(w.sealed.header.toJson()).toJson(QJsonDocument::Compact);

    QVERIFY(!headerLine.contains('\n'));
    QCOMPARE(bytes.indexOf('\n'), headerLine.size());
    QCOMPARE(bytes.left(headerLine.size()), headerLine);
    QCOMPARE(bytes.mid(headerLine.size() + 1), w.sealed.ciphertext);
}

void DocumentFileTest::decodeReportsALaterFormatAndMalformedBytes()
{
    const Written w = written();

    DocumentFile::Sealed later = w.sealed;
    later.header.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    DocumentFile::Sealed read;
    QCOMPARE(DocumentFile::decode(DocumentFile::encode(later), read), DocumentFile::Read::NeedsNewerVersion);

    const QByteArray headerLine = QJsonDocument(w.sealed.header.toJson()).toJson(QJsonDocument::Compact);
    QCOMPARE(DocumentFile::decode(headerLine, read), DocumentFile::Read::Malformed);
    QCOMPARE(DocumentFile::decode(QByteArray("not json\n") + w.sealed.ciphertext, read), DocumentFile::Read::Malformed);
    QCOMPARE(DocumentFile::decode(DocumentFile::encode(w.sealed), read), DocumentFile::Read::Ok);
}
