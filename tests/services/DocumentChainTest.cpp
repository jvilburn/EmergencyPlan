#include "DocumentChainTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QtTest>

using namespace ChainTestSupport;

namespace
{

/// Alice and Bob, one device each, at generation 1. Alice is the only admin.
struct TwoPeople
{
    QString documentId;
    DataKey dataKey;
    TestDevice alice;
    TestDevice bob;
    DocumentHeader header;
};

TwoPeople twoPeople()
{
    TwoPeople world;
    world.documentId = newDocumentId();
    world.dataKey = DocumentCrypto::generateDataKey();
    world.alice = newPerson();
    world.bob = newPerson();
    world.header = buildHeader(world.documentId,
                               1,
                               1,
                               {world.alice, world.bob},
                               world.alice,
                               world.dataKey);
    return world;
}

} // namespace

void DocumentChainTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Payload -----------------------------------------------------------------

void DocumentChainTest::payloadRoundTrips()
{
    const TwoPeople world = twoPeople();
    const QByteArray plaintext = "{\"families\":[],\"note\":\"compressible aaaaaaaaaaaaaaaaaaaa\"}";

    const QByteArray sealed =
        DocumentCrypto::encryptPayload(plaintext, world.dataKey, world.header);
    QVERIFY(!sealed.isEmpty());

    const std::optional<QByteArray> opened =
        DocumentCrypto::decryptPayload(sealed, world.dataKey, world.header);
    QVERIFY(opened.has_value());
    QCOMPARE(opened.value(), plaintext);
}

void DocumentChainTest::tamperedHeaderFailsDecryption()
{
    const TwoPeople world = twoPeople();
    const QByteArray plaintext = "the ward directory";
    const QByteArray sealed =
        DocumentCrypto::encryptPayload(plaintext, world.dataKey, world.header);

    // Each field on its own, because the associated data has to cover all of
    // them and a test that changed several at once would not say which.
    DocumentHeader otherGeneration = world.header;
    otherGeneration.keyGeneration += 1;
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, otherGeneration).has_value());

    DocumentHeader otherCounter = world.header;
    otherCounter.saveCounter += 1;
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, otherCounter).has_value());

    DocumentHeader otherTime = world.header;
    otherTime.lastWriteTime = otherTime.lastWriteTime.addMSecs(1);
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, otherTime).has_value());

    DocumentHeader droppedRecipient = world.header;
    droppedRecipient.recipients.removeLast();
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, droppedRecipient).has_value());

    DocumentHeader otherDocument = world.header;
    otherDocument.documentId = newDocumentId();
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, otherDocument).has_value());

    DocumentHeader otherAdmins = world.header;
    otherAdmins.admins = canonicalAdmins({world.alice.member, world.bob.member});
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, world.dataKey, otherAdmins).has_value());

    // The two fields outside the associated data must NOT affect decryption.
    DocumentHeader otherHash = world.header;
    otherHash.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x99');
    QVERIFY(DocumentCrypto::decryptPayload(sealed, world.dataKey, otherHash).has_value());
}

void DocumentChainTest::truncatedPayloadFailsDecryption()
{
    const TwoPeople world = twoPeople();
    const QByteArray sealed =
        DocumentCrypto::encryptPayload("the ward directory", world.dataKey, world.header);

    QVERIFY(!DocumentCrypto::decryptPayload(sealed.left(sealed.size() - 1), world.dataKey,
                                            world.header)
                 .has_value());
    QVERIFY(!DocumentCrypto::decryptPayload(QByteArray(), world.dataKey, world.header).has_value());

    QByteArray corrupted = sealed;
    corrupted[0] = static_cast<char>(corrupted.at(0) ^ 0xff);
    QVERIFY(!DocumentCrypto::decryptPayload(corrupted, world.dataKey, world.header).has_value());
}

void DocumentChainTest::wrongKeyFailsDecryption()
{
    const TwoPeople world = twoPeople();
    const QByteArray sealed =
        DocumentCrypto::encryptPayload("the ward directory", world.dataKey, world.header);

    QVERIFY(!DocumentCrypto::decryptPayload(sealed, DocumentCrypto::generateDataKey(), world.header)
                 .has_value());
}

void DocumentChainTest::everyNonceIsFresh()
{
    QSet<QByteArray> seen;
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        const QByteArray nonce = DocumentCrypto::generateNonce();
        QCOMPARE(nonce.size(), qsizetype(DocumentCrypto::NonceSize));
        QVERIFY(!seen.contains(nonce));
        seen.insert(nonce);
    }
}

// --- Header signing ----------------------------------------------------------

void DocumentChainTest::signatureVerifiesWithWriterKey()
{
    const TwoPeople world = twoPeople();
    QVERIFY(DocumentCrypto::verifyHeader(world.header));
}

