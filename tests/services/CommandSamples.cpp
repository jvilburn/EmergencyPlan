#include "CommandSamples.h"
#include "AccessCommands.h"
#include "AddressCoordinatesCommands.h"
#include "EmergencyAssetCommands.h"
#include "EmergencyCommands.h"
#include "FamilyCommands.h"
#include "Identity.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
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

/// document after command ran on it.
Document runOn(const Document& document, const CommandPtr& command)
{
    Document applied = document;
    command->execute(applied);
    return applied;
}

/// document with a date for each PDF it was last imported from, so an import records both.
Document withPdfDates(Document document)
{
    document.setWardDirectoryPdfDate(QDate(2026, 4, 1));
    document.setMinisteringPdfDate(QDate(2026, 3, 1));
    return document;
}

/// The Smith family with Bob's phone moved to a third value.
Family movedSmith()
{
    return TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-9999");
}

/// The every-field edit, which removes Alice, on the ward where she is in every kind of
/// place, and its inverse, which restores her to them.
QList<CommandSample> updateFamilySamples()
{
    Document ward = TestWard::documentWithPlaces();
    UndoableCommandPtr edit = UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smithWithEveryFieldChanged(), ward);
    CommandPtr undo = edit->inverse();
    Document edited = runOn(ward, edit);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, ward, edit});
    samples.append(CommandSample{SampleCase::ResultHolds, edited, edit});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withFamily(ward, movedSmith()), edit});
    samples.append(CommandSample{SampleCase::StartHolds, edited, undo});
    samples.append(CommandSample{SampleCase::ResultHolds, ward, undo});
    samples.append(CommandSample{SampleCase::Conflicts,
                                 TestWard::withFamily(edited, TestWard::withPhone(TestWard::family(edited, TestWard::smithId()),
                                                                                  TestWard::bobId(), "555-9999")),
                                 undo});
    return samples;
}

/// The Smith family deleted from the ward where its people are in every kind of place.
QList<CommandSample> deleteFamilySamples()
{
    Document ward = TestWard::documentWithPlaces();
    CommandPtr remove = DeleteFamilyCommand::fromSeen(TestWard::smith(), ward);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, ward, remove});
    samples.append(CommandSample{SampleCase::ResultHolds, runOn(ward, remove), remove});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withFamily(ward, movedSmith()), remove});
    return samples;
}

/// Restore never conflicts, so it has no Conflicts sample.
QList<CommandSample> restoreFamilySamples()
{
    Document ward = TestWard::documentWithPlaces();
    UndoableCommandPtr remove = DeleteFamilyCommand::fromSeen(TestWard::smith(), ward);
    CommandPtr restore = remove->inverse();

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, runOn(ward, remove), restore});
    samples.append(CommandSample{SampleCase::ResultHolds, ward, restore});
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
                                 SetAddressCoordinatesCommand::fromTyped(TestWard::document(), address, typed)});
    samples.append(CommandSample{SampleCase::ResultHolds,
                                 typedThere,
                                 SetAddressCoordinatesCommand::fromTyped(TestWard::document(), address, typed)});
    return samples;
}

QList<CommandSample> revertAddressCoordinatesSamples()
{
    Address address = TestWard::smith().address();
    Coordinates geocoded(40.2330, -111.6580);
    Coordinates typed(40.2338, -111.6585);
    Coordinates other(40.2400, -111.6600);
    Document geocodedThere = TestWard::withCoordinates(TestWard::document(), address, geocoded);
    CommandPtr revert = SetAddressCoordinatesCommand::fromTyped(geocodedThere, address, typed)->inverse();

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

/// The conflict is Alice gone, since an add needs every person it names. The team put back
/// by undoing its deletion also assigns it the task the deletion unassigned.
QList<CommandSample> addTeamSamples()
{
    Team team = TestWard::setupTeam();
    CommandPtr add = AddTeamCommand::fromNew(team, TestWard::document());
    Document ward = TestWard::documentWithPlaces();
    UndoableCommandPtr remove = DeleteTeamCommand::fromSeen(ward.findTeamById(TestWard::setupTeamId()).value(), ward);
    CommandPtr putBack = remove->inverse();
    Document removed = runOn(ward, remove);
    Family bobOnly = TestWard::smith();
    bobOnly.setMembers({TestWard::bob()});

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::document(), add});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::withTeam(TestWard::document(), team), add});
    samples.append(CommandSample{SampleCase::Conflicts, withoutAlice(), add});
    samples.append(CommandSample{SampleCase::StartHolds, removed, putBack});
    samples.append(CommandSample{SampleCase::ResultHolds, ward, putBack});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withFamily(removed, bobOnly), putBack});
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

