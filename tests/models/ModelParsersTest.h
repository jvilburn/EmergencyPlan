#pragma once

#include <QObject>

/// Each model's strict reader against exactly what its writer produces.
///
/// Every type gets the same checks: the full sample reads and writes back as the
/// same value with the same fields; each field removed in turn is refused when
/// required and accepted when optional; each field given the wrong JSON type is
/// refused; a field the writer never writes is refused. The slots after those pin
/// the value-level rules one type at a time.
class ModelParsersTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Every field of every type
    void birthday();
    void person();
    void family();
    void team();
    void tag();
    void emergencyAsset();
    void ministeringDistrict();
    void ministeringGroup();
    void ward();
    void stake();
    void taskNotification();
    void contactAttempt();
    void responseTask();
    void familyResponseRecord();
    void emergencyResponse();

    // Values in exactly the writer's form
    void enumsAreReadInTheWritersSpellingOnly();
    void timesMustCarryTheirMillisecondsInUtc();
    void optionalFieldsAreNeverPresentButEmpty();
    void idSetsRefuseRepeats();
    void aPersonsFlagsAndBirthdayAreAsWritten();
    void anAddressLineIsAsTheAddressKeepsIt();
    void aTeamColourIsTheNameQColorWrites();
    void aTaskAssignmentHasExactlyOneAssignee();
    void aCollectionRefusesARepeatedId();
    void timesSurviveTheRoundTripExactly();
};
