#pragma once

#include <QObject>

/// Section 1.9: the decisions that keep a local file safe when the server moves.
class ReconcileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Accept
    void aValidNewerVersionIsAccepted();
    void aValidNewerVersionIsAcceptedWithNoBaseHeader();
    void aVersionWrittenBySomeoneWhoJustLeftIsAccepted();

    // Each failure is a ServerFault with nothing else set
    void anUnsealableEntryIsAServerFault();
    void aPayloadHashMismatchIsAServerFault();
    void aBadSignatureIsAServerFault();
    void aBrokenChainIsAServerFault();
    void aWithheldHeaderIsAServerFault();
    void aSubstitutedHeaderIsAServerFault();
    void aVersionBelowTheHighWaterMarkIsAServerFault();
    void anEntryWhoseKeysDoNotMatchItsFingerprintIsAServerFault();
    void aWriterListedButMissingFromTheRosterIsAServerFault();
    void aVersionOfAnotherDocumentIsAServerFault();
    void aHeaderDisagreeingWithItsRosterIsAServerFault();
    void anEmptyHeaderListIsAServerFault();

    // Removal
    void aChainProvenRemovalIsRemoved();
    void aRemovalByAnAdminPromotedAfterTheBaseIsRemoved();
    void theLeaversOtherDeviceIsRemovedByTheLeave();
    void withNoBaseHeaderAMissingEntryIsNeverRemoved();
    void checkRemovalReachesTheSameVerdictsFromHeadersAlone();

    // Newer versions
    void aNewerHeaderFormatAnywhereIsNeedsNewerVersion();
    void anUnknownRecipientKindAnywhereIsNeedsNewerVersion();
    void aMalformedHeaderIsAServerFault();
    void aNewerPayloadVersionIsNeedsNewerVersion();
};
