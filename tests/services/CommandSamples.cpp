#include "CommandSamples.h"
#include "AddressCoordinatesCommands.h"
#include "EmergencyAssetCommands.h"
#include "FamilyCommands.h"
#include "TeamCommands.h"
#include "TestWard.h"

namespace
{

/// The test ward with Alice removed from the Smith family.
Document withoutAlice()
{
    Family smith = TestWard::smith();
    smith.setMembers({TestWard::bob()});
    return TestWard::withFamily(TestWard::document(), smith);
}

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

/// The conflict is Alice gone, since an add needs every person it names.
QList<CommandSample> addTeamSamples()
{
    Team team = TestWard::setupTeam();
    CommandPtr add = AddTeamCommand::fromNew(team, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::document(), add});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::withTeam(TestWard::document(), team), add});
    samples.append(CommandSample{SampleCase::Conflicts, withoutAlice(), add});
    return samples;
}

QList<CommandSample> updateTeamSamples()
{
    Team opened = TestWard::setupTeam();
    Team saved = opened;
    saved.setName("Cleanup");
    saved.setMemberIds({TestWard::bobId(), TestWard::carolId()});
    saved.setLeaderId(TestWard::bobId());
    Team renamed = opened;
    renamed.setName("Setup and Cleanup");
    CommandPtr edit = UpdateTeamCommand::fromEdit(opened, saved, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::withTeam(TestWard::document(), opened), edit});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::withTeam(TestWard::document(), saved), edit});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withTeam(TestWard::document(), renamed), edit});
    return samples;
}

QList<CommandSample> deleteTeamSamples()
{
    Team seen = TestWard::setupTeam();
    Team grown = seen;
    grown.setMemberIds({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});
    CommandPtr remove = DeleteTeamCommand::fromSeen(seen, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::withTeam(TestWard::document(), seen), remove});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::document(), remove});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withTeam(TestWard::document(), grown), remove});
    return samples;
}

QList<CommandSample> addEmergencyAssetSamples()
{
    EmergencyAsset asset = TestWard::generator();
    CommandPtr add = AddEmergencyAssetCommand::fromNew(asset, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::document(), add});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::withAsset(TestWard::document(), asset), add});
    samples.append(CommandSample{SampleCase::Conflicts, withoutAlice(), add});
    return samples;
}

QList<CommandSample> updateEmergencyAssetSamples()
{
    EmergencyAsset opened = TestWard::generator();
    EmergencyAsset saved = opened;
    saved.setName("Generator, 5 kW");
    saved.setPersonIds({TestWard::bobId(), TestWard::carolId()});
    EmergencyAsset renamed = opened;
    renamed.setName("Portable generator");
    CommandPtr edit = UpdateEmergencyAssetCommand::fromEdit(opened, saved, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::withAsset(TestWard::document(), opened), edit});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::withAsset(TestWard::document(), saved), edit});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withAsset(TestWard::document(), renamed), edit});
    return samples;
}

QList<CommandSample> deleteEmergencyAssetSamples()
{
    EmergencyAsset seen = TestWard::generator();
    EmergencyAsset grown = seen;
    grown.setPersonIds({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});
    CommandPtr remove = DeleteEmergencyAssetCommand::fromSeen(seen, TestWard::document());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::withAsset(TestWard::document(), seen), remove});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::document(), remove});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withAsset(TestWard::document(), grown), remove});
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
    if (type == AddTeamCommand::TypeName)
    {
        return addTeamSamples();
    }
    if (type == UpdateTeamCommand::TypeName)
    {
        return updateTeamSamples();
    }
    if (type == DeleteTeamCommand::TypeName)
    {
        return deleteTeamSamples();
    }
    if (type == AddEmergencyAssetCommand::TypeName)
    {
        return addEmergencyAssetSamples();
    }
    if (type == UpdateEmergencyAssetCommand::TypeName)
    {
        return updateEmergencyAssetSamples();
    }
    if (type == DeleteEmergencyAssetCommand::TypeName)
    {
        return deleteEmergencyAssetSamples();
    }
    return {};
}
