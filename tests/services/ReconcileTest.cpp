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

/// The mark of a device that has seen generation 1 and nothing of its counter,
/// which every version in these tests is at or above.
const Reconcile::HighWaterMark FirstGeneration{1, 0};

/// Validates a download of the world's document as this device would.
DownloadCheck downloadAs(const World& w,
                         const std::optional<DocumentHeader>& baseHeader,
                         const QList<DocumentHeader>& laterHeaders,
                         const QByteArray& ciphertext,
                         const TestDevice& self)
{
    return Reconcile::validateDownload(
        w.documentId, baseHeader, laterHeaders, ciphertext, selfOf(self), FirstGeneration);
}

/// The verdict on a version, with no base header, against this high-water mark.
Verdict verdictAgainst(const World& w,
                       const Version& version,
                       const TestDevice& self,
                       const Reconcile::HighWaterMark& mark)
{
    return Reconcile::validateDownload(
               w.documentId, std::nullopt, {version.header}, version.ciphertext, selfOf(self), mark)
        .verdict;
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
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.carol);

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
        downloadAs(w, std::nullopt, {next.header}, next.ciphertext, w.carol);

    QCOMPARE(check.verdict, Verdict::Accept);
    QVERIFY(check.document.has_value());
}

void ReconcileTest::aVersionWrittenBySomeoneWhoJustLeftIsAccepted()
{
    const World w = world();

    // Bob writes the version that drops him. He is on neither its recipients nor
    // its roster.
    const Version leave = version(w.documentId, 1, 2, {w.alice, w.carol}, w.bob, w.dataKey, w.admins());

    const DownloadCheck withBase =
        downloadAs(w, w.base.header, {leave.header}, leave.ciphertext, w.carol);
    QCOMPARE(withBase.verdict, Verdict::Accept);
    QVERIFY(!withBase.document.value().accessRoster().contains(w.bob.member));

    const DownloadCheck withoutBase =
        downloadAs(w, std::nullopt, {leave.header}, leave.ciphertext, w.carol);
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
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.carol)));

    // The control: the same version is readable by Alice, whose entry is sound.
    QCOMPARE(downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.alice)
                 .verdict,
             Verdict::Accept);
}

void ReconcileTest::aPayloadHashMismatchIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);

    // Content the header never vouched for, encrypted under that header's own
    // nonce and key by someone who holds the key. It decrypts cleanly, so only the
    // payload hash stands in the way.
    const QByteArray plaintext =
        QJsonDocument(payloadFor(w.everyone(), w.admins())).toJson(QJsonDocument::Compact);
    const QByteArray substituted = DocumentCrypto::encryptPayload(plaintext, w.dataKey, next.header);
    QVERIFY(DocumentCrypto::decryptPayload(substituted, w.dataKey, next.header).has_value());
    QVERIFY(substituted != next.ciphertext);

    const DownloadCheck check = downloadAs(w, w.base.header, {next.header}, substituted, w.carol);
    QVERIFY(isBareFault(check));
    QCOMPARE(check.reason, QObject::tr("The server's document does not match its header."));
}

void ReconcileTest::aBadSignatureIsAServerFault()
{
    const World w = world();
    Version next = bobsEdit(w);
    next.header.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\0');

    // With no base header the chain is not walked, so the signature check stands
    // alone.
    QVERIFY(isBareFault(
        downloadAs(w, std::nullopt, {next.header}, next.ciphertext, w.carol)));
    QVERIFY(isBareFault(
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.carol)));
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
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.alice)));
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

    QCOMPARE(downloadAs(w, w.base.header, {granted.header, davesEdit.header}, davesEdit.ciphertext, w.carol)
                 .verdict,
             Verdict::Accept);

    QVERIFY(isBareFault(downloadAs(w, w.base.header, {davesEdit.header}, davesEdit.ciphertext, w.carol)));
}

void ReconcileTest::aSubstitutedHeaderIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);

    // A header from another ward, signed by one of its own members, served in
    // place of this ward's.
    const World elsewhere = world();
    const Version foreign = bobsEdit(elsewhere);

    QVERIFY(isBareFault(downloadAs(w, w.base.header, {foreign.header}, next.ciphertext, w.carol)));
}

void ReconcileTest::aVersionBelowTheHighWaterMarkIsAServerFault()
{
    const World w = world();
    const Version next = bobsEdit(w);
    const qint64 counter = next.header.saveCounter;

    // The version this device last saw, seen again, is not older than itself.
    QCOMPARE(verdictAgainst(w, next, w.carol, Reconcile::HighWaterMark{1, counter}), Verdict::Accept);

    // An older key, or an older version under the current key.
    QCOMPARE(verdictAgainst(w, next, w.carol, Reconcile::HighWaterMark{2, 0}), Verdict::ServerFault);
    QCOMPARE(verdictAgainst(w, next, w.carol, Reconcile::HighWaterMark{1, counter + 1}), Verdict::ServerFault);

    // A later generation outranks any counter from an earlier one.
    QCOMPARE(verdictAgainst(w, next, w.carol, Reconcile::HighWaterMark{0, counter + 1000}), Verdict::Accept);
}

