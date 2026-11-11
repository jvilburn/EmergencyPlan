#include "AccessTest.h"
#include "AccessCommands.h"
#include "Crypto.h"
#include "FamilyCommands.h"
#include "InvitationFile.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardFile.h"

#include <QTest>

namespace
{

CommitResult removeUser(TestClient& actor, const QString& name)
{
    return actor.session().commit(
        RemoveUserCommand::create(actor.identity().fingerprint(), TestSharing::userNamed(actor, name)));
}

CommitResult setRole(TestClient& actor, const QString& name, AccessRole role)
{
    return actor.session().commit(
        SetRoleCommand::create(actor.identity().fingerprint(), TestSharing::userNamed(actor, name), role));
}

QSet<QByteArray> sealedTo(const WardRead& read)
{
    QSet<QByteArray> fingerprints;
    for (const Recipient& recipient : read.header->recipients())
    {
        fingerprints.insert(recipient.fingerprint);
    }
    return fingerprints;
}

const QString AskAnAdmin = "To add or remove users, ask an admin: Alice";

}  // namespace

AccessTest::AccessTest(QObject* parent)
    : QObject(parent)
{
}

void AccessTest::removalRotatesAndReachesEveryDevice()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QByteArray heldKey = TestSharing::keyOf(alice);
    // Carol writes 2 before the removal reaches anyone; Alice's removal is her 3, so it outranks it.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-3333") == CommitStatus::Applied);
    QByteArray carols = carol.session().writeUpdate();
    alice.session().writeUpdate();
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    // A local save does not rotate.
    QCOMPARE(TestSharing::keyOf(alice), heldKey);

    QByteArray alices = alice.session().writeUpdate();

    QByteArray version = TestSharing::versionIn(alices);
    WardRead read = WardFile::read(version, bob.identity(), PayloadKind::Version);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.dataKey != heldKey);
    QVERIFY(TestSharing::opensUnder(version, read.dataKey));
    QVERIFY(!TestSharing::opensUnder(version, heldKey));
    QCOMPARE(read.payload->ward.access().formerKeys(), QList<QByteArray>{AccessList::formerKeyHash(heldKey)});
    QVERIFY(sealedTo(read) == (QSet<QByteArray>{alice.identity().fingerprint(), bob.identity().fingerprint()}));
    QCOMPARE(TestSharing::keyOf(alice), read.dataKey);

    // Bob takes the removal, then refuses what Carol wrote before it.
    QVERIFY(bob.session().takeUpdate(alices).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
    WardPayload bobBefore = bob.session().payload();
    QVERIFY(bob.session().takeUpdate(carols).status == TakeStatus::UnknownWriter);
    QVERIFY(bob.session().payload() == bobBefore);
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));

    // Carol cannot open the removal, and her copy is no longer hers.
    QVERIFY(carol.session().takeUpdate(alices).status == TakeStatus::NotNamed);
    QVERIFY(carol.session().isNoLongerShared());
    QVERIFY(carol.session().journal().isEmpty());
    CommitResult refused = carol.session().commit(UpdateFamilyCommand::fromEdit(
        TestWard::family(carol.session().document(), TestWard::jonesId()),
        TestWard::withPhone(TestWard::family(carol.session().document(), TestWard::jonesId()), TestWard::carolId(),
                            "555-7777"),
        carol.session().document()));
    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(refused.refusal, QString("This ward is no longer shared with this device."));
    QVERIFY(carol.reopen().status == OpenStatus::Opened);
    QVERIFY(carol.session().isNoLongerShared());
    QCOMPARE(TestSharing::bobsPhone(carol), QString("555-3333"));
}

void AccessTest::writeThatRemovesNobodyKeepsTheKeyAndSealsToExactlyTheAccessList()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice}, {&bob}));
    QByteArray heldKey = TestSharing::keyOf(alice);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);

    QByteArray version = TestSharing::versionIn(alice.session().writeUpdate());

    QVERIFY(TestSharing::opensUnder(version, heldKey));
    WardRead read = WardFile::read(version, bob.identity(), PayloadKind::Version);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QCOMPARE(read.dataKey, heldKey);
    QVERIFY(read.payload->ward.access().formerKeys().isEmpty());
    QVERIFY(sealedTo(read) == (QSet<QByteArray>{alice.identity().fingerprint(), bob.identity().fingerprint()}));
}

