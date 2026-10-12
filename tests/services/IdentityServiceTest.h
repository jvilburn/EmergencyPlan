#pragma once

#include <QObject>

/// This device's identity: its keys from the first run, its person once created.
class IdentityServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aFirstRunCreatesKeysAndNoPerson();
    void aFirstRunThatCannotStoreFails();
    void theKeysSurviveReopening();
    void createPersonGivesAFreshMemberId();
    void thePersonAndNamesSurviveAReload();
    void aPersonIsSetOnce();
    void namesAreRequiredAndARefusalWritesNothing();
    void aPersonThatCannotBeStoredIsNotTaken();
    void aDamagedFileIsReportedAndNotReplaced();
    void aNewerVersionsFileIsReportedAndNotReplaced();
    void startingOverKeepsTheOldFileAndMakesANewDevice();
};
