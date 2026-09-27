#pragma once

#include <QObject>

/// Section 1.5: payload encryption, header signing, and the membership chain.
///
/// Separate from DocumentCryptoTest, which covers the primitives, because these
/// tests need whole signed headers rather than keys and messages.
class DocumentChainTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Payload
    void payloadRoundTrips();
    void tamperedHeaderFailsDecryption();
    void truncatedPayloadFailsDecryption();
    void wrongKeyFailsDecryption();
    void everyNonceIsFresh();

    // Header signing
    void signatureVerifiesWithWriterKey();
    void signatureFailsOnChangedHeaderField();
    void signatureFailsOnChangedPayloadHash();
    void signatureFailsWithAnotherMembersKey();
    void writerIsAccountedForByTheRosterOrAsALeave();

    // Links: writer and counters
    void linkWithWriterNotInPreviousIsBroken();
    void linkWithBadSignatureIsBroken();
    void linkWithHeaderSignedUnderAuthTagIsBroken();
    void linkSkippingAKeyGenerationIsBroken();
    void linkWithDecreasingGenerationIsBroken();
    void linkWithNonIncreasingSaveCounterIsBroken();
    void linkBeyondTheSaveCounterCapIsBroken();
    void linkChangingOriginOrFormatIsBroken();
    void linkAcrossDocumentsIsBroken();

    // Links: device identity
    void linkWithFingerprintNotMatchingKeysIsBroken();
    void linkWithDeviceUnderTwoPeopleIsBroken();
    void linkWithDuplicateVerifyKeyIsBroken();
    void linkMovingDeviceBetweenPeopleIsBroken();
    void linkChangingPublicKeyOfExistingDeviceIsBroken();
    void linkChangingVerifyKeyOfExistingDeviceIsBroken();

    // Links: the admin list
    void linkWithAdminNotAmongThePeopleIsBroken();
    void linkWithNonCanonicalAdminListIsBroken();
    void linkLeavingPeopleWithNoAdminIsBroken();

    // Links: roles
    void linkAdminPromotingAMemberHolds();
    void linkAdminSteppingDownWhenAnotherRemainsHolds();
    void linkMemberChangingTheAdminListIsBroken();
    void linkChangingARoleWithRotationIsBroken();

    // Links: adding and unlinking devices
    void linkAddingDeviceToOwnPersonHolds();
    void linkAddingDeviceToAnotherPersonIsBroken();
    void linkAdminAddingAWhollyNewPersonHolds();
    void linkMemberAddingAWhollyNewPersonIsBroken();
    void linkUnlinkingOwnDeviceWithRotationHolds();
    void linkAdminUnlinkingAnotherPersonsDeviceHolds();
    void linkMemberUnlinkingAnotherPersonsDeviceIsBroken();
    void linkUnlinkingWithoutRotationIsBroken();

    // Links: removing a person
    void linkAdminRemovingAPersonHolds();
    void linkAdminRemovingAnotherAdminHolds();
    void linkMemberRemovingAPersonIsBroken();
    void linkUsingARoleInTheVersionThatGrantsItIsBroken();
    void linkRemovingWithoutRotationIsBroken();

    // Links: leaving
    void linkMemberLeavingHolds();
    void linkLeaveFromEitherDeviceTakesThemAll();
    void linkLeaveThatRotatesIsBroken();
    void linkLeaveThatAlsoChangesOthersIsBroken();
    void linkAdminLeavingWhenAnotherRemainsHolds();
    void linkLastAdminLeavingOthersBehindIsBroken();
    void linkLastPersonLeavingHolds();
    void linkAdminLeaveThatRotatesIsBroken();
    void linkLeavingSomeoneElseIsBroken();
    void linkAfterAnEmptyHeaderIsBroken();
    void linkLeaveSignedByAKeyNotInPreviousIsBroken();

    // Chains
    void chainStillListingSelfIsStillMember();
    void chainProvesRemovalByAdminPromotedAfterAnchor();
    void chainProvesRemovalForEveryDeviceOfTheRemovedPerson();
    void chainProvesUnlinkForTheUnlinkedDevice();
    void chainSkippingHeadersWithUnchangedMembershipVerifies();
    void chainWithWithheldGrantHeaderIsBroken();
    void chainWithWithheldPromotionHeaderIsBroken();
    void chainProvesALeaveToTheLeaversOtherDevices();
    void chainWithBadSignatureIsBroken();
    void chainWithAnchorNotListingSelfIsBroken();
    void chainWithEmptyListIsBroken();
};