void AccessTest::twoAdminsRemovingDifferentUsersBothApply()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &bob, &carol, &dan}, TestSharing::accessOf({&alice, &bob}, {&carol, &dan}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(removeUser(bob, "Dan").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
    QVERIFY(TestSharing::send(bob, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(alice.session().document() == bob.session().document());
    QVERIFY(alice.session().journal().isEmpty());
}

void AccessTest::removingTheSameUserTwiceAppliesInFull()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(removeUser(bob, "Carol").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
}

void AccessTest::removalReplayedAfterThatUserAddedADeviceTakesTheNewDeviceToo()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient carolsPhone("Carol's phone");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    // Alice removes Carol before she has heard of Carol's phone.
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    InviteResult made = carol.session().invite(std::nullopt);
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = carol.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(carol, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    carolsPhone.session().join(carolsPhone.session().readInvitation(invitation), QString(), "Carol's phone");
    QVERIFY(TestSharing::send(carolsPhone, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(TestSharing::userNamed(bob, "Carol").devices().size(), qsizetype(2));
    QVERIFY(TestSharing::send(bob, {&carolsPhone, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));

    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob"}));
    QByteArray alices = alice.session().writeUpdate();
    QVERIFY(carolsPhone.session().takeUpdate(alices).status == TakeStatus::NotNamed);
    QVERIFY(carolsPhone.session().isNoLongerShared());
}

void AccessTest::removalReplayedAfterThatUserMadeAnInviteWithdrawsIt()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    InviteResult made = carol.session().invite(std::nullopt);
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = carol.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(carol, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});

    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob"}));
    QVERIFY(alice.session().document().access().invites().isEmpty());
    QByteArray version = TestSharing::versionIn(alice.session().writeUpdate());
    Identity inviteKeys = Identity::fromInviteSeed(InvitationFile::read(invitation).seed);
    QByteArray inviteKey = WardFile::read(TestSharing::versionIn(invitation), inviteKeys, PayloadKind::Version).dataKey;
    QCOMPARE(inviteKey.size(), qsizetype(Crypto::KeyBytes));
    QVERIFY(WardFile::read(version, inviteKeys, PayloadKind::Version).status == WardReadStatus::NotNamed);
    QVERIFY(!TestSharing::opensUnder(version, inviteKey));
}

void AccessTest::demotedAdminsRemovalIsRefusedWithTheMembersWording()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(bob, "Carol").status == CommitStatus::Applied);
    QVERIFY(setRole(alice, "Bob", AccessRole::Member).status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(bobTakes.replay.toRedo, AskAnAdmin + "\n"
                                     "These need doing again:\n"
                                     "\n"
                                     "Remove Carol (stopped here)\n");
    QCOMPARE(bobTakes.replay.stillHaveAccess, QStringList{"Carol"});
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob", "Carol"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob").role() == AccessRole::Member);
    QVERIFY(bob.session().journal().isEmpty());
}

void AccessTest::onlyAdminCannotStepDownFirstRunOrOnReplay()
{
    const QString onlyAdmin = "You are the only admin. Make someone else an admin first.";
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    CommitResult firstRun = setRole(alice, "Alice", AccessRole::Member);

    QVERIFY(firstRun.status == CommitStatus::Refused);
    QCOMPARE(firstRun.refusal, onlyAdmin);
    QVERIFY(TestSharing::userNamed(alice, "Alice").role() == AccessRole::Admin);
    QVERIFY(alice.session().journal().isEmpty());

    // Two admins each step down while the other is still an admin; the second to land is refused.
    TestClient erin("Erin");
    TestClient frank("Frank");
    TestSharing::share({&erin, &frank}, TestSharing::accessOf({&erin, &frank}, {}));
    QVERIFY(setRole(frank, "Frank", AccessRole::Member).status == CommitStatus::Applied);
    QVERIFY(setRole(erin, "Erin", AccessRole::Member).status == CommitStatus::Applied);

    TakeResult frankTakes = frank.session().takeUpdate(erin.session().writeUpdate());

    QVERIFY(frankTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(frankTakes.replay.toRedo.section('\n', 0, 0), onlyAdmin);
    QVERIFY(TestSharing::userNamed(frank, "Frank").role() == AccessRole::Admin);
    QVERIFY(TestSharing::userNamed(frank, "Erin").role() == AccessRole::Member);
}

void AccessTest::removingYourselfOrYourLastDeviceIsRefusedWithUseLeave()
{
    const QString useLeave = "To stop using this ward, use Leave.";
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    CommitResult yourself = removeUser(alice, "Alice");
    CommitResult lastDevice = carol.session().commit(
        RemoveDeviceCommand::create(carol.identity().fingerprint(), carol.device()));
    CommitResult anothersDevice = alice.session().commit(
        RemoveDeviceCommand::create(alice.identity().fingerprint(), carol.device()));

    QVERIFY(yourself.status == CommitStatus::Refused);
    QCOMPARE(yourself.refusal, useLeave);
    QVERIFY(lastDevice.status == CommitStatus::Refused);
    QCOMPARE(lastDevice.refusal, useLeave);
    QVERIFY(anothersDevice.status == CommitStatus::Refused);
    QCOMPARE(anothersDevice.refusal, QString("You can remove only your own devices."));
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(carol.session().journal().isEmpty());
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Carol"}));
}

void AccessTest::memberCannotRemoveUsersSetRolesOrWithdrawAnothersInvite()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &carol, &dan}, TestSharing::accessOf({&alice}, {&carol, &dan}));
    InviteResult alicesInvite = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(alicesInvite.commit.status == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    AccessInvite invite = *carol.session().document().access().findInvite(alicesInvite.invite);

    CommitResult remove = removeUser(carol, "Dan");
    CommitResult promote = setRole(carol, "Dan", AccessRole::Admin);
    CommitResult withdraw = carol.session().commit(
        WithdrawInviteCommand::create(carol.identity().fingerprint(), invite, carol.session().document().access()));

    QVERIFY(remove.status == CommitStatus::Refused);
    QCOMPARE(remove.refusal, AskAnAdmin);
    QVERIFY(promote.status == CommitStatus::Refused);
    QCOMPARE(promote.refusal, AskAnAdmin);
    QVERIFY(withdraw.status == CommitStatus::Refused);
    QCOMPARE(withdraw.refusal, AskAnAdmin);
    QVERIFY(carol.session().journal().isEmpty());

    // A member withdraws an invite of their own.
    InviteResult carolsInvite = carol.session().invite(std::nullopt);
    QVERIFY(carolsInvite.commit.status == CommitStatus::Applied);
    AccessInvite own = *carol.session().document().access().findInvite(carolsInvite.invite);
    CommitResult withdrawOwn = carol.session().commit(
        WithdrawInviteCommand::create(carol.identity().fingerprint(), own, carol.session().document().access()));
    QVERIFY(withdrawOwn.status == CommitStatus::Applied);
    QVERIFY(!carol.session().document().access().findInvite(carolsInvite.invite).has_value());
    QVERIFY(carol.session().document().access().findInvite(alicesInvite.invite).has_value());
}

