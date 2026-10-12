#include "ReconcileTest.h"

#include "DocumentFile.h"
#include "Reconcile.h"
#include "ServerTestSupport.h"

#include <QJsonDocument>
#include <QtTest>

using Reconcile::DownloadCheck;
using Reconcile::Verdict;
using ServerTestSupport::TestDevice;

namespace
{

Reconcile::Self selfOf(const TestDevice& device)
{
    return Reconcile::Self{device.sealing, device.signing.verifyKey};
}

/// A roster of these devices, each its own person, the first an admin who invited
/// the rest.
SealingSet setOf(const QList<TestDevice>& devices)
{
    const MemberId maker = ServerTestSupport::newMember();
    SealingSet set = SealingSet::of(maker, devices.first().name, NamedDevice{devices.first().name, devices.first().keys()});
    for (qsizetype i = 1; i < devices.size(); ++i)
    {
        Invite invite;
        invite.purpose = Invite::For::User;
        invite.person = maker;
        invite.label = "for " + devices.at(i).name;
        invite.keys = ServerTestSupport::newDevice("Invite").keys();
        set.addInvite(invite);
        set.join(invite.fingerprint(),
                 NamedDevice{devices.at(i).name, devices.at(i).keys()},
                 ServerTestSupport::newMember(),
                 devices.at(i).name);
    }
    return set;
}

/// A ward sealed to Alice and Bob under its first key, with one family.
struct World
{
    QString documentId;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    DataKey dataKey;
    Document document;
    DocumentFile::Sealed sealed;
};

World world()
{
    World w;
    w.documentId = ServerTestSupport::newDocumentId();
    w.alice = ServerTestSupport::newDevice("Alice");
    w.bob = ServerTestSupport::newDevice("Bob");
    w.carol = ServerTestSupport::newDevice("Carol");
    w.dataKey = DocumentCrypto::generateDataKey();
    w.document = Document::empty();
    w.document.setSealingSet(setOf({w.alice, w.bob}));

    DocumentHeader first;
    first.documentId = w.documentId;
    w.sealed = DocumentFile::seal(w.document, first, w.dataKey).value();
    return w;
}

DownloadCheck check(const World& w,
                    const DocumentFile::Sealed& sealed,
                    const TestDevice& reader,
                    const QList<DataKey>& trusted)
{
    return Reconcile::validate(w.documentId, sealed.header, sealed.ciphertext, selfOf(reader), trusted);
}

/// A fault, and nothing else set.
bool isBareFault(const DownloadCheck& result)
{
    return result.verdict == Verdict::ServerFault && !result.reason.isEmpty() && !result.header.has_value()
        && !result.dataKey.has_value() && !result.document.has_value();
}

} // namespace

void ReconcileTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Accept ------------------------------------------------------------------------

void ReconcileTest::aValidVersionIsAccepted()
{
    const World w = world();
    const DownloadCheck result = check(w, w.sealed, w.bob, {});
    QCOMPARE(result.verdict, Verdict::Accept);
    QVERIFY(result.header.value() == w.sealed.header);
    QVERIFY(result.dataKey.value() == w.dataKey);
    QVERIFY(result.document.value() == w.document);
}

void ReconcileTest::aVersionUnderATrustedKeyIsAccepted()
{
    const World w = world();
    QCOMPARE(check(w, w.sealed, w.bob, {DocumentCrypto::generateDataKey(), w.dataKey}).verdict, Verdict::Accept);
}

void ReconcileTest::aVersionNamingATrustedKeyAmongItsFormerKeysIsAccepted()
{
    const World w = world();

    // Two rotations since the key Bob holds: the second still names it.
    Document rotated = w.document;
    SealingSet set = rotated.sealingSet();
    set.addFormerKey(w.dataKey);
    const DataKey middle = DocumentCrypto::generateDataKey();
    set.addFormerKey(middle);
    rotated.setSealingSet(set);
    const DataKey latest = DocumentCrypto::generateDataKey();
    const DocumentFile::Sealed sealed = DocumentFile::seal(rotated, w.sealed.header, latest).value();

    const DownloadCheck result = check(w, sealed, w.bob, {w.dataKey});
    QCOMPARE(result.verdict, Verdict::Accept);
    QVERIFY(result.dataKey.value() == latest);
}

void ReconcileTest::withNothingTrustedTheFirstVersionIsTakenAsItStands()
{
    World w = world();
    Document other = w.document;
    const DocumentFile::Sealed sealed =
        DocumentFile::seal(other, w.sealed.header, DocumentCrypto::generateDataKey()).value();
    QCOMPARE(check(w, sealed, w.bob, {}).verdict, Verdict::Accept);
}

// --- Failures ----------------------------------------------------------------------

void ReconcileTest::aVersionNotDescendedFromATrustedKeyIsAServerFault()
{
    const World w = world();

    // Everything a server could make: sealed to Bob and to a device of its own, under
    // a key of its own, naming keys it made up.
    Document forged = w.document;
    SealingSet set = setOf({w.bob, w.carol});
    set.addFormerKey(DocumentCrypto::generateDataKey());
    forged.setSealingSet(set);
    const DocumentFile::Sealed sealed =
        DocumentFile::seal(forged, w.sealed.header, DocumentCrypto::generateDataKey()).value();

    QVERIFY(isBareFault(check(w, sealed, w.bob, {w.dataKey})));

    // Anchor: the same version is taken by a device that trusts nothing yet.
    QCOMPARE(check(w, sealed, w.bob, {}).verdict, Verdict::Accept);
}