void DocumentChainTest::signatureFailsOnChangedHeaderField()
{
    const TwoPeople world = twoPeople();

    DocumentHeader changed = world.header;
    changed.saveCounter += 1;
    QVERIFY(!DocumentCrypto::verifyHeader(changed));

    DocumentHeader otherAdmins = world.header;
    otherAdmins.admins = canonicalAdmins({world.alice.member, world.bob.member});
    QVERIFY(!DocumentCrypto::verifyHeader(otherAdmins));
}

void DocumentChainTest::signatureFailsOnChangedPayloadHash()
{
    const TwoPeople world = twoPeople();

    // The hash is outside the associated data but inside the signature, which is
    // what binds a header to its payload.
    DocumentHeader changed = world.header;
    changed.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x99');
    QVERIFY(!DocumentCrypto::verifyHeader(changed));
}

void DocumentChainTest::signatureFailsWithAnotherMembersKey()
{
    const TwoPeople world = twoPeople();

    // Alice signed it; a header claiming Bob's verify key does not verify.
    DocumentHeader claimed = world.header;
    claimed.writerVerifyKey = world.bob.signing.verifyKey;
    QVERIFY(!DocumentCrypto::verifyHeader(claimed));
}

void DocumentChainTest::writerIsAccountedForByTheRosterOrAsALeave()
{
    const TwoPeople world = twoPeople();
    const AccessRoster roster = rosterOf({world.alice, world.bob});

    // A writer in the roster, with the keys the header carries for it.
    QVERIFY(DocumentCrypto::writerIsAccountedFor(world.header, roster));

    // A writer the header lists but the roster does not: an entry nobody can
    // attribute.
    const AccessRoster withoutAlice = rosterOf({world.bob});
    QVERIFY(!DocumentCrypto::writerIsAccountedFor(world.header, withoutAlice));

    // A writer in neither: a leave, whose writer has just gone.
    const DocumentHeader leave =
        buildHeader(world.documentId, 1, 2, {world.bob}, world.alice, world.dataKey);
    QVERIFY(DocumentCrypto::writerIsAccountedFor(leave, withoutAlice));

    // A writer in the roster but not in the header is neither.
    QVERIFY(!DocumentCrypto::writerIsAccountedFor(leave, roster));
}

// --- Links: writer and counters ----------------------------------------------

void DocumentChainTest::linkWithWriterNotInPreviousIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice stranger = newPerson();

    // A server minting a header that lists a key of its own and signs with it.
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob, stranger},
                                            stranger,
                                            world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkWithBadSignatureIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob},
                                      world.alice,
                                      world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, next));

    next.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\x00');
    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkWithHeaderSignedUnderAuthTagIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob},
                                      world.alice,
                                      world.dataKey);

    // A login challenge the server chose, signed by a real member, must never
    // stand in as document evidence.
    next.writerSignature = DocumentCrypto::sign(SignatureTag::Auth,
                                                next.signedBytes(),
                                                world.alice.signing.signingKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkSkippingAKeyGenerationIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice alicePhone = newDeviceFor(world.alice.member);
    const DocumentHeader before = buildHeader(world.documentId,
                                              1,
                                              1,
                                              {world.alice, world.bob, alicePhone},
                                              world.alice,
                                              world.dataKey);

    // A drop that earns a rotation, with the generation stepped by two. A jump
    // would let one member pin the generation at the maximum and block every later
    // rotation, so the rule is exactly one - and a step of one is the control.
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader byOne =
        buildHeader(world.documentId, 2, 2, {world.alice, world.bob}, world.alice, rotated);
    QVERIFY(DocumentCrypto::verifyLink(before, byOne));

    const DocumentHeader byTwo =
        buildHeader(world.documentId, 3, 2, {world.alice, world.bob}, world.alice, rotated);
    QVERIFY(!DocumentCrypto::verifyLink(before, byTwo));

    // And without any drop, any step at all.
    const DocumentHeader unchanged = buildHeader(world.documentId,
                                                 3,
                                                 2,
                                                 {world.alice, world.bob},
                                                 world.alice,
                                                 world.dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, unchanged));
}

void DocumentChainTest::linkWithDecreasingGenerationIsBroken()
{
    const TwoPeople world = twoPeople();
    const DocumentHeader later = buildHeader(world.documentId,
                                             5,
                                             2,
                                             {world.alice, world.bob},
                                             world.alice,
                                             world.dataKey);
    const DocumentHeader back = buildHeader(world.documentId,
                                            4,
                                            3,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(later, back));
}

