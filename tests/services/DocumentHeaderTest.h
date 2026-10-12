#pragma once

#include <QObject>

class DocumentHeaderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void roundTripsThroughJson();
    void roundTripKeepsRecipientOrder();
    void omitsOriginDocumentIdWhenAbsent();

    void parseRefusesMissingOrIllFormedFields();
    void parseRefusesAWrongSizeNonce();
    void parseRefusesAWrongSizeSealedKey();
    void parseRefusesAMalformedFingerprint();
    void parseRefusesANonObjectRecipient();
    void parseRefusesASaveCounterAboveTheCap();
    void parseRefusesAnUnknownKey();
    void parseRefusesAHeaderListingNoOne();
    void parseRefusesARepeatedFingerprint();
    void parseRefusesAnEarlierFormatVersion();
    void parseNeedsNewerVersionForALaterFormat();

    void associatedDataDiffersWhenAnyFieldDiffers();
    void associatedDataDiffersOnRecipientOrder();
    void associatedDataSurvivesAJsonRoundTrip();

    void entryForFindsOnlyItsOwnFingerprint();
};
