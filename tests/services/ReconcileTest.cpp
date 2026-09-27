#include "ReconcileTest.h"

#include "ChainTestSupport.h"
#include "Reconcile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace ChainTestSupport;
using Reconcile::DownloadCheck;
using Reconcile::Verdict;

namespace
{

/// One version as the server holds it: the header, the ciphertext it vouches for,
/// and the document that was encrypted.
struct Version
{
    DocumentHeader header;
    QByteArray ciphertext;
    QJsonObject payload;
};

/// A payload whose roster holds these devices and these admins.
QJsonObject payloadFor(const QList<TestDevice>& devices, const QList<MemberId>& admins)
{
    Document document = Document::empty();
    document.setAccessRoster(rosterOf(devices, admins));
    return document.toJson();
}

/// Encrypts the payload under the header, then fills in the real payload hash and
/// signs as the writer - the order a save path follows.
Version seal(DocumentHeader header,
             const TestDevice& writer,
             const DataKey& dataKey,
             const QJsonObject& payload)
{
    const QByteArray plaintext = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QByteArray ciphertext = DocumentCrypto::encryptPayload(plaintext, dataKey, header);
    header.payloadHash = DocumentCrypto::payloadHash(ciphertext);
    resign(header, writer);
    return Version{header, ciphertext, payload};
}

/// A version listing these devices with a roster to match. The recipients stand in
/// for the roster's people; the admins default to the first device's person.
Version version(const QString& documentId,
                int keyGeneration,
                qint64 saveCounter,
                const QList<TestDevice>& devices,
                const TestDevice& writer,
                const DataKey& dataKey,
                const QList<MemberId>& admins)
{
    const DocumentHeader header =
        buildHeader(documentId, keyGeneration, saveCounter, devices, writer, dataKey, admins);
    return seal(header, writer, dataKey, payloadFor(devices, admins));
}

Reconcile::Self selfOf(const TestDevice& device)
{
    return Reconcile::Self{device.sealing, device.signing.verifyKey};
}

/// Alice, Bob, and Carol at generation 1. Alice is the only admin. The base is the
/// version this machine and the server last agreed on.
struct World
{
    QString documentId;
    DataKey dataKey;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    Version base;

    QList<TestDevice> everyone() const { return {alice, bob, carol}; }
    QList<MemberId> admins() const { return {alice.member}; }
};

World world()
{
    World w;
    w.documentId = newDocumentId();
    w.dataKey = DocumentCrypto::generateDataKey();
    w.alice = newPerson();
    w.bob = newPerson();
    w.carol = newPerson();
    w.base = version(w.documentId, 1, 1, w.everyone(), w.alice, w.dataKey, w.admins());
    return w;
}

/// Bob's next ordinary version on top of the base.
Version bobsEdit(const World& w)
{
    return version(w.documentId, 1, 2, w.everyone(), w.bob, w.dataKey, w.admins());
}

/// A fault with nothing else set, so no caller can act on half a result.
bool isBareFault(const DownloadCheck& check)
{
    return check.verdict == Verdict::ServerFault && !check.reason.isEmpty()
        && !check.header.has_value() && !check.dataKey.has_value() && !check.document.has_value();
}

} // namespace

void ReconcileTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Accept ------------------------------------------------------------------

void ReconcileTest::aValidNewerVersionIsAccepted()
{
    const World w = world();
    const Version next = bobsEdit(w);

    const DownloadCheck check =
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.carol), 1);

    QCOMPARE(check.verdict, Verdict::Accept);
    QVERIFY(check.reason.isEmpty());
    QVERIFY(check.header.value() == next.header);
    QVERIFY(check.dataKey.value() == w.dataKey);

    // The document handed back is the one that was written, whole.
    Document written;
    QCOMPARE(Document::parse(next.payload, written), DocumentParse::Ok);
    QVERIFY(check.document.value() == written);
    QVERIFY(written.accessRoster() == rosterOf(w.everyone(), w.admins()));
}

void ReconcileTest::aValidNewerVersionIsAcceptedWithNoBaseHeader()
{
    const World w = world();
    const Version next = bobsEdit(w);

    const DownloadCheck check =
        Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 1);

    QCOMPARE(check.verdict, Verdict::Accept);
    QVERIFY(check.document.has_value());
}

void ReconcileTest::offlineLocalSavesDoNotDisturbTheAnchor()
{
    const World w = world();

    // Carol saved offline many times; her local header ran far ahead and was never
    // uploaded. The anchor is the base, so Bob's version is judged against that.
    const DocumentHeader localHeader =
        buildHeader(w.documentId, 1, 50, w.everyone(), w.carol, w.dataKey, w.admins());
    QVERIFY(localHeader.saveCounter > bobsEdit(w).header.saveCounter);

    const Version next = bobsEdit(w);
    const DownloadCheck check =
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.carol), 1);
    QCOMPARE(check.verdict, Verdict::Accept);
}

