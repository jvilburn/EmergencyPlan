#include "DocumentFileTest.h"

#include "ChainTestSupport.h"
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

/// A ward of Alice, its admin, and Bob, a member, written once by Alice.
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
    w.documentId = ChainTestSupport::newDocumentId();
    w.alice = newIdentity("Alice", "Laptop");
    w.bob = newIdentity("Bob", "Phone");
    w.dataKey = DocumentCrypto::generateDataKey();

    AccessRoster roster;
    roster.addPerson(w.alice.asNewPerson());
    RosterPerson bob = w.bob.asNewPerson();
    bob.setRole(RosterPerson::Role::Member);
    roster.addPerson(bob);

    w.document = Document::empty();
    w.document.setAccessRoster(roster);

    w.first.documentId = w.documentId;
    w.first.saveCounter = 0;

    w.sealed = DocumentFile::seal(w.document, w.first, w.dataKey, w.alice).value();
    return w;
}

Reconcile::DownloadCheck openAs(const Written& w, const DocumentFile::Sealed& sealed, const DeviceIdentity& reader)
{
    return Reconcile::validateDownload(w.documentId,
                                       std::nullopt,
                                       {sealed.header},
                                       sealed.ciphertext,
                                       reader.self(),
                                       Reconcile::HighWaterMark());
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

    // Another id, re-signed by the writer so the signature is not what fails: the
    // id is in the associated data, so the payload no longer opens.
    DocumentFile::Sealed renamed = read;
    renamed.header.documentId = ChainTestSupport::newDocumentId();
    renamed.header.writerSignature = DocumentCrypto::signHeader(renamed.header, w.alice.signing.signingKey);
    QVERIFY(!DocumentCrypto::decryptPayload(renamed.ciphertext, w.dataKey, renamed.header).has_value());
}

void DocumentFileTest::aSignatureReplacedByAnotherMembersIsRefused()
{
    const Written w = written();

    DocumentFile::Sealed forged = w.sealed;
    forged.header.writerSignature = DocumentCrypto::signHeader(forged.header, w.bob.signing.signingKey);

    QCOMPARE(openAs(w, w.sealed, w.bob).verdict, Reconcile::Verdict::Accept);
    QCOMPARE(openAs(w, forged, w.bob).verdict, Reconcile::Verdict::ServerFault);
}

void DocumentFileTest::sealingAdvancesTheSaveCounterByOne()
{
    const Written w = written();
    QCOMPARE(w.sealed.header.saveCounter, qint64(1));

    const DocumentFile::Sealed second = DocumentFile::seal(w.document, w.sealed.header, w.dataKey, w.alice).value();
    QCOMPARE(second.header.saveCounter, qint64(2));
    QCOMPARE(second.header.keyGeneration, w.sealed.header.keyGeneration);
    QVERIFY(second.header.lastWriter == w.alice.fingerprint());
    QVERIFY(DocumentCrypto::verifyLink(w.sealed.header, second.header));
}

void DocumentFileTest::twoConsecutiveSealsUseDifferentNonces()
{
    const Written w = written();
    const DocumentFile::Sealed second = DocumentFile::seal(w.document, w.sealed.header, w.dataKey, w.alice).value();

    QCOMPARE(second.header.nonce.size(), qsizetype(DocumentCrypto::NonceSize));
    QVERIFY(second.header.nonce != w.sealed.header.nonce);
}

void DocumentFileTest::twoSealsFromTheSamePreviousHeaderUseDifferentNonces()
{
    // Catches a nonce derived from the counter, which the consecutive test passes.
    const Written w = written();
    const DocumentFile::Sealed one = DocumentFile::seal(w.document, w.sealed.header, w.dataKey, w.alice).value();
    const DocumentFile::Sealed other = DocumentFile::seal(w.document, w.sealed.header, w.dataKey, w.alice).value();

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
