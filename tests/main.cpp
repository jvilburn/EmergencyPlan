#include "FieldChangeTest.h"
#include "CommandRoundTripTest.h"
#include "PersonJsonTest.h"
#include "UpdateFamilyCommandTest.h"
#include "WardSessionTest.h"

#include <QApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    PersonJsonTest personJsonTest(nullptr);
    FieldChangeTest changeTest(nullptr);
    UpdateFamilyCommandTest updateFamilyCommandTest(nullptr);
    CommandRoundTripTest commandRoundTripTest(nullptr);
    WardSessionTest wardSessionTest(nullptr);

    int failures = 0;
    failures += QTest::qExec(&personJsonTest, argc, argv);
    failures += QTest::qExec(&changeTest, argc, argv);
    failures += QTest::qExec(&updateFamilyCommandTest, argc, argv);
    failures += QTest::qExec(&commandRoundTripTest, argc, argv);
    failures += QTest::qExec(&wardSessionTest, argc, argv);
    return failures;
}
