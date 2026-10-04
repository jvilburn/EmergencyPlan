#include "PairingTest.h"

#include "DocumentCrypto.h"
#include "FakeServer.h"
#include "Pairing.h"
#include "ServerTestSupport.h"

#include <QRegularExpression>
#include <QSet>
#include <QtTest>

using namespace ChainTestSupport;
using namespace ServerContract;
using namespace ServerTestSupport;

namespace
{

struct Device
{
    DeviceKeys keys;
    SigningKey signingKey;
};

Device newDevice()
{
    const Keypair sealing = DocumentCrypto::generateKeypair();
    const SigningKeypair signing = DocumentCrypto::generateSigningKeypair();

    Device device;
    device.keys.publicKey = sealing.publicKey;
    device.keys.verifyKey = signing.verifyKey;
    device.signingKey = signing.signingKey;
    return device;
}

Pairing::Handover signedHandover(const Device& responder, const QByteArray& nonce)
{
    Pairing::Handover handover;
    handover.member = newMember();
    handover.displayName = "Alice Reed";
    handover.documentIds = {"00112233445566778899aabbccddeeff", "ffeeddccbbaa99887766554433221100"};
    handover.signature = Pairing::signHandover(handover, nonce, responder.signingKey);
    return handover;
}

} // namespace

void PairingTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Commitment --------------------------------------------------------------

void PairingTest::revealMatchesTheCommittedKeysAndNonce()
{
    const Device initiator = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QByteArray commitment = Pairing::commit(initiator.keys, nonce);

    QCOMPARE(commitment.size(), qsizetype(32));
    QVERIFY(Pairing::revealMatches(commitment, initiator.keys, nonce));
}

void PairingTest::revealRefusesAChangedPublicKey()
{
    const Device initiator = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QByteArray commitment = Pairing::commit(initiator.keys, nonce);

    DeviceKeys changed = initiator.keys;
    changed.publicKey = DocumentCrypto::generateKeypair().publicKey;
    QVERIFY(!Pairing::revealMatches(commitment, changed, nonce));
}

void PairingTest::revealRefusesAChangedVerifyKey()
{
    const Device initiator = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QByteArray commitment = Pairing::commit(initiator.keys, nonce);

    DeviceKeys changed = initiator.keys;
    changed.verifyKey = DocumentCrypto::generateSigningKeypair().verifyKey;
    QVERIFY(!Pairing::revealMatches(commitment, changed, nonce));
}

void PairingTest::revealRefusesAChangedNonce()
{
    const Device initiator = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QByteArray commitment = Pairing::commit(initiator.keys, nonce);

    QByteArray changed = nonce;
    changed[0] = char(changed.at(0) ^ 0x01);
    QVERIFY(!Pairing::revealMatches(commitment, initiator.keys, changed));
}

void PairingTest::revealRefusesAMalformedCommitment()
{
    const Device initiator = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QByteArray commitment = Pairing::commit(initiator.keys, nonce);
    QVERIFY(Pairing::revealMatches(commitment, initiator.keys, nonce));

    QVERIFY(!Pairing::revealMatches(commitment.left(31), initiator.keys, nonce));
    QVERIFY(!Pairing::revealMatches(QByteArray(), initiator.keys, nonce));

    QByteArray flipped = commitment;
    flipped[31] = char(flipped.at(31) ^ 0x01);
    QVERIFY(!Pairing::revealMatches(flipped, initiator.keys, nonce));
}

// --- Comparison number -------------------------------------------------------

void PairingTest::comparisonNumberIsTheSameOnBothSides()
{
    const Device initiator = newDevice();
    const Device responder = newDevice();
    const QByteArray nonce = Pairing::newNonce();

    // Each side computes from its own copy of the bytes it received.
    const DeviceKeys initiatorAsReceived{initiator.keys.publicKey, initiator.keys.verifyKey};
    const DeviceKeys responderAsReceived{responder.keys.publicKey, responder.keys.verifyKey};
    const QByteArray nonceAsReceived = QByteArray(nonce.constData(), nonce.size());

    QCOMPARE(Pairing::comparisonNumber(initiator.keys, responderAsReceived, nonce),
             Pairing::comparisonNumber(initiatorAsReceived, responder.keys, nonceAsReceived));
}

void PairingTest::comparisonNumberDiffersWhenEitherSidesKeysChange()
{
    const Device initiator = newDevice();
    const Device responder = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const QString original = Pairing::comparisonNumber(initiator.keys, responder.keys, nonce);

    // Each substitute has one chance in a million of matching by coincidence.
    QVERIFY(Pairing::comparisonNumber(newDevice().keys, responder.keys, nonce) != original);
    QVERIFY(Pairing::comparisonNumber(initiator.keys, newDevice().keys, nonce) != original);
    QVERIFY(Pairing::comparisonNumber(initiator.keys, responder.keys, Pairing::newNonce())
            != original);

    DeviceKeys initiatorOtherVerifyKey = initiator.keys;
    initiatorOtherVerifyKey.verifyKey = DocumentCrypto::generateSigningKeypair().verifyKey;
    QVERIFY(Pairing::comparisonNumber(initiatorOtherVerifyKey, responder.keys, nonce) != original);

    DeviceKeys responderOtherPublicKey = responder.keys;
    responderOtherPublicKey.publicKey = DocumentCrypto::generateKeypair().publicKey;
    QVERIFY(Pairing::comparisonNumber(initiator.keys, responderOtherPublicKey, nonce) != original);

    // The two roles are not interchangeable.
    QVERIFY(Pairing::comparisonNumber(responder.keys, initiator.keys, nonce) != original);
}

