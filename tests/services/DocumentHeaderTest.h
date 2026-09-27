#pragma once

#include <QObject>

class DocumentHeaderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void roundTripsThroughJson();
    void roundTripKeepsRecipientAndAdminOrder();
    void omitsOriginDocumentIdWhenAbsent();
    void isAdminFindsOnlyListedMembers();

    void parseRefusesMissingOrIllFormedFields();
    void parseRefusesAWrongSizeNonce();
    void parseRefusesAWrongSizeSignature();
    void parseRefusesAWrongSizePayloadHash();
    void parseRefusesAWrongSizeRecipientVerifyKey();
    void parseRefusesAMalformedFingerprint();
    void parseRefusesANonObjectRecipient();
    void parseRefusesASaveCounterAboveTheCap();
    void parseRefusesAnUnknownKey();
    void parseAcceptsAHeaderListingNoOne();
    void parseRefusesAnEarlierFormatVersion();
    void parseRefusesANonCanonicalTimestamp();
    void parseRefusesNonCanonicalAdmins();
    void parseRefusesWriterKeysThatAreNotTheLastWriters();

    void parseNeedsNewerVersionForALaterFormat();

    void associatedDataDiffersWhenAnyCoveredFieldDiffers();
    void associatedDataIgnoresPayloadHashAndSignature();
    void associatedDataDiffersOnRecipientOrder();
    void associatedDataSurvivesAJsonRoundTrip();

    void signedBytesDiffersWhenPayloadHashDiffers();

    void entryForFindsOnlyItsOwnFingerprint();
};
