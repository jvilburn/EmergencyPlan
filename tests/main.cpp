#include "AutomaticUpdatesTest.h"
#include "BirthdayTest.h"
#include "FamilyEditPanelTest.h"
#include "FamilyOrderTest.h"
#include "FieldChangeTest.h"
#include "FilterTest.h"
#include "ImportCommandsTest.h"
#include "CommandRoundTripTest.h"
#include "EmergencyAssetCommandsTest.h"
#include "EmergencyCommandsTest.h"
#include "MemberEditorTest.h"
#include "PersonJsonTest.h"
#include "ReferenceCleanupTest.h"
#include "TeamCommandsTest.h"
#include "UpdateFamilyCommandTest.h"
#include "WardSessionTest.h"

#include <QApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    PersonJsonTest personJsonTest(nullptr);
    BirthdayTest birthdayTest(nullptr);
    FamilyOrderTest familyOrderTest(nullptr);
    FieldChangeTest changeTest(nullptr);
    UpdateFamilyCommandTest updateFamilyCommandTest(nullptr);
    CommandRoundTripTest commandRoundTripTest(nullptr);
    WardSessionTest wardSessionTest(nullptr);
    AutomaticUpdatesTest automaticUpdatesTest(nullptr);
    TeamCommandsTest teamCommandsTest(nullptr);
    EmergencyAssetCommandsTest emergencyAssetCommandsTest(nullptr);
    EmergencyCommandsTest emergencyCommandsTest(nullptr);
    ImportCommandsTest importCommandsTest(nullptr);
    ReferenceCleanupTest referenceCleanupTest(nullptr);
    FilterTest filterTest(nullptr);
    MemberEditorTest memberEditorTest(nullptr);
    FamilyEditPanelTest familyEditPanelTest(nullptr);

    int failures = 0;
    failures += QTest::qExec(&personJsonTest, argc, argv);
    failures += QTest::qExec(&birthdayTest, argc, argv);
    failures += QTest::qExec(&familyOrderTest, argc, argv);
    failures += QTest::qExec(&changeTest, argc, argv);
    failures += QTest::qExec(&updateFamilyCommandTest, argc, argv);
    failures += QTest::qExec(&commandRoundTripTest, argc, argv);
    failures += QTest::qExec(&wardSessionTest, argc, argv);
    failures += QTest::qExec(&automaticUpdatesTest, argc, argv);
    failures += QTest::qExec(&teamCommandsTest, argc, argv);
    failures += QTest::qExec(&emergencyAssetCommandsTest, argc, argv);
    failures += QTest::qExec(&emergencyCommandsTest, argc, argv);
    failures += QTest::qExec(&importCommandsTest, argc, argv);
    failures += QTest::qExec(&referenceCleanupTest, argc, argv);
    failures += QTest::qExec(&filterTest, argc, argv);
    failures += QTest::qExec(&memberEditorTest, argc, argv);
    failures += QTest::qExec(&familyEditPanelTest, argc, argv);
    return failures;
}