void DocumentChainTest::linkWithNonIncreasingSaveCounterIsBroken()
{
    const TwoPeople world = twoPeople();

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            world.header.saveCounter + 1,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, next));

    const DocumentHeader same = buildHeader(world.documentId,
                                            1,
                                            world.header.saveCounter,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, same));

    const DocumentHeader back = buildHeader(world.documentId,
                                            1,
                                            world.header.saveCounter - 1,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, back));

    // Any rise links: the step bound is the server's, because a chain walked with
    // headers missing sees larger gaps than any one upload made.
    const DocumentHeader farAhead = buildHeader(world.documentId,
                                                1,
                                                world.header.saveCounter
                                                    + 10 * DocumentHeader::MaxSaveCounterStep,
                                                {world.alice, world.bob},
                                                world.alice,
                                                world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, farAhead));
}

void DocumentChainTest::linkBeyondTheSaveCounterCapIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader atCap = buildHeader(world.documentId,
                                       1,
                                       DocumentHeader::MaxSaveCounter,
                                       {world.alice, world.bob},
                                       world.alice,
                                       world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, atCap));

    DocumentHeader beyond = atCap;
    beyond.saveCounter = DocumentHeader::MaxSaveCounter + 1;
    resign(beyond, world.alice);
    QVERIFY(!DocumentCrypto::isCoherent(beyond));
    QVERIFY(!DocumentCrypto::verifyLink(world.header, beyond));
}

void DocumentChainTest::coherenceHoldsWhatParseHolds()
{
    const TwoPeople world = twoPeople();
    QVERIFY(DocumentCrypto::isCoherent(world.header));

    // A header built in memory is judged by the rules a parsed one meets.
    DocumentHeader generationZero = world.header;
    generationZero.keyGeneration = 0;
    QVERIFY(!DocumentCrypto::isCoherent(generationZero));

    DocumentHeader otherFormat = world.header;
    otherFormat.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    QVERIFY(!DocumentCrypto::isCoherent(otherFormat));

    DocumentHeader upperCaseId = world.header;
    upperCaseId.documentId = world.header.documentId.toUpper();
    QVERIFY(upperCaseId.documentId != world.header.documentId);
    QVERIFY(!DocumentCrypto::isCoherent(upperCaseId));

    DocumentHeader shortOrigin = world.header;
    shortOrigin.originDocumentId = "0011";
    QVERIFY(!DocumentCrypto::isCoherent(shortOrigin));

    DocumentHeader withOrigin = world.header;
    withOrigin.originDocumentId = newDocumentId();
    QVERIFY(DocumentCrypto::isCoherent(withOrigin));
}

void DocumentChainTest::linkChangingOriginOrFormatIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader withOrigin = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, withOrigin));

    withOrigin.originDocumentId = newDocumentId();
    resign(withOrigin, world.alice);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, withOrigin));

    DocumentHeader otherFormat = buildHeader(world.documentId,
                                             1,
                                             2,
                                             {world.alice, world.bob},
                                             world.alice,
                                             world.dataKey);
    otherFormat.formatVersion += 1;
    resign(otherFormat, world.alice);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, otherFormat));
}

void DocumentChainTest::linkAcrossDocumentsIsBroken()
{
    const TwoPeople world = twoPeople();

    // Splicing another ward's headers into this chain.
    const DocumentHeader elsewhere = buildHeader(newDocumentId(),
                                                 1,
                                                 2,
                                                 {world.alice, world.bob},
                                                 world.alice,
                                                 world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, elsewhere));
}

// --- Links: device identity --------------------------------------------------