/// The Setup team deleted from the ward where a task is assigned to it.
QList<CommandSample> deleteTeamSamples()
{
    Document ward = TestWard::documentWithPlaces();
    Team seen = ward.findTeamById(TestWard::setupTeamId()).value();
    Team grown = seen;
    grown.setMemberIds({TestWard::bobId(), TestWard::aliceId(), TestWard::carolId()});
    CommandPtr remove = DeleteTeamCommand::fromSeen(seen, ward);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, ward, remove});
    samples.append(CommandSample{SampleCase::ResultHolds, runOn(ward, remove), remove});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withTeam(ward, grown), remove});
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

/// The test ward holding response.
Document withResponse(const EmergencyResponse& response)
{
    return TestWard::withResponse(TestWard::document(), response);
}

/// Bob phoned the Smiths and found them well.
ContactAttempt phonedSmiths()
{
    return TestWard::attempt("attempt-phoned", TestWard::smith(), TestWard::octoberFirst(10, 0), ContactMethod::Phone,
                             TestWard::bob(), ContactOutcome::OK, "All well");
}

ResponseTask floodedBasement()
{
    return TestWard::task("task-basement", TestWard::smith(), TestWard::octoberFirst(10, 30), "Flooding",
                          "Water in the basement");
}

/// Start never conflicts, so it has no Conflicts sample. Its result holds whatever
/// emergency is in progress.
QList<CommandSample> startEmergencySamples()
{
    CommandPtr start = StartEmergencyCommand::fromNew(TestWard::storm());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::document(), start});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::storm()), start});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::flood()), start});
    return samples;
}

/// End conflicts with the emergency in progress changed since the report, or a different one.
QList<CommandSample> endEmergencySamples()
{
    CommandPtr end = EndEmergencyCommand::fromReported(TestWard::storm());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::storm()), end});
    samples.append(CommandSample{SampleCase::ResultHolds, TestWard::document(), end});
    samples.append(CommandSample{SampleCase::Conflicts,
                                 withResponse(TestWard::stormWith({phonedSmiths()}, {})),
                                 end});
    samples.append(CommandSample{SampleCase::Conflicts, withResponse(TestWard::flood()), end});
    return samples;
}

/// Restore's result holds when the emergency in progress equals it, and it conflicts with any
/// other: the same one changed since, or a different one.
QList<CommandSample> restoreEmergencySamples()
{
    CommandPtr restore = EndEmergencyCommand::fromReported(TestWard::storm())->inverse();

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::document(), restore});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::storm()), restore});
    samples.append(CommandSample{SampleCase::Conflicts,
                                 withResponse(TestWard::stormWith({phonedSmiths()}, {})),
                                 restore});
    samples.append(CommandSample{SampleCase::Conflicts, withResponse(TestWard::flood()), restore});
    return samples;
}

QList<CommandSample> addContactAttemptSamples()
{
    CommandPtr add = AddContactAttemptCommand::fromNew(phonedSmiths());

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::storm()), add});
    samples.append(CommandSample{SampleCase::ResultHolds,
                                 withResponse(TestWard::stormWith({phonedSmiths()}, {})),
                                 add});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::document(), add});
    return samples;
}

QList<CommandSample> deleteContactAttemptSamples()
{
    CommandPtr remove = AddContactAttemptCommand::fromNew(phonedSmiths())->inverse();

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds,
                                 withResponse(TestWard::stormWith({phonedSmiths()}, {})),
                                 remove});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::storm()), remove});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::document(), remove});
    return samples;
}

/// The conflicts are the Setup team missing, since an open task's assignee must exist, and
/// no emergency in progress. A resolved task's assignee is part of the log, so adding one
/// assigned to the missing Setup team applies.
QList<CommandSample> addTaskSamples()
{
    ResponseTask task = floodedBasement();
    task.setAssignment(TaskAssignment::toTeam(TestWard::setupTeam()));
    Document withTeam = TestWard::withTeam(TestWard::document(), TestWard::setupTeam());
    CommandPtr add = AddTaskCommand::fromNew(task);
    ResponseTask resolved = task;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    CommandPtr addResolved = AddTaskCommand::fromNew(resolved);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, TestWard::withResponse(withTeam, TestWard::storm()), add});
    samples.append(CommandSample{SampleCase::ResultHolds,
                                 TestWard::withResponse(withTeam, TestWard::stormWith({}, {task})),
                                 add});
    samples.append(CommandSample{SampleCase::Conflicts, withResponse(TestWard::storm()), add});
    samples.append(CommandSample{SampleCase::Conflicts, withTeam, add});
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::storm()), addResolved});
    return samples;
}