void ReconcileTest::anEntryWhoseKeysDoNotMatchItsFingerprintIsAServerFault()
{
    const World w = world();
    Version next = bobsEdit(w);

    // Carol's fingerprint, with another signing key behind it: the key would still
    // unseal. A fingerprint fixes both keys, so this is refused as an ill-formed
    // header before any check of the entry against this device.
    for (RecipientEntry& entry : next.header.recipients)
    {
        if (entry.fingerprint == w.carol.fingerprint())
        {
            entry.verifyKey = DocumentCrypto::generateSigningKeypair().verifyKey;
        }
    }
    next = seal(next.header, w.bob, w.dataKey, payloadFor(w.everyone(), w.admins()));

    const DownloadCheck check = downloadAs(w, std::nullopt, {next.header}, next.ciphertext, w.carol);
    QVERIFY(isBareFault(check));
    QCOMPARE(check.reason, QObject::tr("The server's version is not a well-formed header."));
}

void ReconcileTest::aWriterListedButMissingFromTheRosterIsAServerFault()
{
    const World w = world();

    // Bob writes a version that still lists him, so it is not a leave, but its
    // roster leaves him out: an entry on the access list nobody can attribute. The
    // header and roster disagree, which is what refuses it.
    const DocumentHeader header =
        buildHeader(w.documentId, 1, 2, w.everyone(), w.bob, w.dataKey, w.admins());
    const Version next = seal(header, w.bob, w.dataKey, payloadFor({w.alice, w.carol}, w.admins()));

    const DownloadCheck check = downloadAs(w, std::nullopt, {next.header}, next.ciphertext, w.carol);
    QVERIFY(isBareFault(check));
    QCOMPARE(check.reason, QObject::tr("The server's version disagrees with its own access list."));
}

void ReconcileTest::aVersionOfAnotherDocumentIsAServerFault()
{
    const World w = world();

    // Another ward that Carol also belongs to, sealed to her own device: every
    // check but the document id passes, which the control shows.
    const QString otherId = newDocumentId();
    const DataKey otherKey = DocumentCrypto::generateDataKey();
    const Version foreign = version(otherId, 1, 2, w.everyone(), w.alice, otherKey, w.admins());

    QCOMPARE(Reconcile::validateDownload(otherId,
                                         std::nullopt,
                                         {foreign.header},
                                         foreign.ciphertext,
                                         selfOf(w.carol),
                                         FirstGeneration)
                 .verdict,
             Verdict::Accept);

    const DownloadCheck check =
        downloadAs(w, std::nullopt, {foreign.header}, foreign.ciphertext, w.carol);
    QVERIFY(isBareFault(check));
    QCOMPARE(check.reason, QObject::tr("The server sent a version of a different document."));

    // Nor does a base header of the other ward prove anything about this one.
    const Version otherBase = version(otherId, 1, 1, w.everyone(), w.alice, otherKey, w.admins());
    const Version removal = version(otherId,
                                    2,
                                    3,
                                    {w.alice, w.bob},
                                    w.alice,
                                    DocumentCrypto::generateDataKey(),
                                    w.admins());
    QCOMPARE(Reconcile::checkRemoval(otherId, otherBase.header, {removal.header}, w.carol.fingerprint()),
             Verdict::Removed);
    QCOMPARE(Reconcile::checkRemoval(w.documentId, otherBase.header, {removal.header}, w.carol.fingerprint()),
             Verdict::ServerFault);
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
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.bob)));
}

void ReconcileTest::anEmptyHeaderListIsAServerFault()
{
    const World w = world();

    QVERIFY(isBareFault(
        downloadAs(w, w.base.header, {}, w.base.ciphertext, w.carol)));
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

    const DownloadCheck check = downloadAs(w, w.base.header, {removal.header}, removal.ciphertext, w.carol);

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

    QCOMPARE(downloadAs(w, w.base.header, {granted.header, removal.header}, removal.ciphertext, w.carol)
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

    QCOMPARE(downloadAs(w, w.base.header, {leave.header}, leave.ciphertext, bobPhone)
                 .verdict,
             Verdict::Removed);
    QCOMPARE(Reconcile::checkRemoval(w.documentId, w.base.header, {leave.header}, bobPhone.fingerprint()),
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
    QVERIFY(isBareFault(downloadAs(w, std::nullopt, {removal.header}, removal.ciphertext, w.carol)));
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

    QCOMPARE(Reconcile::checkRemoval(w.documentId, w.base.header, {removal.header}, carol), Verdict::Removed);
    QCOMPARE(Reconcile::checkRemoval(w.documentId, std::nullopt, {removal.header}, carol), Verdict::ServerFault);
    QCOMPARE(Reconcile::checkRemoval(w.documentId, w.base.header, {}, carol), Verdict::ServerFault);

    // Still listed is not removed.
    QCOMPARE(Reconcile::checkRemoval(w.documentId, w.base.header, {removal.header}, w.bob.fingerprint()),
             Verdict::ServerFault);

    // A removal written by a member does not hold, so it proves nothing.
    const Version byMember = version(w.documentId,
                                     2,
                                     2,
                                     {w.alice, w.bob},
                                     w.bob,
                                     DocumentCrypto::generateDataKey(),
                                     w.admins());
    QCOMPARE(Reconcile::checkRemoval(w.documentId, w.base.header, {byMember.header}, carol), Verdict::ServerFault);
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
        downloadAs(w, w.base.header, {next.header}, next.ciphertext, w.carol);
    QCOMPARE(check.verdict, Verdict::NeedsNewerVersion);
    QVERIFY(!check.document.has_value());
}
