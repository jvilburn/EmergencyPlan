#pragma once

#include <QObject>

/// Section 1.11: device pairing by comparing a six-digit number.
class PairingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Commitment
    void revealMatchesTheCommittedKeysAndNonce();
    void revealRefusesAChangedPublicKey();
    void revealRefusesAChangedVerifyKey();
    void revealRefusesAChangedNonce();
    void revealRefusesAMalformedCommitment();

    // Comparison number
    void comparisonNumberIsTheSameOnBothSides();
    void comparisonNumberDiffersWhenEitherSidesKeysChange();
    void comparisonNumberIsAlwaysSixDigitsInTwoGroups();

    // Fresh values
    void twoNoncesDiffer();
    void pairingCodesAreEightDigitsAndDiffer();

    // The relay
    void aRelaySwappingKeysAfterCommitmentCannotPassTheReveal();
    void theResponderKeepsTheFirstCommitmentItReads();
    void aGenuinePairingShowsTheSameNumberOnBothScreens();
    void theInitiatorKeepsTheFirstResponderKeysItReads();
    void commitRefusesInvalidKeysAndAWrongSizedNonce();

    // Handover
    void handoverVerifiesFromThePairedResponder();
    void handoverRefusesAnotherKeyOrAnotherPairing();
    void handoverRefusesChangedContents();
};