QList<CommandSample> updateTaskSamples()
{
    ResponseTask opened = floodedBasement();
    ResponseTask saved = opened;
    saved.setCategory("Shelter");
    saved.setDescription("Water in the basement and the garage");
    saved.setAssignment(TaskAssignment::toPerson(TestWard::carol()));
    saved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    ResponseTask moved = opened;
    moved.setDescription("Water in the garage");
    CommandPtr edit = UpdateTaskCommand::fromEdit(opened, saved);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::stormWith({}, {opened})), edit});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::stormWith({}, {saved})), edit});
    samples.append(CommandSample{SampleCase::Conflicts, withResponse(TestWard::stormWith({}, {moved})), edit});
    return samples;
}

QList<CommandSample> deleteTaskSamples()
{
    ResponseTask seen = floodedBasement();
    ResponseTask resolved = seen;
    resolved.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    CommandPtr remove = DeleteTaskCommand::fromSeen(seen);

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::stormWith({}, {seen})), remove});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(TestWard::storm()), remove});
    samples.append(CommandSample{SampleCase::Conflicts, withResponse(TestWard::stormWith({}, {resolved})), remove});
    return samples;
}

QList<CommandSample> addTaskCategorySamples()
{
    CommandPtr add = AddTaskCategoryCommand::fromNew("Power lines");
    EmergencyResponse withCategory = TestWard::storm();
    withCategory.addTaskCategory("Power lines");

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, withResponse(TestWard::storm()), add});
    samples.append(CommandSample{SampleCase::ResultHolds, withResponse(withCategory), add});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::document(), add});
    return samples;
}

/// A directory import onto the ward where its people are in every kind of place, which holds
/// both PDF dates: Bob's phone changed, the Jones family gone, the Lee family added with the
/// coordinates the PDF prints for its address, the Tanglewood ward record, and a newer PDF
/// date.
QList<CommandSample> importWardDirectorySamples()
{
    Family smith = TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-2222");
    Family lee = TestWard::lee(2010);
    WardDirectoryImportResult result;
    result.success = true;
    result.families.insert(smith.id(), smith);
    result.families.insert(lee.id(), lee);
    result.removedFamilyIds.insert(TestWard::jonesId());
    result.addressCoordinates.insert(lee.address().full(), Coordinates(40.2400, -111.6600));
    result.wardName = "Tanglewood";
    result.wardUnitNumber = "123456";
    result.pdfDate = QDate(2026, 10, 15);
    Document ward = withPdfDates(TestWard::documentWithPlaces());
    CommandPtr import = ImportWardDirectoryCommand::fromImport(ward, result, "Tanglewood.pdf");

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, ward, import});
    samples.append(CommandSample{SampleCase::ResultHolds, runOn(ward, import), import});
    samples.append(CommandSample{SampleCase::Conflicts, TestWard::withFamily(ward, movedSmith()), import});
    return samples;
}

/// A ministering import onto the test ward holding both PDF dates: one group, Bob ministering
/// to the Jones and Lee families, in a district Alice oversees; Carol's birthday filled in, the
/// Lee family added, and a newer PDF date. The conflict is another group in place since the
/// import was worked out.
QList<CommandSample> importMinisteringSamples()
{
    Family jones = TestWard::jones();
    Person carol = TestWard::carol();
    carol.setBirthday(Birthday::create(1970, 3, 14));
    jones = TestWard::withMember(jones, carol);
    Family lee = TestWard::lee(2010);
    MinisteringGroup group = MinisteringGroup::createEQ({TestWard::bobId()}, {TestWard::jonesId(), lee.id()});
    MinisteringDistrict district = MinisteringDistrict::create("District 1", TestWard::aliceId(), {group.id()});

    MinisteringImportResult result;
    result.success = true;
    result.families = TestWard::document().families();
    result.families.insert(jones.id(), jones);
    result.families.insert(lee.id(), lee);
    result.groups.insert(group.id(), group);
    result.districts.insert(district.id(), district);
    result.pdfDate = QDate(2026, 9, 1);
    Document ward = withPdfDates(TestWard::document());
    CommandPtr import = ImportMinisteringCommand::fromImport(ward, result, "Tanglewood EQ.pdf");

    Document regrouped = ward;
    MinisteringGroup other = MinisteringGroup::createEQ({TestWard::carolId()}, {TestWard::smithId()});
    regrouped.setGroups(MinisteringOrg::EldersQuorum, {{other.id(), other}});

    QList<CommandSample> samples;
    samples.append(CommandSample{SampleCase::StartHolds, ward, import});
    samples.append(CommandSample{SampleCase::ResultHolds, runOn(ward, import), import});
    samples.append(CommandSample{SampleCase::Conflicts, regrouped, import});
    return samples;
}

