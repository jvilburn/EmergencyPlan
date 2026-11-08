#include "EveryField.h"
#include "CommandRegistry.h"
#include "CommandSamples.h"
#include "Crypto.h"
#include "TestWard.h"

namespace
{

/// Bob with a value in every Person field.
Person bobWithEveryField()
{
    Person bob = TestWard::bob();
    bob.setAltPhone(Phone("555-1112"));
    bob.setBirthday(Birthday::create(1980, 7, 4));
    bob.setCallings({"Clerk", "Choir director"});
    bob.setWardUnitNumber("123456");
    bob.setStakeUnitNumber("654321");
    bob.setSpecialNeedNote("Uses a wheelchair; the ramp is at the back door.");
    bob.setNote("Drives the ward van");
    return bob;
}

Ward tanglewood()
{
    Ward ward;
    ward.setName("Tanglewood");
    ward.setUnitNumber("123456");
    ward.setStakeUnitNumber("654321");
    ward.setChapelAddress("100 Chapel Rd, Provo, UT");
    ward.setChapelLat(40.2501);
    ward.setChapelLng(-111.6493);
    ward.setChapelPhone("+1 555-555-1234");
    ward.setMeetingTime("Sunday 9:00 AM");
    return ward;
}

Stake provoStake()
{
    Stake stake;
    stake.setName("Provo Utah");
    stake.setUnitNumber("654321");
    stake.setWardUnitNumbers({"123456", "234567"});
    stake.setStakeCenterAddress("200 Stake Center Dr, Provo, UT");
    stake.setStakeCenterLat(40.2602);
    stake.setStakeCenterLng(-111.6604);
    return stake;
}

/// The Storm emergency as documentWithPlaces holds it, with the Flooding task's assignment
/// notified and the task resolved, an attempt with every field, an attempt with only an
/// outcome, and the Power lines category added.
EmergencyResponse stormWithEveryField(const EmergencyResponse& storm)
{
    EmergencyResponse response = storm;
    ResponseTask basement = *response.findTask(TestWard::basementTaskId());
    TaskAssignment toSetup = *basement.assignment();
    toSetup.setNotification(TestWard::notification(ContactMethod::Text, "Bring the pump", TestWard::octoberFirst(10, 45)));
    basement.setAssignment(toSetup);
    basement.setResolution(TestWard::resolution("Pumped out", TestWard::octoberFirst(12, 0)));
    response.setTask(basement);
    response.insertContactAttempt(TestWard::attempt("attempt-phoned", TestWard::smith(), TestWard::octoberFirst(9, 0),
                                                    ContactMethod::Phone, TestWard::bob(), ContactOutcome::OK,
                                                    "All well"));
    response.insertContactAttempt(TestWard::attempt("attempt-marked", TestWard::jones(), TestWard::octoberFirst(9, 30),
                                                    std::nullopt, std::nullopt, ContactOutcome::UnableToReach,
                                                    QString()));
    response.addTaskCategory("Power lines");
    return response;
}

AccessList everyAccessField(const Identity& laptop, const Identity& phone)
{
    Identity desktop = Identity::generate();
    Identity bobInvite = Identity::generate();
    Identity carolInvite = Identity::generate();
    UserId alice = UserId::generate();
    UserId carol = UserId::generate();

    QList<AccessUser> users = {
        AccessUser(alice, "Alice Smith", AccessRole::Admin,
                   {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                    AccessDevice("Pixel 8", phone.publicKey(), phone.verifyKey())}),
        AccessUser(carol, "Carol Jones", AccessRole::Member,
                   {AccessDevice("Desktop", desktop.publicKey(), desktop.verifyKey())}),
    };
    QList<AccessInvite> invites = {
        AccessInvite::forUser(alice, AccessRole::Member, "Bob", bobInvite.publicKey(), bobInvite.verifyKey()),
        AccessInvite::forDevice(carol, carolInvite.publicKey(), carolInvite.verifyKey()),
    };
    return AccessList(users, invites, {Crypto::randomBytes(Crypto::KeyBytes)});
}

DeviceState everyDeviceField(const AccessList& access)
{
    DeviceState device;
    for (const CommandType& type : CommandRegistry::types())
    {
        QList<CommandSample> samples = CommandSamples::forType(type.name);
        Q_ASSERT(!samples.isEmpty());
        device.journal.append(*samples.front().command);
    }
    device.baseVersion = 7;
    device.dismissedAccess = access;
    device.singleHolderNoticeDismissed = true;
    device.singleAdminNoticeDismissed = true;
    device.noLongerShared = true;
    device.inviteSeeds = {Crypto::randomBytes(Crypto::KeyBytes), Crypto::randomBytes(Crypto::KeyBytes)};
    return device;
}

}  // namespace

Document EveryField::ward()
{
    Document ward = TestWard::documentWithPlaces();
    ward.setFamily(TestWard::withMember(TestWard::smith(), bobWithEveryField()));
    Family smith = TestWard::family(ward, TestWard::smithId());
    smith.setNote("Gate code is 1234");
    ward.setFamily(smith);

    MinisteringGroup rsGroup = TestWard::rsGroup();
    rsGroup.setPresidencyMemberId(TestWard::aliceId());
    ward.setGroup(MinisteringOrg::ReliefSociety, rsGroup);
    MinisteringDistrict rsDistrict = MinisteringDistrict::create("District 2", TestWard::aliceId(), {rsGroup.id()});
    ward.setDistrict(MinisteringOrg::ReliefSociety, rsDistrict);

    ward.metadata().updateWard(tanglewood());
    ward.metadata().updateStake(provoStake());
    ward.setAddressCoordinates(TestWard::smith().address().full(), Coordinates(40.2338, -111.6585));
    ward.setWardDirectoryPdfDate(QDate(2026, 10, 15));
    ward.setMinisteringPdfDate(QDate(2026, 9, 1));
    return TestWard::withResponse(ward, stormWithEveryField(*ward.emergencyResponse()));
}

WardPayload EveryField::payload(const Identity& laptop, const Identity& phone)
{
    Document ward = EveryField::ward();
    ward.setAccess(everyAccessField(laptop, phone));
    return WardPayload{ward,
                       {{laptop.fingerprint(), 3}, {phone.fingerprint(), 5}},
                       everyDeviceField(ward.access())};
}