void DocumentChainTest::linkWithFingerprintNotMatchingKeysIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice bobPhone = newDeviceFor(world.bob.member);

    // Bob links his own new phone: no drop, no rotation, his own person, so every
    // cross-header rule is satisfied.
    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob, bobPhone},
                                      world.bob,
                                      world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, next));

    // The phone's entry then claims a fingerprint that belongs to nobody. Only the
    // recompute-and-compare check stands in the way.
    Fingerprint unrelated;
    unrelated.bytes = DocumentCrypto::generateDataKey().bytes.left(Fingerprint::Size);
    next.recipients[2].fingerprint = unrelated;
    resign(next, world.bob);

    QVERIFY(!DocumentCrypto::isCoherent(next));
    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkWithDeviceUnderTwoPeopleIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob, world.bob},
                                      world.alice,
                                      world.dataKey);
    next.recipients[2].member = newMember();
    resign(next, world.alice);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkWithDuplicateVerifyKeyIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice extra = newDeviceFor(world.alice.member);

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob, extra},
                                      world.alice,
                                      world.dataKey);

    // Two devices claiming one signing key: a second entry that can forge the
    // first one's signatures.
    next.recipients[2].verifyKey = world.bob.signing.verifyKey;
    next.recipients[2].fingerprint =
        DocumentCrypto::fingerprintOf(next.recipients.at(2).publicKey, world.bob.signing.verifyKey);
    resign(next, world.alice);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkMovingDeviceBetweenPeopleIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone}, alice, dataKey);

    // Bob keeps a device, so Bob's person does not vanish; no device is added or
    // dropped, so the add and drop rules never apply. Only rule 4's cross-header
    // binding can refuse this.
    //
    // What it stops: a device relabelled as another person's, acting with that
    // person's rights.
    DocumentHeader next =
        buildHeader(documentId, 1, 2, {alice, bob, bobPhone}, alice, dataKey);
    next.recipients[1].member = alice.member;
    resign(next, alice);

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkChangingPublicKeyOfExistingDeviceIsBroken()
{
    const TwoPeople world = twoPeople();
    const Keypair replacement = DocumentCrypto::generateKeypair();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob},
                                      world.alice,
                                      world.dataKey);

    // Same fingerprint claimed, different sealing key behind it.
    next.recipients[1].publicKey = replacement.publicKey;
    next.recipients[1].sealedKey =
        DocumentCrypto::sealDataKey(world.dataKey, replacement.publicKey);
    resign(next, world.alice);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkChangingVerifyKeyOfExistingDeviceIsBroken()
{
    const TwoPeople world = twoPeople();
    const SigningKeypair replacement = DocumentCrypto::generateSigningKeypair();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob},
                                      world.alice,
                                      world.dataKey);
    next.recipients[1].verifyKey = replacement.verifyKey;
    resign(next, world.alice);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

// --- Links: the admin list ---------------------------------------------------

void DocumentChainTest::linkWithAdminNotAmongThePeopleIsBroken()
{
    const TwoPeople world = twoPeople();

    // An admin nobody can find in the header could never be checked against a
    // device.
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey,
                                            {world.alice.member, newMember()});

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkWithNonCanonicalAdminListIsBroken()
{
    const TwoPeople world = twoPeople();
    const QList<MemberId> both = canonicalAdmins({world.alice.member, world.bob.member});

    // Written directly, past buildHeader's ordering, so the in-memory header
    // is exactly as a hostile writer would sign it.
    const DocumentHeader canonical = buildHeader(world.documentId,
                                                 1,
                                                 2,
                                                 {world.alice, world.bob},
                                                 world.alice,
                                                 world.dataKey,
                                                 both);
    QVERIFY(DocumentCrypto::verifyLink(world.header, canonical));

    DocumentHeader reversed = canonical;
    reversed.admins = {both.at(1), both.at(0)};
    resign(reversed, world.alice);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, reversed));

    DocumentHeader repeated = canonical;
    repeated.admins = {both.at(0), both.at(0)};
    resign(repeated, world.alice);
    QVERIFY(!DocumentCrypto::verifyLink(world.header, repeated));
}

void DocumentChainTest::linkLeavingPeopleWithNoAdminIsBroken()
{
    const TwoPeople world = twoPeople();

    // A ward with people and nobody able to change who they are.
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey,
                                            {});

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

// --- Links: roles ------------------------------------------------------------

void DocumentChainTest::linkAdminPromotingAMemberHolds()
{
    const TwoPeople world = twoPeople();

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey,
                                            {world.alice.member, world.bob.member});

    QVERIFY(DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkAdminSteppingDownWhenAnotherRemainsHolds()
{
    const TwoPeople world = twoPeople();
    const DocumentHeader bothAdmins = buildHeader(world.documentId,
                                                  1,
                                                  1,
                                                  {world.alice, world.bob},
                                                  world.alice,
                                                  world.dataKey,
                                                  {world.alice.member, world.bob.member});

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey,
                                            {world.bob.member});

    QVERIFY(DocumentCrypto::verifyLink(bothAdmins, next));
}

void DocumentChainTest::linkMemberChangingTheAdminListIsBroken()
{
    const TwoPeople world = twoPeople();

    // Bob making himself an admin.
    const DocumentHeader promoted = buildHeader(world.documentId,
                                                1,
                                                2,
                                                {world.alice, world.bob},
                                                world.bob,
                                                world.dataKey,
                                                {world.alice.member, world.bob.member});
    QVERIFY(!DocumentCrypto::verifyLink(world.header, promoted));

    // Or taking Alice's role away and his own in its place.
    const DocumentHeader swapped = buildHeader(world.documentId,
                                               1,
                                               2,
                                               {world.alice, world.bob},
                                               world.bob,
                                               world.dataKey,
                                               {world.bob.member});
    QVERIFY(!DocumentCrypto::verifyLink(world.header, swapped));
}

