#pragma once

#include <QObject>

/// The release check: what counts as newer, what stays quiet, and how often it asks.
class UpdateCheckerTest : public QObject
{
    Q_OBJECT

private slots:
    void aNewerReleaseIsReported();
    void anEqualOrOlderReleaseIsNot();
    void versionsCompareNumerically();
    void aFailedOrMalformedAnswerIsSilent();
    void aCheckWithinADayIsSkipped();
    void notNowKeepsThatVersionQuietUntilANewerOne();
};
