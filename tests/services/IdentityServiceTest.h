#pragma once

#include <QObject>

/// This device's identity: its keys from the first run, its person once created
/// or joined.
class IdentityServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aFirstRunCreatesKeysAndNoPerson();
    void theKeysSurviveReopening();
    void createPersonGivesAFreshMemberId();
    void joinPersonKeepsTheGivenMemberId();
    void thePersonAndNamesSurviveAReload();
    void aPersonIsSetOnce();
    void namesAndAMemberIdAreRequired();
    void aDamagedFileIsReportedAndNotReplaced();
    void theDefaultDeviceNameIsTheHostName();
};