void DocumentChainTest::linkChangingARoleWithRotationIsBroken()
{
    const TwoPeople world = twoPeople();

    // Nobody lost access, so there is nothing for a new key to protect.
    const DocumentHeader next = buildHeader(world.documentId,
                                            2,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            DocumentCrypto::generateDataKey(),
                                            {world.alice.member, world.bob.member});

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

// --- Links: adding and unlinking devices -------------------------------------

void DocumentChainTest::linkAddingDeviceToOwnPersonHolds()
{
    const TwoPeople world = twoPeople();
    const TestDevice alicePhone = newDeviceFor(world.alice.member);
    const TestDevice bobPhone = newDeviceFor(world.bob.member);

    const DocumentHeader byAdmin = buildHeader(world.documentId,
                                               1,
                                               2,
                                               {world.alice, world.bob, alicePhone},
                                               world.alice,
                                               world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, byAdmin));

    // Managing your own devices takes no admin.
    const DocumentHeader byMember = buildHeader(world.documentId,
                                                1,
                                                2,
                                                {world.alice, world.bob, bobPhone},
                                                world.bob,
                                                world.dataKey);
    QVERIFY(DocumentCrypto::verifyLink(world.header, byMember));
}

void DocumentChainTest::linkAddingDeviceToAnotherPersonIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice bobPhone = newDeviceFor(world.bob.member);

    // Not even an admin adds a device to Bob's person: that is how a stolen
    // account would be given a second key.
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob, bobPhone},
                                            world.alice,
                                            world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkAdminAddingAWhollyNewPersonHolds()
{
    const TwoPeople world = twoPeople();
    const TestDevice carol = newPerson();

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob, carol},
                                            world.alice,
                                            world.dataKey);

    QVERIFY(DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkMemberAddingAWhollyNewPersonIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice carol = newPerson();

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob, carol},
                                            world.bob,
                                            world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkUnlinkingOwnDeviceWithRotationHolds()
{
    const TwoPeople world = twoPeople();
    const TestDevice bobPhone = newDeviceFor(world.bob.member);

    const DocumentHeader before = buildHeader(world.documentId,
                                              1,
                                              1,
                                              {world.alice, world.bob, bobPhone},
                                              world.alice,
                                              world.dataKey);

    // Bob, a member, drops his own phone.
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next =
        buildHeader(world.documentId, 2, 2, {world.alice, world.bob}, world.bob, rotated);

    QVERIFY(DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkAdminUnlinkingAnotherPersonsDeviceHolds()
{
    const TwoPeople world = twoPeople();
    const TestDevice bobStolenPhone = newDeviceFor(world.bob.member);

    const DocumentHeader before = buildHeader(world.documentId,
                                              1,
                                              1,
                                              {world.alice, world.bob, bobStolenPhone},
                                              world.alice,
                                              world.dataKey);

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next =
        buildHeader(world.documentId, 2, 2, {world.alice, world.bob}, world.alice, rotated);

    QVERIFY(DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkMemberUnlinkingAnotherPersonsDeviceIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice alicePhone = newDeviceFor(world.alice.member);

    const DocumentHeader before = buildHeader(world.documentId,
                                              1,
                                              1,
                                              {world.alice, world.bob, alicePhone},
                                              world.alice,
                                              world.dataKey);

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next =
        buildHeader(world.documentId, 2, 2, {world.alice, world.bob}, world.bob, rotated);

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkUnlinkingWithoutRotationIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice alicePhone = newDeviceFor(world.alice.member);

    const DocumentHeader before = buildHeader(world.documentId,
                                              1,
                                              1,
                                              {world.alice, world.bob, alicePhone},
                                              world.alice,
                                              world.dataKey);

    // The dropped device still holds the old key, so not rotating leaves it able
    // to read everything written afterwards.
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

// --- Links: removing a person ------------------------------------------------

void DocumentChainTest::linkAdminRemovingAPersonHolds()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);
    const TestDevice carol = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone, carol}, alice, dataKey);

    // Removing Bob takes both of his devices, on Alice's word alone.
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next = buildHeader(documentId, 2, 2, {alice, carol}, alice, rotated);

    QVERIFY(DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkAdminRemovingAnotherAdminHolds()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();

    const DocumentHeader before = buildHeader(
        documentId, 1, 1, {alice, bob, carol}, alice, dataKey, {alice.member, carol.member});

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next =
        buildHeader(documentId, 2, 2, {alice, bob}, alice, rotated, {alice.member});

    QVERIFY(DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkMemberRemovingAPersonIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    // Bob, a member, dropping Carol.
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next = buildHeader(documentId, 2, 2, {alice, bob}, bob, rotated);

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkUsingARoleInTheVersionThatGrantsItIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    // Bob listed as an admin in the very version in which he removes Carol. An
    // admin is always an admin of the previous version.
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader next = buildHeader(
        documentId, 2, 2, {alice, bob}, bob, rotated, {alice.member, bob.member});

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkRemovingWithoutRotationIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    const DocumentHeader next = buildHeader(documentId, 1, 2, {alice, carol}, alice, dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(before, next));
}

// --- Links: leaving ----------------------------------------------------------

void DocumentChainTest::linkMemberLeavingHolds()
{
    const TwoPeople world = twoPeople();

    // Bob writes the version that drops him. It is the one version whose writer
    // is not among its recipients.
    const DocumentHeader next =
        buildHeader(world.documentId, 1, 2, {world.alice}, world.bob, world.dataKey);

    QVERIFY(DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkLeaveFromEitherDeviceTakesThemAll()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone}, alice, dataKey);

    const DocumentHeader fromPhone = buildHeader(documentId, 1, 2, {alice}, bobPhone, dataKey);
    QVERIFY(DocumentCrypto::verifyLink(before, fromPhone));

    // Leaving a device behind is not leaving: Bob would still be in.
    const DocumentHeader partial =
        buildHeader(documentId, 1, 2, {alice, bobPhone}, bob, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(before, partial));
}

void DocumentChainTest::linkLeaveThatRotatesIsBroken()
{
    const TwoPeople world = twoPeople();

    // The leaver would generate the new key and so know it: a rotation here
    // protects nothing and only hides that one was skipped.
    const DocumentHeader next = buildHeader(
        world.documentId, 2, 2, {world.alice}, world.bob, DocumentCrypto::generateDataKey());

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

void DocumentChainTest::linkLeaveThatAlsoChangesOthersIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();
    const TestDevice dave = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    // A leave drops the writer's person and nothing else.
    const DocumentHeader takingCarol = buildHeader(documentId, 1, 2, {alice}, bob, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(before, takingCarol));

    const DocumentHeader addingDave =
        buildHeader(documentId, 1, 2, {alice, carol, dave}, bob, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(before, addingDave));

    const DocumentHeader promotingCarol = buildHeader(
        documentId, 1, 2, {alice, carol}, bob, dataKey, {alice.member, carol.member});
    QVERIFY(!DocumentCrypto::verifyLink(before, promotingCarol));
}

void DocumentChainTest::linkAdminLeavingWhenAnotherRemainsHolds()
{
    const TwoPeople world = twoPeople();
    const DocumentHeader bothAdmins = buildHeader(world.documentId,
                                                  1,
                                                  1,
                                                  {world.alice, world.bob},
                                                  world.alice,
                                                  world.dataKey,
                                                  {world.alice.member, world.bob.member});

    const DocumentHeader next = buildHeader(
        world.documentId, 1, 2, {world.bob}, world.alice, world.dataKey, {world.bob.member});

    QVERIFY(DocumentCrypto::verifyLink(bothAdmins, next));
}

void DocumentChainTest::linkLastAdminLeavingOthersBehindIsBroken()
{
    const TwoPeople world = twoPeople();

    // With no admin left the header is incoherent; naming Bob instead is a
    // promotion the leave cannot carry.
    const DocumentHeader noAdmin =
        buildHeader(world.documentId, 1, 2, {world.bob}, world.alice, world.dataKey, {});
    QVERIFY(!DocumentCrypto::verifyLink(world.header, noAdmin));

    const DocumentHeader successor = buildHeader(
        world.documentId, 1, 2, {world.bob}, world.alice, world.dataKey, {world.bob.member});
    QVERIFY(!DocumentCrypto::verifyLink(world.header, successor));
}

void DocumentChainTest::linkLastPersonLeavingHolds()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();

    const DocumentHeader before = buildHeader(documentId, 1, 1, {alice}, alice, dataKey);
    const DocumentHeader next = buildHeader(documentId, 1, 2, {}, alice, dataKey, {});

    QVERIFY(next.recipients.isEmpty());
    QVERIFY(DocumentCrypto::verifyLink(before, next));
}

