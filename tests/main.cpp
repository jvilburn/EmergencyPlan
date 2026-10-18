#include "BirthdayTest.h"
#include "FamilyEditPanelTest.h"
#include "FamilyOrderTest.h"
#include "FieldChangeTest.h"
#include "CommandRoundTripTest.h"
#include "MemberEditorTest.h"
#include "PersonJsonTest.h"
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
    failures += QTest::qExec(&memberEditorTest, argc, argv);
    failures += QTest::qExec(&familyEditPanelTest, argc, argv);
    return failures;
}
