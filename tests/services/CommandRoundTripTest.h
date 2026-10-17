#pragma once

#include <QObject>

/// Every registered command type survives the journal's round trip, judged by behaviour.
class CommandRoundTripTest : public QObject
{
    Q_OBJECT

public:
    explicit CommandRoundTripTest(QObject* parent);

private slots:
    void everyRegisteredTypeHasSamples();
    void sampleRebuiltFromItsEntryBehavesAsTheOriginal_data();
    void sampleRebuiltFromItsEntryBehavesAsTheOriginal();
    void runningACommandDoesNotChangeIt();
};