void DocumentChainTest::linkAdminLeaveThatRotatesIsBroken()
{
    const TwoPeople world = twoPeople();
    const DocumentHeader bothAdmins = buildHeader(world.documentId,
                                                  1,
                                                  1,
                                                  {world.alice, world.bob},
                                                  world.alice,
                                                  world.dataKey,
                                                  {world.alice.member, world.bob.member});

    // The same leave holds without a rotation.
    const DocumentHeader leave = buildHeader(
        world.documentId, 1, 2, {world.bob}, world.alice, world.dataKey, {world.bob.member});
    QVERIFY(DocumentCrypto::verifyLink(bothAdmins, leave));

    const DocumentHeader rotating = buildHeader(world.documentId,
                                                2,
                                                2,
                                                {world.bob},
                                                world.alice,
                                                DocumentCrypto::generateDataKey(),
                                                {world.bob.member});
    QVERIFY(!DocumentCrypto::verifyLink(bothAdmins, rotating));
}

void DocumentChainTest::linkLeavingSomeoneElseIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);
    const TestDevice carol = newPerson();

    const DocumentHeader before =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone, carol}, alice, dataKey);

    // Bob's laptop, absent from its own version, while Bob stays on through his
    // phone: that drops a device, not Bob, so it is no leave.
    const DocumentHeader laptopOnly =
        buildHeader(documentId, 1, 2, {alice, bobPhone, carol}, bob, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(before, laptopOnly));

    // The same, taking Carol with it.
    const DocumentHeader takingCarol =
        buildHeader(documentId, 1, 2, {alice, bobPhone}, bob, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(before, takingCarol));
}

