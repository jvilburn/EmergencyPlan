#pragma once

#include <QObject>

/// Proves the harness builds, runs, and reports a failure.
class SmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void harnessRuns();
};