void AccessTest::removingAnotherDeviceOfYourOwnRotatesItOut()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    AccessList access({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {laptop.device(), phone.device()})},
                      {}, {});
    TestSharing::share({&laptop, &phone}, access);
    QByteArray heldKey = TestSharing::keyOf(phone);

    QVERIFY(laptop.session().commit(RemoveDeviceCommand::create(laptop.identity().fingerprint(), phone.device())).status
            == CommitStatus::Applied);
    QByteArray laptops = laptop.session().writeUpdate();

    QCOMPARE(TestSharing::userNamed(laptop, "Alice").devices().size(), qsizetype(1));
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(laptops), TestSharing::keyOf(laptop)));
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(laptops), heldKey));
    QVERIFY(phone.session().takeUpdate(laptops).status == TakeStatus::NotNamed);
    QVERIFY(phone.session().isNoLongerShared());
}

void AccessTest::roleChangeReplayedAfterItsUserWasRemovedConflicts()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(setRole(bob, "Carol", AccessRole::Admin).status == CommitStatus::Applied);
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(bobTakes.replay.toRedo.section('\n', 0, 0),
             QString("Your changes could not be applied, because the ward changed first."));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
}

void AccessTest::offlineAcrossTwoRotationsTakesTheLatest()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestClient erin("Erin");
    TestSharing::share({&alice, &bob, &carol, &dan, &erin},
                       TestSharing::accessOf({&alice, &bob}, {&carol, &dan, &erin}));
    QByteArray firstKey = TestSharing::keyOf(erin);
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QByteArray secondKey = TestSharing::keyOf(bob);
    QVERIFY(secondKey != firstKey);
    QVERIFY(removeUser(bob, "Dan").status == CommitStatus::Applied);

    QVERIFY(TestSharing::send(bob, {&erin}) == QList<TakeStatus>{TakeStatus::Taken});

    QCOMPARE(erin.session().document().access().formerKeys(),
             (QList<QByteArray>{AccessList::formerKeyHash(firstKey), AccessList::formerKeyHash(secondKey)}));
    QCOMPARE(TestSharing::userNames(erin), (QStringList{"Alice", "Bob", "Erin"}));
    QVERIFY(TestSharing::keyOf(erin) != secondKey);
}