void DocumentChainTest::linkAfterAnEmptyHeaderIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();

    const DocumentHeader before = buildHeader(documentId, 1, 1, {alice}, alice, dataKey);
    const DocumentHeader lastLeave = buildHeader(documentId, 1, 2, {}, alice, dataKey, {});
    QVERIFY(DocumentCrypto::verifyLink(before, lastLeave));

    // Nobody is left to write the next version, so nothing can follow it - not
    // even the person who just left.
    const DocumentHeader afterwards = buildHeader(documentId, 1, 3, {alice}, alice, dataKey);
    QVERIFY(!DocumentCrypto::verifyLink(lastLeave, afterwards));
}

void DocumentChainTest::linkLeaveSignedByAKeyNotInPreviousIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice impostor = newDeviceFor(world.bob.member);

    // Bob's member id, but a key that was never Bob's in the previous header.
    const DocumentHeader next =
        buildHeader(world.documentId, 1, 2, {world.alice}, impostor, world.dataKey);

    QVERIFY(!DocumentCrypto::verifyLink(world.header, next));
}

// --- Chains ------------------------------------------------------------------

void DocumentChainTest::chainStillListingSelfIsStillMember()
{
    const TwoPeople world = twoPeople();
    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);

    QCOMPARE(DocumentCrypto::verifyChain(world.header, {next}, world.bob.fingerprint()),
             ChainResult::StillMember);
}

void DocumentChainTest::chainProvesRemovalByAdminPromotedAfterAnchor()
{
    // The case the design turns on: the anchor is A+B+C, Alice grants Dave and
    // makes him an admin, then Dave removes Carol. Carol never saw Dave arrive,
    // and must still be convinced.
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();
    const TestDevice dave = newPerson();

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    const DocumentHeader granted = buildHeader(
        documentId, 1, 2, {alice, bob, carol, dave}, alice, dataKey, {alice.member, dave.member});

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader removed = buildHeader(
        documentId, 2, 3, {alice, bob, dave}, dave, rotated, {alice.member, dave.member});

    QCOMPARE(DocumentCrypto::verifyChain(anchor, {granted, removed}, carol.fingerprint()),
             ChainResult::Removed);
}

void DocumentChainTest::chainProvesRemovalForEveryDeviceOfTheRemovedPerson()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);
    const TestDevice carol = newPerson();

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone, carol}, alice, dataKey);

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader removed = buildHeader(documentId, 2, 2, {alice, carol}, alice, rotated);

    QCOMPARE(DocumentCrypto::verifyChain(anchor, {removed}, bob.fingerprint()),
             ChainResult::Removed);
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {removed}, bobPhone.fingerprint()),
             ChainResult::Removed);
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {removed}, carol.fingerprint()),
             ChainResult::StillMember);
}

void DocumentChainTest::chainProvesUnlinkForTheUnlinkedDevice()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice aliceLostPhone = newDeviceFor(alice.member);
    const TestDevice bob = newPerson();

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, aliceLostPhone, bob}, alice, dataKey);

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader unlinked =
        buildHeader(documentId, 2, 2, {alice, bob}, alice, rotated);

    // The lost phone learns it is out, while Alice's laptop stays in.
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {unlinked}, aliceLostPhone.fingerprint()),
             ChainResult::Removed);
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {unlinked}, alice.fingerprint()),
             ChainResult::StillMember);
}