void PairingTest::comparisonNumberIsAlwaysSixDigitsInTwoGroups()
{
    const QRegularExpression shape("^[0-9]{3} [0-9]{3}$");

    QSet<QString> seen;
    for (int attempt = 0; attempt < 500; ++attempt)
    {
        const QString number =
            Pairing::comparisonNumber(newDevice().keys, newDevice().keys, Pairing::newNonce());
        QVERIFY2(shape.match(number).hasMatch(), qPrintable(number));
        seen.insert(number);
    }

    // Five hundred draws from a million: a number stuck on a few values would
    // show here long before it showed to users.
    QVERIFY(seen.size() >= 495);
}

// --- Fresh values ------------------------------------------------------------

void PairingTest::twoNoncesDiffer()
{
    const QByteArray first = Pairing::newNonce();
    const QByteArray second = Pairing::newNonce();

    QCOMPARE(first.size(), qsizetype(32));
    QCOMPARE(second.size(), qsizetype(32));
    QVERIFY(first != second);
}

void PairingTest::pairingCodesAreEightDigitsAndDiffer()
{
    const QRegularExpression shape("^[0-9]{8}$");

    QSet<QString> seen;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        const QString code = Pairing::newPairingCode();
        QVERIFY2(shape.match(code).hasMatch(), qPrintable(code));
        seen.insert(code);
    }

    // Fifty draws from a hundred million: even one repeat is rare.
    QVERIFY(seen.size() >= 49);
}

// --- The relay ---------------------------------------------------------------

void PairingTest::aRelaySwappingKeysAfterCommitmentCannotPassTheReveal()
{
    const Device initiator = newDevice();
    const Device responder = newDevice();
    const Device relay = newDevice();
    const QByteArray nonce = Pairing::newNonce();

    // The responder kept the initiator's real commitment.
    const QByteArray kept = Pairing::commit(initiator.keys, nonce);

    // The relay reveals its own keys in place of the initiator's, with the real
    // nonce or one of its own.
    QVERIFY(!Pairing::revealMatches(kept, relay.keys, nonce));
    QVERIFY(!Pairing::revealMatches(kept, relay.keys, Pairing::newNonce()));

    // Had the responder not checked, the two screens would disagree.
    QVERIFY(Pairing::comparisonNumber(relay.keys, responder.keys, nonce)
            != Pairing::comparisonNumber(initiator.keys, responder.keys, nonce));
}

void PairingTest::theResponderKeepsTheFirstCommitmentItReads()
{
    FakeServer server;
    std::optional<SharedDocument> ward = wardOfThree(server);
    QVERIFY(ward.has_value());
    SharedDocument& w = ward.value();

    // Alice's new phone starts a pairing; Alice's laptop responds.
    const TestDevice phone = newDeviceFor(w.alice.member);
    const std::optional<Session> phoneSession = signIn(server, phone);
    QVERIFY(phoneSession.has_value());
    Pairing::Initiator initiator(keysOf(phone));
    const Result<QString> code =
        signedStartPairing(server, phoneSession.value(), "Phone", initiator.commitment());
    QVERIFY(code.ok());

    // A relay serves the real commitment until the responder posts its keys, then
    // a commitment-and-reveal pair of its own, self-consistent by construction.
    const TestDevice relay = newPerson();
    const QByteArray forgedNonce = Pairing::newNonce();
    PairingState before;
    before.code = code.value.value();
    before.commitment = initiator.commitment();
    PairingState after = before;
    after.commitment = Pairing::commit(keysOf(relay), forgedNonce);
    after.initiator = keysOf(relay);
    after.nonce = forgedNonce;
    server.scriptPairingReads(code.value.value(), {before, after});

    // The responder, driven by what the server returns.
    Pairing::Responder responder(keysOf(w.alice));
    const PairingState first = wait<PairingState>([&](Reply<PairingState> reply)
    {
        server.pairing(w.aliceSession, code.value.value(), reply);
    }).value.value();
    QVERIFY(responder.keepCommitment(first.commitment));
    QVERIFY(signedRespondToPairing(server, w.aliceSession, code.value.value(), keysOf(w.alice)).ok());

    const PairingState second = wait<PairingState>([&](Reply<PairingState> reply)
    {
        server.pairing(w.aliceSession, code.value.value(), reply);
    }).value.value();
    QVERIFY(!responder.keepCommitment(second.commitment));
    QVERIFY(!responder.acceptReveal(second.initiator.value(), second.nonce).has_value());

    // A responder that took the commitment from the later read would have been
    // fooled: the ordering, not the maths, is what refuses the relay.
    Pairing::Responder rereading(keysOf(w.alice));
    QVERIFY(rereading.keepCommitment(second.commitment));
    QVERIFY(rereading.acceptReveal(second.initiator.value(), second.nonce).has_value());
}