void ReconcileTest::aVersionWrittenBySomeoneWhoJustLeftIsAccepted()
{
    const World w = world();

    // Bob writes the version that drops him. He is on neither its recipients nor
    // its roster.
    const Version leave = version(w.documentId, 1, 2, {w.alice, w.carol}, w.bob, w.dataKey, w.admins());

    const DownloadCheck withBase =
        Reconcile::validateDownload(w.base.header, {leave.header}, leave.ciphertext, selfOf(w.carol), 1);
    QCOMPARE(withBase.verdict, Verdict::Accept);
    QVERIFY(!withBase.document.value().accessRoster().contains(w.bob.member));

    const DownloadCheck withoutBase =
        Reconcile::validateDownload(std::nullopt, {leave.header}, leave.ciphertext, selfOf(w.carol), 1);
    QCOMPARE(withoutBase.verdict, Verdict::Accept);
}

// --- Failures ----------------------------------------------------------------

void ReconcileTest::anUnsealableEntryIsAServerFault()
{
    const World w = world();
    Version next = bobsEdit(w);

    // Carol's entry keeps her keys but seals the data key to someone else.
    for (RecipientEntry& entry : next.header.recipients)
    {
        if (entry.fingerprint == w.carol.fingerprint())
        {
            entry.sealedKey =
                DocumentCrypto::sealDataKey(w.dataKey, DocumentCrypto::generateKeypair().publicKey);
        }
    }
    next = seal(next.header, w.bob, w.dataKey, payloadFor(w.everyone(), w.admins()));

    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.carol), 1)));

    // The control: the same version is readable by Alice, whose entry is sound.
    QCOMPARE(Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.alice), 1)
                 .verdict,
             Verdict::Accept);
}

void ReconcileTest::aPayloadHashMismatchIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);

    // Another version's ciphertext, genuine in itself, under this header.
    const Version other = version(w.documentId, 1, 2, w.everyone(), w.bob, w.dataKey, w.admins());
    QVERIFY(other.ciphertext != next.ciphertext);

    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {next.header}, other.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aBadSignatureIsAServerFault()
{
    const World w = world();
    Version next = bobsEdit(w);
    next.header.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\0');

    // With no base header the chain is not walked, so the signature check stands
    // alone.
    QVERIFY(isBareFault(
        Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 1)));
    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aBrokenChainIsAServerFault()
{
    const World w = world();

    // Carol, a member, adds Dave. The version is otherwise sound and Alice can
    // read it, but the chain refuses the grant.
    const TestDevice dave = newPerson();
    const QList<TestDevice> withDave = {w.alice, w.bob, w.carol, dave};
    const Version next = version(w.documentId, 1, 2, withDave, w.carol, w.dataKey, w.admins());

    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.alice), 1)));
}

