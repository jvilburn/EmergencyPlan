#include "CommandSamples.h"
#include "FamilyCommands.h"
#include "TestWard.h"

namespace
{

QList<CommandSample> updateFamilySamples()
{
    Family opened = TestWard::smith();
    Family saved = TestWard::smithWithEveryFieldChanged();
    Family moved = TestWard::withPhone(opened, TestWard::bobId(), "555-9999");

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds,
                                 TestWard::document(),
                                 UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.append(CommandSample{SampleCase::ResultHolds,
                                 TestWard::withFamily(TestWard::document(), saved),
                                 UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.append(CommandSample{SampleCase::Conflicts,
                                 TestWard::withFamily(TestWard::document(), moved),
                                 UpdateFamilyCommand::fromEdit(opened, saved)});
    return samples;
}

}  // namespace

QList<CommandSample> CommandSamples::forType(const QString& type)
{
    if (type == UpdateFamilyCommand::TypeName)
    {
        return updateFamilySamples();
    }
    return {};
}