void PairingTest::aGenuinePairingShowsTheSameNumberOnBothScreens()
{
    const Device phone = newDevice();
    const Device laptop = newDevice();

    Pairing::Initiator initiator(phone.keys);
    Pairing::Responder responder(laptop.keys);

    QVERIFY(responder.keepCommitment(initiator.commitment()));
    QVERIFY(!initiator.reveal().has_value());
    QVERIFY(!initiator.comparisonNumber().has_value());

    QVERIFY(initiator.keepResponder(laptop.keys));
    const std::optional<QByteArray> nonce = initiator.reveal();
    QVERIFY(nonce.has_value());

    const std::optional<QString> shownByLaptop = responder.acceptReveal(phone.keys, nonce.value());
    QVERIFY(shownByLaptop.has_value());
    QCOMPARE(shownByLaptop.value(), initiator.comparisonNumber().value());
}

void PairingTest::theInitiatorKeepsTheFirstResponderKeysItReads()
{
    const Device phone = newDevice();
    const Device laptop = newDevice();
    const Device relay = newDevice();

    Pairing::Initiator initiator(phone.keys);
    QVERIFY(initiator.keepResponder(laptop.keys));
    const QString number = initiator.comparisonNumber().value();

    // Keys offered afterwards change nothing the initiator shows.
    QVERIFY(!initiator.keepResponder(relay.keys));
    QCOMPARE(initiator.comparisonNumber().value(), number);
    QCOMPARE(number, Pairing::comparisonNumber(phone.keys, laptop.keys, initiator.reveal().value()));
}

void PairingTest::commitRefusesInvalidKeysAndAWrongSizedNonce()
{
    const Device initiator = newDevice();

    QVERIFY(Pairing::commit(initiator.keys, QByteArray()).isEmpty());
    QVERIFY(Pairing::commit(initiator.keys, QByteArray(Pairing::NonceSize - 1, 'x')).isEmpty());
    QVERIFY(Pairing::commit(DeviceKeys(), Pairing::newNonce()).isEmpty());

    // An empty commitment matches nothing, even an equally empty expectation.
    QVERIFY(!Pairing::revealMatches(QByteArray(), initiator.keys, QByteArray()));
}

// --- Handover ----------------------------------------------------------------

void PairingTest::handoverVerifiesFromThePairedResponder()
{
    const Device responder = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const Pairing::Handover handover = signedHandover(responder, nonce);

    QCOMPARE(handover.signature.size(), qsizetype(DocumentCrypto::SignatureSize));
    QVERIFY(Pairing::handoverVerifies(handover, nonce, responder.keys.verifyKey));
}

void PairingTest::handoverRefusesAnotherKeyOrAnotherPairing()
{
    const Device responder = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const Pairing::Handover handover = signedHandover(responder, nonce);
    QVERIFY(Pairing::handoverVerifies(handover, nonce, responder.keys.verifyKey));

    QVERIFY(!Pairing::handoverVerifies(handover, nonce, newDevice().keys.verifyKey));
    QVERIFY(!Pairing::handoverVerifies(handover, Pairing::newNonce(), responder.keys.verifyKey));
}

void PairingTest::handoverRefusesChangedContents()
{
    const Device responder = newDevice();
    const QByteArray nonce = Pairing::newNonce();
    const Pairing::Handover handover = signedHandover(responder, nonce);
    QVERIFY(Pairing::handoverVerifies(handover, nonce, responder.keys.verifyKey));

    Pairing::Handover otherMember = handover;
    otherMember.member = newMember();
    QVERIFY(!Pairing::handoverVerifies(otherMember, nonce, responder.keys.verifyKey));

    Pairing::Handover otherName = handover;
    otherName.displayName = "Mallory";
    QVERIFY(!Pairing::handoverVerifies(otherName, nonce, responder.keys.verifyKey));

    Pairing::Handover extraWard = handover;
    extraWard.documentIds.append("0123456789abcdef0123456789abcdef");
    QVERIFY(!Pairing::handoverVerifies(extraWard, nonce, responder.keys.verifyKey));

    Pairing::Handover fewerWards = handover;
    fewerWards.documentIds.removeLast();
    QVERIFY(!Pairing::handoverVerifies(fewerWards, nonce, responder.keys.verifyKey));

    Pairing::Handover reordered = handover;
    std::swap(reordered.documentIds[0], reordered.documentIds[1]);
    QVERIFY(!Pairing::handoverVerifies(reordered, nonce, responder.keys.verifyKey));
}