void AccessTest::updatesThatCrossARotationConverge()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &bob, &carol, &dan}, TestSharing::accessOf({&alice, &bob}, {&carol, &dan}));
    QByteArray firstKey = TestSharing::keyOf(alice);
    // Bob sends twice before he hears from Alice, so his 3 outranks her 2.
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-2222") == CommitStatus::Applied);
    bob.session().writeUpdate();
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QByteArray alices2 = alice.session().writeUpdate();
    QVERIFY(dan.session().takeUpdate(alices2).status == TakeStatus::Taken);
    QVERIFY(TestSharing::keyOf(dan) != firstKey);

    // Bob's 3 is under the key Alice rotated away from; she takes it and her removal replays.
    TakeResult aliceTakes = alice.session().takeUpdate(bobs);

    QVERIFY(aliceTakes.status == TakeStatus::Taken);
    QVERIFY(aliceTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob", "Dan"}));
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-3333"));

    // Her next version is under a third key. Dan, under the second, takes it by the first, which both name.
    QByteArray alices4 = alice.session().writeUpdate();
    QVERIFY(dan.session().takeUpdate(alices4).status == TakeStatus::Taken);
    QVERIFY(bob.session().takeUpdate(alices4).status == TakeStatus::Taken);
    QVERIFY(dan.session().document() == alice.session().document());
    QVERIFY(bob.session().document() == alice.session().document());
    QVERIFY(bob.session().journal().isEmpty());

    // Bob, having taken a removal, writes under the third key too.
    QVERIFY(TestSharing::commitPhone(bob, TestWard::jonesId(), TestWard::carolId(), "555-6666") == CommitStatus::Applied);
    QByteArray bobs5 = bob.session().writeUpdate();
    QVERIFY(alice.session().takeUpdate(bobs5).status == TakeStatus::Taken);
    QVERIFY(dan.session().takeUpdate(bobs5).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::phoneOf(dan, TestWard::carolId()), QString("555-6666"));

    // Carol, removed, opens none of the versions written after her removal.
    const QList<QByteArray> afterRemoval = {alices2, alices4, bobs5};
    for (const QByteArray& update : afterRemoval)
    {
        QByteArray version = TestSharing::versionIn(update);
        QVERIFY(WardFile::read(version, carol.identity(), PayloadKind::Version).status == WardReadStatus::NotNamed);
        QVERIFY(!TestSharing::opensUnder(version, firstKey));
    }
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(bobs), firstKey));
}

void AccessTest::renamesReachEveryDevice()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));
    QByteArray actor = alice.identity().fingerprint();
    QVERIFY(alice.session().commit(RenameUserCommand::create(actor, alice.session().document().access(), "Alice Smith"))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.session()
                .commit(RenameDeviceCommand::create(actor, alice.session().document().access(), "Alice's laptop"))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.session().commit(RenameUserCommand::create(actor, alice.session().document().access(), "Alice Smith"))
                .status
            == CommitStatus::NothingToDo);

    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});

    AccessUser renamed = TestSharing::userNamed(bob, "Alice Smith");
    QCOMPARE(renamed.devices().size(), qsizetype(1));
    QCOMPARE(renamed.devices()[0].name(), QString("Alice's laptop"));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice Smith", "Bob"}));
}