void ReconcileTest::aVersionNotSealedToThisDeviceIsAServerFault()
{
    const World w = world();
    QCOMPARE(check(w, w.sealed, w.alice, {}).verdict, Verdict::Accept);
    QVERIFY(isBareFault(check(w, w.sealed, w.carol, {})));
}

void ReconcileTest::anEntryThisDeviceCannotUnsealIsAServerFault()
{
    const World w = world();
    DocumentFile::Sealed sealed = w.sealed;
    for (RecipientEntry& entry : sealed.header.recipients)
    {
        if (entry.fingerprint == w.bob.fingerprint())
        {
            entry.sealedKey = DocumentCrypto::sealDataKey(w.dataKey, w.carol.sealing.publicKey);
        }
    }
    QVERIFY(isBareFault(check(w, sealed, w.bob, {})));
}

void ReconcileTest::aTamperedHeaderIsAServerFault()
{
    const World w = world();
    DocumentFile::Sealed counter = w.sealed;
    counter.header.saveCounter += 1;
    QVERIFY(isBareFault(check(w, counter, w.bob, {})));

    DocumentFile::Sealed nonce = w.sealed;
    nonce.header.nonce[0] = char(nonce.header.nonce.at(0) ^ 0x01);
    QVERIFY(isBareFault(check(w, nonce, w.bob, {})));
}

void ReconcileTest::aVersionOfAnotherDocumentIsAServerFault()
{
    const World w = world();
    const DownloadCheck result =
        Reconcile::validate(ServerTestSupport::newDocumentId(), w.sealed.header, w.sealed.ciphertext, selfOf(w.bob), {});
    QVERIFY(isBareFault(result));
}

void ReconcileTest::aHeaderDisagreeingWithItsSealingSetIsAServerFault()
{
    const World w = world();

    // A header naming Carol, whom the payload does not seal to.
    DocumentFile::Sealed wider = w.sealed;
    wider.header.recipients.append(
        RecipientEntry{w.carol.fingerprint(), DocumentCrypto::sealDataKey(w.dataKey, w.carol.sealing.publicKey)});
    const QByteArray plaintext = QJsonDocument(w.document.toJson()).toJson(QJsonDocument::Compact);
    wider.ciphertext = DocumentCrypto::encryptPayload(plaintext, w.dataKey, wider.header);
    QVERIFY(isBareFault(check(w, wider, w.bob, {})));

    // A payload sealing to Carol too, with a header that leaves her out.
    Document more = w.document;
    more.setSealingSet(setOf({w.alice, w.bob, w.carol}));
    DocumentFile::Sealed narrower = w.sealed;
    narrower.ciphertext = DocumentCrypto::encryptPayload(
        QJsonDocument(more.toJson()).toJson(QJsonDocument::Compact), w.dataKey, narrower.header);
    QVERIFY(isBareFault(check(w, narrower, w.bob, {})));

    QVERIFY(Reconcile::headerMatchesSealingSet(w.sealed.header, w.document.sealingSet()));
    QVERIFY(!Reconcile::headerMatchesSealingSet(wider.header, w.document.sealingSet()));
}

void ReconcileTest::anUnreadablePayloadIsAServerFault()
{
    const World w = world();
    DocumentFile::Sealed sealed = w.sealed;
    sealed.ciphertext = DocumentCrypto::encryptPayload("not a document", w.dataKey, sealed.header);
    QVERIFY(isBareFault(check(w, sealed, w.bob, {})));

    DocumentFile::Sealed flipped = w.sealed;
    flipped.ciphertext[4] = char(flipped.ciphertext.at(4) ^ 0x01);
    QVERIFY(isBareFault(check(w, flipped, w.bob, {})));
}

// --- Newer versions ----------------------------------------------------------------

void ReconcileTest::aNewerHeaderFormatIsNeedsNewerVersion()
{
    const World w = world();
    QJsonObject header = w.sealed.header.toJson();
    header["formatVersion"] = DocumentHeader::CurrentFormatVersion + 1;

    const DownloadCheck result =
        Reconcile::validateDownload(w.documentId, header, w.sealed.ciphertext, selfOf(w.bob), {});
    QCOMPARE(result.verdict, Verdict::NeedsNewerVersion);
    QVERIFY(!result.document.has_value());

    // Anchor: the stored header itself reads.
    QCOMPARE(Reconcile::validateDownload(w.documentId, w.sealed.header.toJson(), w.sealed.ciphertext, selfOf(w.bob), {})
                 .verdict,
             Verdict::Accept);
}

void ReconcileTest::aMalformedHeaderIsAServerFault()
{
    const World w = world();
    QJsonObject header = w.sealed.header.toJson();
    header.remove("nonce");
    QVERIFY(isBareFault(Reconcile::validateDownload(w.documentId, header, w.sealed.ciphertext, selfOf(w.bob), {})));
}