/// The test ward whose access list has Alice, an admin, on her laptop and phone; Carol, a
/// member, on her desktop; and a member invite Alice made for Bob, with the keys behind them.
struct AccessSampleWard
{
    Document ward;
    Identity laptop;
    Identity phone;
    Identity desktop;
};

AccessSampleWard accessSampleWard()
{
    Identity laptop = Identity::generate();
    Identity phone = Identity::generate();
    Identity desktop = Identity::generate();
    Identity bobsInvite = Identity::generate();
    UserId alice = UserId::generate();
    AccessList access({AccessUser(alice, "Alice", AccessRole::Admin,
                                  {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                                   AccessDevice("Phone", phone.publicKey(), phone.verifyKey())}),
                       AccessUser(UserId::generate(), "Carol", AccessRole::Member,
                                  {AccessDevice("Desktop", desktop.publicKey(), desktop.verifyKey())})},
                      {AccessInvite::forUser(alice, AccessRole::Member, "Bob", bobsInvite.publicKey(),
                                             bobsInvite.verifyKey())},
                      {});
    Document ward = TestWard::document();
    ward.setAccess(access);
    return AccessSampleWard{ward, laptop, phone, desktop};
}

/// The user named name in ward's access list.
AccessUser accessUserNamed(const Document& ward, const QString& name)
{
    for (const AccessUser& user : ward.access().users())
    {
        if (user.name() == name)
        {
            return user;
        }
    }
    qFatal("CommandSamples: the ward holds no user named %s", qPrintable(name));
}

/// ward with user in place of the user who has user's id.
Document withAccessUser(const Document& ward, const AccessUser& user)
{
    AccessList access = ward.access();
    access.setUser(user);
    Document changed = ward;
    changed.setAccess(access);
    return changed;
}

/// Invites and removals never conflict, so they have no Conflicts sample.
QList<CommandSample> inviteSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr invite = InviteCommand::create(sample.laptop.fingerprint(), sample.ward.access(), std::nullopt,
                                              Identity::generate());
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, invite},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, invite), invite},
    };
}

QList<CommandSample> removeUserSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr remove = RemoveUserCommand::create(sample.laptop.fingerprint(), accessUserNamed(sample.ward, "Carol"));
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, remove},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, remove), remove},
    };
}

QList<CommandSample> removeDeviceSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr remove = RemoveDeviceCommand::create(
        sample.laptop.fingerprint(), AccessDevice("Phone", sample.phone.publicKey(), sample.phone.verifyKey()));
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, remove},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, remove), remove},
    };
}

QList<CommandSample> withdrawInviteSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr withdraw = WithdrawInviteCommand::create(sample.laptop.fingerprint(),
                                                        sample.ward.access().invites().first(), sample.ward.access());
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, withdraw},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, withdraw), withdraw},
    };
}

/// Carol made an admin; it conflicts only once she is gone.
QList<CommandSample> setRoleSamples()
{
    AccessSampleWard sample = accessSampleWard();
    AccessUser carol = accessUserNamed(sample.ward, "Carol");
    CommandPtr promote = SetRoleCommand::create(sample.laptop.fingerprint(), carol, AccessRole::Admin);
    CommandPtr remove = RemoveUserCommand::create(sample.laptop.fingerprint(), carol);
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, promote},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, promote), promote},
        CommandSample{SampleCase::Conflicts, runOn(sample.ward, remove), promote},
    };
}

QList<CommandSample> renameUserSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr rename = RenameUserCommand::create(sample.laptop.fingerprint(), sample.ward.access(), "Alice Jones");
    AccessUser renamedElsewhere = accessUserNamed(sample.ward, "Alice");
    renamedElsewhere.setName("Alice Brown");
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, rename},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, rename), rename},
        CommandSample{SampleCase::Conflicts, withAccessUser(sample.ward, renamedElsewhere), rename},
    };
}

