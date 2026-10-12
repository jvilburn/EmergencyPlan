#pragma once

#include <QObject>

/// Section 6.4: the file on disk.
class DocumentFileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aRoundTripWithTwoRecipientsOpensForEither();
    void aThirdKeypairIsRefused();
    void aFlippedPayloadByteIsRefused();
    void theDocumentIdSurvivesARoundTripAndIsAuthenticated();
    void aSealedKeyMovedToAnotherEntryIsRefused();
    void sealingAdvancesTheSaveCounterByOne();
    void twoConsecutiveSealsUseDifferentNonces();
    void twoSealsFromTheSamePreviousHeaderUseDifferentNonces();
    void newlinesAndCarriageReturnsSurviveWriteAndRead();
    void encodeWritesExactlyOneNewlineBeforeTheCiphertext();
    void decodeReportsALaterFormatAndMalformedBytes();
};
