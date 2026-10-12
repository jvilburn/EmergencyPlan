#pragma once

#include <QObject>

/// The one settings file.
class AppSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aMissingFileIsAnEmptyStore();
    void everyValueReadsBackThroughASecondInstance();
    void numbersRoundTripAsNumbers();
    void recordsForTwoIdsStaySeparate();
    void removingARecordLeavesTheOthers();
    void wardEntriesReadBackThroughASecondInstance();
    void removingAWardLeavesItsRecordAndClearsTheLastWard();
    void aMalformedFileIsReportedAndLeftAlone_data();
    void aMalformedFileIsReportedAndLeftAlone();
    void aFailedWriteKeepsTheValuesAndSaysSo();
};
