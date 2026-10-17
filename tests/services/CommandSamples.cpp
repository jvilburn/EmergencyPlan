#include "CommandSamples.h"
#include "FamilyCommands.h"
#include "TestWard.h"

namespace
{

/// The Smith family with every field of Bob changed, Alice removed, Dan added and the
/// address changed, so the sample round-trips every value type a family edit holds.
Family smithWithEveryFieldChanged()
{
    Person bob = TestWard::bob();
    bob.setName(Name("Smith", "Robert"));
    bob.setGender(std::nullopt);
    bob.setPhone(Phone("555-2222"));
    bob.setAltPhone(Phone("555-2223"));
    bob.setEmail("robert@example.com");
    bob.setBirthday(Birthday::create(1980, 7, 4));
    bob.setCallings({"Clerk"});
    bob.setIsParent(false);
    bob.setWardUnitNumber("123456");
    bob.setStakeUnitNumber("654321");
    bob.setSpecialNeedNote("Uses a wheelchair; the ramp is at the back door.");

    Person dan = Person::createWithId(PersonId::fromString("person-dan"), Name("Smith", "Dan"),
                                      Gender::Male, Birthday(), Phone(), Phone(), QString(),
                                      QStringList(), false, QString(), QString());

    Family saved = TestWard::smith();
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));
    saved.setMembers({bob, dan});
    return saved;
}

std::vector<CommandSample> updateFamilySamples()
{
    Family opened = TestWard::smith();
    Family saved = smithWithEveryFieldChanged();
    Family moved = TestWard::withPhone(opened, TestWard::bobId(), "555-9999");

    std::vector<CommandSample> samples;
    samples.push_back(CommandSample{SampleCase::StartHolds,
                                    TestWard::document(),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.push_back(CommandSample{SampleCase::ResultHolds,
                                    TestWard::withFamily(TestWard::document(), saved),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    samples.push_back(CommandSample{SampleCase::Conflicts,
                                    TestWard::withFamily(TestWard::document(), moved),
                                    UpdateFamilyCommand::fromEdit(opened, saved)});
    return samples;
}

}  // namespace

std::vector<CommandSample> CommandSamples::forType(const QString& type)
{
    if (type == UpdateFamilyCommand::TypeName)
    {
        return updateFamilySamples();
    }
    return {};
}