void ReconcileTest::aWithheldHeaderIsAServerFault()
{
    const World w = world();

    // Alice grants Dave and makes him an admin; Dave then writes. Without the
    // grant header, Dave writing is a stranger writing.
    const TestDevice dave = newPerson();
    const QList<TestDevice> withDave = {w.alice, w.bob, w.carol, dave};
    const QList<MemberId> admins = {w.alice.member, dave.member};
    const Version granted = version(w.documentId, 1, 2, withDave, w.alice, w.dataKey, admins);
    const Version davesEdit = version(w.documentId, 1, 3, withDave, dave, w.dataKey, admins);

    QCOMPARE(Reconcile::validateDownload(w.base.header,
                                         {granted.header, davesEdit.header},
                                         davesEdit.ciphertext,
                                         selfOf(w.carol),
                                         1)
                 .verdict,
             Verdict::Accept);

    QVERIFY(isBareFault(Reconcile::validateDownload(
        w.base.header, {davesEdit.header}, davesEdit.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aSubstitutedHeaderIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);

    // A header from another ward, signed by one of its own members, served in
    // place of this ward's.
    const World elsewhere = world();
    const Version foreign = bobsEdit(elsewhere);

    QVERIFY(isBareFault(Reconcile::validateDownload(
        w.base.header, {foreign.header}, next.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aGenerationBelowTheHighWaterMarkIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);

    QCOMPARE(Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 1)
                 .verdict,
             Verdict::Accept);
    QVERIFY(isBareFault(
        Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 2)));
}

void ReconcileTest::anEntryWithAnotherVerifyKeyIsAServerFault()
{
    const World w = world();
    Version next = bobsEdit(w);

    // Carol's fingerprint, with another signing key behind it: the key would still
    // unseal, but the entry is not Carol's device.
    for (RecipientEntry& entry : next.header.recipients)
    {
        if (entry.fingerprint == w.carol.fingerprint())
        {
            entry.verifyKey = DocumentCrypto::generateSigningKeypair().verifyKey;
        }
    }
    next = seal(next.header, w.bob, w.dataKey, payloadFor(w.everyone(), w.admins()));

    QVERIFY(isBareFault(
        Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aWriterListedButMissingFromTheRosterIsAServerFault()
{
    const World w = world();

    // Bob writes a version that still lists him, so it is not a leave, but its
    // roster leaves him out: an entry on the access list nobody can attribute.
    const DocumentHeader header =
        buildHeader(w.documentId, 1, 2, w.everyone(), w.bob, w.dataKey, w.admins());
    const Version next = seal(header, w.bob, w.dataKey, payloadFor({w.alice, w.carol}, w.admins()));

    QVERIFY(isBareFault(
        Reconcile::validateDownload(std::nullopt, {next.header}, next.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::aHeaderDisagreeingWithItsRosterIsAServerFault()
{
    const World w = world();

    // The roster makes Carol an admin; the header does not.
    const DocumentHeader header =
        buildHeader(w.documentId, 1, 2, w.everyone(), w.alice, w.dataKey, w.admins());
    const Version next =
        seal(header, w.alice, w.dataKey, payloadFor(w.everyone(), {w.alice.member, w.carol.member}));

    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.bob), 1)));
}

void ReconcileTest::anEmptyHeaderListIsAServerFault()
{
    const World w = world();

    QVERIFY(isBareFault(
        Reconcile::validateDownload(w.base.header, {}, w.base.ciphertext, selfOf(w.carol), 1)));
}

// --- Removal -----------------------------------------------------------------

void ReconcileTest::aChainProvenRemovalIsRemoved()
{
    const World w = world();
    const Version removal = version(w.documentId,
                                    2,
                                    2,
                                    {w.alice, w.bob},
                                    w.alice,
                                    DocumentCrypto::generateDataKey(),
                                    w.admins());

    const DownloadCheck check = Reconcile::validateDownload(
        w.base.header, {removal.header}, removal.ciphertext, selfOf(w.carol), 1);

    QCOMPARE(check.verdict, Verdict::Removed);
    QVERIFY(!check.header.has_value() && !check.dataKey.has_value() && !check.document.has_value());
}

void ReconcileTest::aRemovalByAnAdminPromotedAfterTheBaseIsRemoved()
{
    const World w = world();

    // Alice grants Dave and makes him an admin; Dave removes Carol. Carol never saw
    // Dave arrive.
    const TestDevice dave = newPerson();
    const QList<MemberId> admins = {w.alice.member, dave.member};
    const Version granted =
        version(w.documentId, 1, 2, {w.alice, w.bob, w.carol, dave}, w.alice, w.dataKey, admins);
    const Version removal = version(w.documentId,
                                    2,
                                    3,
                                    {w.alice, w.bob, dave},
                                    dave,
                                    DocumentCrypto::generateDataKey(),
                                    admins);

    QCOMPARE(Reconcile::validateDownload(w.base.header,
                                         {granted.header, removal.header},
                                         removal.ciphertext,
                                         selfOf(w.carol),
                                         1)
                 .verdict,
             Verdict::Removed);
}

void ReconcileTest::theLeaversOtherDeviceIsRemovedByTheLeave()
{
    World w = world();
    const TestDevice bobPhone = newDeviceFor(w.bob.member);
    const QList<TestDevice> withPhone = {w.alice, w.bob, w.carol, bobPhone};
    w.base = version(w.documentId, 1, 1, withPhone, w.alice, w.dataKey, w.admins());

    // Bob leaves from his laptop. His phone, offline at the time, learns it is out.
    const Version leave = version(w.documentId, 1, 2, {w.alice, w.carol}, w.bob, w.dataKey, w.admins());

    QCOMPARE(Reconcile::validateDownload(w.base.header, {leave.header}, leave.ciphertext, selfOf(bobPhone), 1)
                 .verdict,
             Verdict::Removed);
    QCOMPARE(Reconcile::checkRemoval(w.base.header, {leave.header}, bobPhone.fingerprint()),
             Verdict::Removed);
}

void ReconcileTest::withNoBaseHeaderAMissingEntryIsNeverRemoved()
{
    const World w = world();
    const Version removal = version(w.documentId,
                                    2,
                                    2,
                                    {w.alice, w.bob},
                                    w.alice,
                                    DocumentCrypto::generateDataKey(),
                                    w.admins());

    // The same removal that is Removed from the base proves nothing without it.
    QVERIFY(isBareFault(Reconcile::validateDownload(
        std::nullopt, {removal.header}, removal.ciphertext, selfOf(w.carol), 1)));
}

void ReconcileTest::checkRemovalReachesTheSameVerdictsFromHeadersAlone()
{
    const World w = world();
    const Version removal = version(w.documentId,
                                    2,
                                    2,
                                    {w.alice, w.bob},
                                    w.alice,
                                    DocumentCrypto::generateDataKey(),
                                    w.admins());
    const Fingerprint carol = w.carol.fingerprint();

    QCOMPARE(Reconcile::checkRemoval(w.base.header, {removal.header}, carol), Verdict::Removed);
    QCOMPARE(Reconcile::checkRemoval(std::nullopt, {removal.header}, carol), Verdict::ServerFault);
    QCOMPARE(Reconcile::checkRemoval(w.base.header, {}, carol), Verdict::ServerFault);

    // Still listed is not removed.
    QCOMPARE(Reconcile::checkRemoval(w.base.header, {removal.header}, w.bob.fingerprint()),
             Verdict::ServerFault);

    // A removal written by a member does not hold, so it proves nothing.
    const Version byMember = version(w.documentId,
                                     2,
                                     2,
                                     {w.alice, w.bob},
                                     w.bob,
                                     DocumentCrypto::generateDataKey(),
                                     w.admins());
    QCOMPARE(Reconcile::checkRemoval(w.base.header, {byMember.header}, carol), Verdict::ServerFault);
}

// --- Newer versions ----------------------------------------------------------

void ReconcileTest::aNewerHeaderFormatAnywhereIsNeedsNewerVersion()
{
    const World w = world();
    const Version next = bobsEdit(w);

    QList<DocumentHeader> headers;
    QCOMPARE(Reconcile::readHeaders({w.base.header.toJson(), next.header.toJson()}, headers),
             Verdict::Accept);
    QCOMPARE(headers.size(), qsizetype(2));
    QVERIFY(headers.last() == next.header);

    // A later format first in the list, and alongside a malformed one: a newer
    // header is never read as a reason to delete anything.
    QJsonObject newer = w.base.header.toJson();
    newer["formatVersion"] = DocumentHeader::CurrentFormatVersion + 1;
    QJsonObject malformed = next.header.toJson();
    malformed.remove("nonce");

    QList<DocumentHeader> untouched;
    QCOMPARE(Reconcile::readHeaders({newer, next.header.toJson()}, untouched), Verdict::NeedsNewerVersion);
    QCOMPARE(Reconcile::readHeaders({malformed, newer}, untouched), Verdict::NeedsNewerVersion);
    QVERIFY(untouched.isEmpty());
}

void ReconcileTest::anUnknownRecipientKindAnywhereIsNeedsNewerVersion()
{
    const World w = world();
    const Version next = bobsEdit(w);

    QJsonObject json = next.header.toJson();
    QJsonArray recipients = json["recipients"].toArray();
    QJsonObject entry = recipients.at(1).toObject();
    entry["kind"] = 2;
    recipients.replace(1, entry);
    json["recipients"] = recipients;

    QList<DocumentHeader> headers;
    QCOMPARE(Reconcile::readHeaders({w.base.header.toJson(), json}, headers), Verdict::NeedsNewerVersion);
}

void ReconcileTest::aMalformedHeaderIsAServerFault()
{
    const World w = world();

    QJsonObject malformed = bobsEdit(w).header.toJson();
    malformed.remove("nonce");

    QList<DocumentHeader> headers;
    QCOMPARE(Reconcile::readHeaders({w.base.header.toJson(), malformed}, headers), Verdict::ServerFault);
    QVERIFY(headers.isEmpty());
}

void ReconcileTest::aNewerPayloadVersionIsNeedsNewerVersion()
{
    const World w = world();

    QJsonObject payload = payloadFor(w.everyone(), w.admins());
    payload["payloadVersion"] = Document::CurrentPayloadVersion + 1;
    const DocumentHeader header =
        buildHeader(w.documentId, 1, 2, w.everyone(), w.bob, w.dataKey, w.admins());
    const Version next = seal(header, w.bob, w.dataKey, payload);

    const DownloadCheck check =
        Reconcile::validateDownload(w.base.header, {next.header}, next.ciphertext, selfOf(w.carol), 1);
    QCOMPARE(check.verdict, Verdict::NeedsNewerVersion);
    QVERIFY(!check.document.has_value());
}