void DocumentChainTest::chainSkippingHeadersWithUnchangedMembershipVerifies()
{
    // The server keeps only headers whose membership changed, plus the current
    // one. This is what makes dropping the rest safe.
    const TwoPeople world = twoPeople();
    const TestDevice carol = newPerson();

    // Alice grants Carol; Carol then writes an ordinary edit, which the server
    // drops; Bob writes the current version.
    const QList<TestDevice> everyone = {world.alice, world.bob, carol};
    const DocumentHeader granted =
        buildHeader(world.documentId, 1, 2, everyone, world.alice, world.dataKey);
    const DocumentHeader dropped =
        buildHeader(world.documentId, 1, 3, everyone, carol, world.dataKey);
    const DocumentHeader current =
        buildHeader(world.documentId, 1, 4, everyone, world.bob, world.dataKey);

    QCOMPARE(DocumentCrypto::verifyChain(world.header,
                                         {granted, dropped, current},
                                         world.bob.fingerprint()),
             ChainResult::StillMember);

    // Without the dropped header, Bob's version links straight from the grant.
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {granted, current}, world.bob.fingerprint()),
             ChainResult::StillMember);
}

void DocumentChainTest::chainWithWithheldGrantHeaderIsBroken()
{
    // The same story as chainProvesRemovalByAdminPromotedAfterAnchor, but the
    // server withholds the header that introduced Dave. Without it, Dave writing
    // is a stranger writing, and the chain must refuse rather than delete.
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();
    const TestDevice dave = newPerson();

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader removed = buildHeader(
        documentId, 2, 3, {alice, bob, dave}, dave, rotated, {alice.member, dave.member});

    QCOMPARE(DocumentCrypto::verifyChain(anchor, {removed}, carol.fingerprint()),
             ChainResult::Broken);
}

void DocumentChainTest::chainWithWithheldPromotionHeaderIsBroken()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice carol = newPerson();

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, bob, carol}, alice, dataKey);

    // Alice makes Carol an admin; Carol removes Bob.
    const QList<MemberId> admins = {alice.member, carol.member};
    const DocumentHeader promoted =
        buildHeader(documentId, 1, 2, {alice, bob, carol}, alice, dataKey, admins);
    const DataKey rotated = DocumentCrypto::generateDataKey();
    const DocumentHeader removed =
        buildHeader(documentId, 2, 3, {alice, carol}, carol, rotated, admins);

    QCOMPARE(DocumentCrypto::verifyChain(anchor, {promoted, removed}, bob.fingerprint()),
             ChainResult::Removed);

    // Without the promotion, Carol removing anyone is a member removing someone.
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {removed}, bob.fingerprint()),
             ChainResult::Broken);
}

void DocumentChainTest::chainProvesALeaveToTheLeaversOtherDevices()
{
    const QString documentId = newDocumentId();
    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const TestDevice alice = newPerson();
    const TestDevice bob = newPerson();
    const TestDevice bobPhone = newDeviceFor(bob.member);

    const DocumentHeader anchor =
        buildHeader(documentId, 1, 1, {alice, bob, bobPhone}, alice, dataKey);

    // Bob leaves from his laptop. His phone, offline at the time, can no longer
    // decrypt, and the leave is all the evidence it has.
    const DocumentHeader leave = buildHeader(documentId, 1, 2, {alice}, bob, dataKey);

    QCOMPARE(DocumentCrypto::verifyChain(anchor, {leave}, bobPhone.fingerprint()),
             ChainResult::Removed);
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {leave}, bob.fingerprint()),
             ChainResult::Removed);
    QCOMPARE(DocumentCrypto::verifyChain(anchor, {leave}, alice.fingerprint()),
             ChainResult::StillMember);
}

void DocumentChainTest::chainWithBadSignatureIsBroken()
{
    const TwoPeople world = twoPeople();

    DocumentHeader next = buildHeader(world.documentId,
                                      1,
                                      2,
                                      {world.alice, world.bob},
                                      world.alice,
                                      world.dataKey);
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {next}, world.bob.fingerprint()),
             ChainResult::StillMember);

    next.writerSignature = QByteArray(DocumentCrypto::SignatureSize, '\x00');
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {next}, world.bob.fingerprint()),
             ChainResult::Broken);
}

void DocumentChainTest::chainWithAnchorNotListingSelfIsBroken()
{
    const TwoPeople world = twoPeople();
    const TestDevice stranger = newPerson();

    const DocumentHeader next = buildHeader(world.documentId,
                                            1,
                                            2,
                                            {world.alice, world.bob},
                                            world.alice,
                                            world.dataKey);

    // Removed is the only verdict that deletes a file. A device that was never in
    // the anchor has proved nothing about itself, however sound the chain is.
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {next}, stranger.fingerprint()),
             ChainResult::Broken);
}

void DocumentChainTest::chainWithEmptyListIsBroken()
{
    const TwoPeople world = twoPeople();

    // Never conclude anything from no evidence.
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {}, world.bob.fingerprint()),
             ChainResult::Broken);
    QCOMPARE(DocumentCrypto::verifyChain(world.header, {world.header}, Fingerprint()),
             ChainResult::Broken);
}
