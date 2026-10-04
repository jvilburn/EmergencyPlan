#pragma once

#include <QObject>

/// Task 8: the one settings file.
class AppSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aMissingFileIsAnEmptyStore();
    void everyValueReadsBackThroughASecondInstance();
    void numbersRoundTripAsNumbers();
    void recordsForTwoIdsStaySeparate();
    void aRecordWithPendingRosterChangesRoundTrips();
    void removingARecordForgetsTheWholeDocument();
    void aMalformedFileIsReportedAndLeftAlone();
};
