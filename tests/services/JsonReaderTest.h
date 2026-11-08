#pragma once

#include <QObject>

class JsonReaderTest : public QObject
{
    Q_OBJECT

public:
    explicit JsonReaderTest(QObject* parent);

private slots:
    void everyFieldTakenByItsTypeReads();
    void requiredFieldThatIsMissingFails();
    void fieldOfTheWrongTypeFails();
    void fieldLeftUntakenFailsAtFinish();
    void optionalFieldReadsItsAbsenceAsEmptyAndRefusesEmptyWrittenOut();
    void firstFailureIsKeptWithItsPath();
    void bytesAreUnpaddedBase64UrlOfTheirExactLength_data();
    void bytesAreUnpaddedBase64UrlOfTheirExactLength();
};
