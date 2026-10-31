#include "CommandSamples.h"
#include "AddressCoordinatesCommands.h"
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

/// Set never conflicts, so it has no Conflicts sample.
QList<CommandSample> setAddressCoordinatesSamples()
{
    Address address = TestWard::smith().address();
    Coordinates typed(40.2338, -111.6585);
    Document typedThere = TestWard::withCoordinates(TestWard::document(), address, typed);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds,
                                 TestWard::document(),
                                 SetAddressCoordinatesCommand::fromTyped(TestWard::document(), address.full(), typed)});
    samples.append(CommandSample{SampleCase::ResultHolds,
                                 typedThere,
                                 SetAddressCoordinatesCommand::fromTyped(TestWard::document(), address.full(), typed)});
    return samples;
}

QList<CommandSample> revertAddressCoordinatesSamples()
{
    Address address = TestWard::smith().address();
    Coordinates geocoded(40.2330, -111.6580);
    Coordinates typed(40.2338, -111.6585);
    Coordinates other(40.2400, -111.6600);
    Document geocodedThere = TestWard::withCoordinates(TestWard::document(), address, geocoded);
    CommandPtr revert = SetAddressCoordinatesCommand::fromTyped(geocodedThere, address.full(), typed)->inverse();

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds,
                                 TestWard::withCoordinates(TestWard::document(), address, typed),
                                 revert});
    samples.append(CommandSample{SampleCase::ResultHolds, geocodedThere, revert});
    samples.append(CommandSample{SampleCase::Conflicts,
                                 TestWard::withCoordinates(TestWard::document(), address, other),
                                 revert});
    return samples;
}

}  // namespace

QList<CommandSample> CommandSamples::forType(const QString& type)
{
    if (type == UpdateFamilyCommand::TypeName)
    {
        return updateFamilySamples();
    }
    if (type == SetAddressCoordinatesCommand::TypeName)
    {
        return setAddressCoordinatesSamples();
    }
    if (type == RevertAddressCoordinatesCommand::TypeName)
    {
        return revertAddressCoordinatesSamples();
    }
    return {};
}