QList<CommandSample> renameDeviceSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr rename = RenameDeviceCommand::create(sample.laptop.fingerprint(), sample.ward.access(), "Work laptop");
    AccessUser renamedElsewhere = accessUserNamed(sample.ward, "Alice");
    QList<AccessDevice> devices = renamedElsewhere.devices();
    devices[0].setName("Old laptop");
    renamedElsewhere.setDevices(devices);
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, rename},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, rename), rename},
        CommandSample{SampleCase::Conflicts, withAccessUser(sample.ward, renamedElsewhere), rename},
    };
}

/// Bob joining through his user invite, its result holding once his phone is on the list, then
/// a tablet joining through a device invite Carol made. A join never conflicts.
QList<CommandSample> joinSamples()
{
    AccessSampleWard sample = accessSampleWard();
    Identity bobsPhone = Identity::generate();
    CommandPtr userJoin = JoinCommand::create(sample.ward.access().invites().first(),
                                              AccessDevice("Bob's phone", bobsPhone.publicKey(), bobsPhone.verifyKey()),
                                              "Bob Brown", "Tanglewood Ward");

    Identity carolsInvite = Identity::generate();
    Identity carolsTablet = Identity::generate();
    AccessInvite deviceInvite = AccessInvite::forDevice(accessUserNamed(sample.ward, "Carol").id(),
                                                        carolsInvite.publicKey(), carolsInvite.verifyKey());
    AccessList access = sample.ward.access();
    access.addInvite(deviceInvite);
    Document withDeviceInvite = sample.ward;
    withDeviceInvite.setAccess(access);
    CommandPtr deviceJoin = JoinCommand::create(
        deviceInvite, AccessDevice("Carol's tablet", carolsTablet.publicKey(), carolsTablet.verifyKey()), QString(),
        "Tanglewood Ward");
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, userJoin},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, userJoin), userJoin},
        CommandSample{SampleCase::StartHolds, withDeviceInvite, deviceJoin},
    };
}

}  // namespace

QList<CommandSample> CommandSamples::forType(const QString& type)
{
    if (type == UpdateFamilyCommand::TypeName)
    {
        return updateFamilySamples();
    }
    if (type == DeleteFamilyCommand::TypeName)
    {
        return deleteFamilySamples();
    }
    if (type == RestoreFamilyCommand::TypeName)
    {
        return restoreFamilySamples();
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
    if (type == StartEmergencyCommand::TypeName)
    {
        return startEmergencySamples();
    }
    if (type == EndEmergencyCommand::TypeName)
    {
        return endEmergencySamples();
    }
    if (type == RestoreEmergencyCommand::TypeName)
    {
        return restoreEmergencySamples();
    }
    if (type == AddContactAttemptCommand::TypeName)
    {
        return addContactAttemptSamples();
    }
    if (type == DeleteContactAttemptCommand::TypeName)
    {
        return deleteContactAttemptSamples();
    }
    if (type == AddTaskCommand::TypeName)
    {
        return addTaskSamples();
    }
    if (type == UpdateTaskCommand::TypeName)
    {
        return updateTaskSamples();
    }
    if (type == DeleteTaskCommand::TypeName)
    {
        return deleteTaskSamples();
    }
    if (type == AddTaskCategoryCommand::TypeName)
    {
        return addTaskCategorySamples();
    }
    if (type == ImportWardDirectoryCommand::TypeName)
    {
        return importWardDirectorySamples();
    }
    if (type == ImportMinisteringCommand::TypeName)
    {
        return importMinisteringSamples();
    }
    if (type == InviteCommand::TypeName)
    {
        return inviteSamples();
    }
    if (type == RemoveUserCommand::TypeName)
    {
        return removeUserSamples();
    }
    if (type == RemoveDeviceCommand::TypeName)
    {
        return removeDeviceSamples();
    }
    if (type == WithdrawInviteCommand::TypeName)
    {
        return withdrawInviteSamples();
    }
    if (type == SetRoleCommand::TypeName)
    {
        return setRoleSamples();
    }
    if (type == RenameUserCommand::TypeName)
    {
        return renameUserSamples();
    }
    if (type == RenameDeviceCommand::TypeName)
    {
        return renameDeviceSamples();
    }
    if (type == JoinCommand::TypeName)
    {
        return joinSamples();
    }
    return {};
}
