#include "CommandRoundTripTest.h"
#include "CommandRegistry.h"
#include "CommandSamples.h"
#include "FamilyCommands.h"
#include "TestWard.h"

#include <QSet>
#include <QTest>

CommandRoundTripTest::CommandRoundTripTest(QObject* parent)
    : QObject(parent)
{
}

void CommandRoundTripTest::everyRegisteredTypeHasSamples()
{
    QVERIFY(!CommandRegistry::types().isEmpty());
    QSet<QString> names;
    for (const CommandType& type : CommandRegistry::types())
    {
        QVERIFY2(!names.contains(type.name), qPrintable(type.name));
        names.insert(type.name);

        std::vector<CommandSample> samples = CommandSamples::forType(type.name);
        QVERIFY2(!samples.empty(), qPrintable(type.name));
        bool hasStartHolds = false;
        for (const CommandSample& sample : samples)
        {
            QCOMPARE(sample.command->type(), type.name);
            hasStartHolds = hasStartHolds || sample.sampleCase == SampleCase::StartHolds;
        }
        QVERIFY2(hasStartHolds, qPrintable(type.name));
    }
    QVERIFY(names.contains(UpdateFamilyCommand::TypeName));
}

void CommandRoundTripTest::sampleRebuiltFromItsEntryBehavesAsTheOriginal_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<int>("index");
    for (const CommandType& type : CommandRegistry::types())
    {
        std::vector<CommandSample> samples = CommandSamples::forType(type.name);
        for (int i = 0; i < static_cast<int>(samples.size()); ++i)
        {
            QTest::newRow(qPrintable(QString("%1 #%2").arg(type.name).arg(i))) << type.name << i;
        }
    }
}

void CommandRoundTripTest::sampleRebuiltFromItsEntryBehavesAsTheOriginal()
{
    QFETCH(QString, type);
    QFETCH(int, index);
    std::vector<CommandSample> samples = CommandSamples::forType(type);
    const CommandSample& sample = samples[index];

    QJsonObject entry = CommandRegistry::toJson(*sample.command);
    CommandPtr rebuilt = CommandRegistry::fromJson(entry);
    QVERIFY(rebuilt != nullptr);
    QCOMPARE(rebuilt->type(), type);
    QVERIFY(CommandRegistry::toJson(*rebuilt) == entry);
    QVERIFY(rebuilt->description() == sample.command->description());

    Document byOriginal = sample.document;
    Document byRebuilt = sample.document;
    Outcome original = sample.command->execute(byOriginal);
    Outcome again = rebuilt->execute(byRebuilt);

    Outcome expected = sample.sampleCase == SampleCase::Conflicts ? Outcome::Conflicted : Outcome::Applied;
    QVERIFY(original == expected);
    QVERIFY(again == expected);
    QVERIFY(byOriginal == byRebuilt);
    if (sample.sampleCase == SampleCase::StartHolds)
    {
        QVERIFY(byOriginal != sample.document);
    }
    else
    {
        QVERIFY(byOriginal == sample.document);
    }
}

void CommandRoundTripTest::runningACommandDoesNotChangeIt()
{
    Family opened = TestWard::smith();
    std::unique_ptr<UpdateFamilyCommand> command =
        UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), "555-2222"));
    QJsonObject before = CommandRegistry::toJson(*command);

    Document applied = TestWard::document();
    QVERIFY(command->execute(applied) == Outcome::Applied);
    QCOMPARE(QString(TestWard::person(applied, TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(CommandRegistry::toJson(*command) == before);

    Document moved = TestWard::withFamily(TestWard::document(),
                                          TestWard::withPhone(opened, TestWard::bobId(), "555-9999"));
    Document movedBefore = moved;
    QVERIFY(command->execute(moved) == Outcome::Conflicted);
    QVERIFY(moved == movedBefore);
}
