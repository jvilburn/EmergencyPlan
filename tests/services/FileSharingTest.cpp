#include "FileSharingTest.h"
#include "AccessCommands.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "InvitationFile.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace
{

QString writerOf(const QByteArray& update)
{
    return WardFileEdits::headerOf(TestSharing::versionIn(update))["writer"].toString();
}

}  // namespace

FileSharingTest::FileSharingTest(QObject* parent)
    : QObject(parent)
{
}

void FileSharingTest::invitationReadsBackItsDocumentAndSeed()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    VersionRank held = alice.session().version();

    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);

    QVERIFY(made.commit.status == CommitStatus::Applied);
    QList<QByteArray> seeds = alice.session().payload().device->inviteSeeds;
    QCOMPARE(seeds.size(), qsizetype(1));
    QCOMPARE(Identity::fromInviteSeed(seeds[0]).fingerprint(), made.invite);
    InvitationRead read = InvitationFile::read(invitation);
    QVERIFY(read.status == InvitationReadStatus::Read);
    QCOMPARE(read.seed, seeds[0]);
    // The invitation carries the next version, which names the invite; this device still holds
    // the version before it, so the invite is still a change to share.
    QCOMPARE(WardFileEdits::headerOf(read.version)["number"].toInteger(), held.number + 1);
    WardRead version = WardFile::read(read.version, Identity::fromInviteSeed(read.seed), PayloadKind::Version);
    QVERIFY2(version.status == WardReadStatus::Read, qPrintable(version.problem));
    QVERIFY(version.header->document() == alice.session().documentId());
    QVERIFY(alice.session().version() == held);
    QVERIFY(alice.session().hasChangesToShare());
    AccessInvite invite = *alice.session().document().access().findInvite(made.invite);
    QCOMPARE(alice.session().document().access().describeInvite(invite), QString("for Bob"));
    QCOMPARE(InvitationFile::fileName("Tanglewood Ward", "for Bob"), QString("Tanglewood Ward - for Bob.wardinvite"));
}

void FileSharingTest::anyOtherFileIsNotAnInvitation()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    DocumentId ward = TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(std::nullopt);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QByteArray wardFile = alice.store().file(ward);
    QByteArray update = alice.session().writeUpdate();
    QJsonObject newerLine{{"format", 2}, {"seed", Base64Url::encode(Crypto::randomBytes(Crypto::KeyBytes))}};
    QByteArray newer = QJsonDocument(newerLine).toJson(QJsonDocument::Compact) + '\n' + TestSharing::versionIn(invitation);
    TestClient stranger("Stranger");

    const QList<QByteArray> others = {wardFile, update, QByteArray(), QByteArray("not an invitation\n")};
    for (const QByteArray& file : others)
    {
        QVERIFY(stranger.session().readInvitation(file).status == InvitationStatus::NotAnInvitation);
    }
    QVERIFY(stranger.session().readInvitation(newer).status == InvitationStatus::Newer);
    QVERIFY(stranger.session().readInvitation(invitation).status == InvitationStatus::Joinable);
}

void FileSharingTest::userInvitedAndJoinedEntirelyThroughFiles()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    QByteArray firstKey = TestSharing::keyOf(alice);

    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(alice.session().hasChangesToShare());
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(!alice.session().hasChangesToShare());
    QVERIFY(carol.session().document().access().findInvite(made.invite).has_value());

    InvitedWard invited = bob.session().readInvitation(invitation);

    QVERIFY(invited.status == InvitationStatus::Joinable);
    QCOMPARE(invited.wardName, alice.session().document().wardName());
    QCOMPARE(invited.invite->invitedUser()->label, QString("Bob"));

    bob.session().join(invited, {"Bob Brown", ""}, "Bob's phone");

    QVERIFY(bob.session().documentId() == alice.session().documentId());
    AccessUser bobUser = TestSharing::userNamed(bob, "Bob Brown");
    QVERIFY(bobUser.role() == AccessRole::Member);
    QCOMPARE(bobUser.devices().size(), qsizetype(1));
    QCOMPARE(bobUser.devices()[0].fingerprint(), bob.identity().fingerprint());
    QVERIFY(bob.session().document().access().invites().isEmpty());
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);
    QVERIFY(bob.session().payload().device->joinSeed == InvitationFile::read(invitation).seed);

    // The join goes out written by the invite, under a key the invite's seed cannot reach.
    QByteArray join = bob.session().writeUpdate();
    QCOMPARE(writerOf(join), Base64Url::encode(made.invite));
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(invitation), firstKey));
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(join), firstKey));
    QVERIFY(!bob.session().hasChangesToShare());
    QVERIFY(alice.session().takeUpdate(join).status == TakeStatus::Taken);
    QVERIFY(carol.session().takeUpdate(join).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(alice.session().invitesMadeHere().isEmpty());
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());

    // Carol's next version names Bob, and carries his join; taking it, Bob keeps the seed and,
    // holding a version that names him, writes as himself.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(carol, {&bob, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(bob.session().payload().device->joinSeed == InvitationFile::read(invitation).seed);
    QVERIFY(bob.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(bob.identity().fingerprint()));
    QVERIFY(alice.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-3333"));
    QVERIFY(alice.session().document() == bob.session().document());
}

void FileSharingTest::memberVersionWrittenBeforeTakingTheJoinIsReadWithTheSeed()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    bob.session().join(bob.session().readInvitation(invitation), {"Bob Brown", ""}, "Bob's phone");
    UserId bobsId = TestSharing::userNamed(bob, "Bob Brown").id();
    // Carol writes before she has Bob's join: her version is sealed to the open invite, not to Bob.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QByteArray carols = carol.session().writeUpdate();
    QVERIFY(WardFile::read(TestSharing::versionIn(carols), bob.identity(), PayloadKind::Version).status
            == WardReadStatus::NotNamed);

    TakeResult bobTakes = bob.session().takeUpdate(carols);

    // The join runs again on Carol's version; the first join, still in the journal, finds its
    // result already there.
    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob Brown").id() == bobsId);
    QCOMPARE(TestSharing::userNamed(bob, "Bob Brown").devices()[0].fingerprint(), bob.identity().fingerprint());
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(2));
    QCOMPARE(bob.session().journal().entries()[0].command["type"].toString(), QString("join"));
    QCOMPARE(bob.session().journal().entries()[1].command["type"].toString(), QString("join"));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);

    // The version Bob holds does not name him, so his next is written as the invite, which
    // both copies still list.
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(made.invite));
    QVERIFY(carol.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QVERIFY(alice.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(carol), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(carol.session().document() == bob.session().document());
    QVERIFY(alice.session().document() == bob.session().document());
}

void FileSharingTest::versionFromAMemberWhoNeverTookTheJoinRunsTheJoinAgain()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol, &dan}, TestSharing::accessOf({&alice}, {&carol, &dan}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol, &dan}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    bob.session().join(bob.session().readInvitation(invitation), {"Bob Brown", ""}, "Bob's phone");
    UserId bobsId = TestSharing::userNamed(bob, "Bob Brown").id();
    QVERIFY(TestSharing::send(bob, {&alice, &carol}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    // Carol's 4 names Bob, and Bob takes it: his join has landed.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(carol, {&bob, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(bob.session().journal().isEmpty());
    // Dan never took the join, and sends three times, so his 5 outranks Carol's 4.
    dan.session().writeUpdate();
    dan.session().writeUpdate();
    QVERIFY(TestSharing::commitPhone(dan, TestWard::jonesId(), TestWard::carolId(), "555-6666") == CommitStatus::Applied);
    QByteArray dans = dan.session().writeUpdate();
    QCOMPARE(WardFileEdits::headerOf(TestSharing::versionIn(dans))["number"].toInteger(), qint64(5));
    QVERIFY(WardFile::read(TestSharing::versionIn(dans), bob.identity(), PayloadKind::Version).status
            == WardReadStatus::NotNamed);

    TakeResult bobTakes = bob.session().takeUpdate(dans);

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(!bob.session().isNoLongerShared());
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Carol", "Dan", "Bob Brown"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob Brown").id() == bobsId);
    QCOMPARE(TestSharing::phoneOf(bob, TestWard::carolId()), QString("555-6666"));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(bob.session().journal().entries()[0].command["type"].toString(), QString("join"));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);

    // Bob's 6 is written as the invite, which Dan's copy lists; Dan takes the join with it.
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(made.invite));
    QVERIFY(dan.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(dan), (QStringList{"Alice", "Carol", "Dan", "Bob Brown"}));

    // Dan's 7 reaches everyone, Carol's edit goes on top of it in her 8, and all hold equal wards.
    QVERIFY(TestSharing::send(dan, {&alice, &carol, &bob})
            == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(TestSharing::send(carol, {&alice, &bob, &dan})
            == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(alice.session().document() == carol.session().document());
    QVERIFY(bob.session().document() == carol.session().document());
    QVERIFY(dan.session().document() == carol.session().document());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(TestSharing::phoneOf(alice, TestWard::carolId()), QString("555-6666"));
    QVERIFY(bob.session().journal().isEmpty());
}

void FileSharingTest::sameInvitationUsedOnTwoDevices()
{
    TestClient alice("Alice");
    TestClient phone("Bob's phone");
    TestClient tablet("Bob's tablet");
    TestSharing::share({&alice}, TestSharing::accessOf({&alice}, {}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    phone.session().join(phone.session().readInvitation(invitation), {"Bob", ""}, "Bob's phone");
    tablet.session().join(tablet.session().readInvitation(invitation), {"Bob", ""}, "Bob's tablet");
    QByteArray phones = phone.session().writeUpdate();
    QByteArray tablets = tablet.session().writeUpdate();

    QVERIFY(alice.session().takeUpdate(phones).status == TakeStatus::Taken);
    // The tablet's join is written by the invite, which Alice's list no longer holds.
    QVERIFY(alice.session().takeUpdate(tablets).status == TakeStatus::UnknownWriter);
    QCOMPARE(TestSharing::userNamed(alice, "Bob").devices()[0].fingerprint(), phone.identity().fingerprint());

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QByteArray alices = alice.session().writeUpdate();

    QVERIFY(tablet.session().takeUpdate(alices).status == TakeStatus::InviteUsedOrWithdrawn);
    QVERIFY(tablet.session().isNoLongerShared());
    QVERIFY(phone.session().takeUpdate(alices).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::bobsPhone(phone), QString("555-2222"));
}

void FileSharingTest::withdrawnInviteRotatesAndItsInvitationJoinsNothingLater()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    AccessInvite invite = *alice.session().document().access().findInvite(made.invite);
    QVERIFY(alice.session()
                .commit(WithdrawInviteCommand::create(alice.identity().fingerprint(), invite,
                                                      alice.session().document().access()))
                .status
            == CommitStatus::Applied);

    QByteArray alices = alice.session().writeUpdate();

    Identity inviteKeys = Identity::fromInviteSeed(InvitationFile::read(invitation).seed);
    QByteArray inviteKey = WardFile::read(TestSharing::versionIn(invitation), inviteKeys, PayloadKind::Version).dataKey;
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(invitation), inviteKey));
    QVERIFY(WardFile::read(TestSharing::versionIn(alices), inviteKeys, PayloadKind::Version).status
            == WardReadStatus::NotNamed);
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(alices), inviteKey));
    QVERIFY(carol.session().takeUpdate(alices).status == TakeStatus::Taken);
    QVERIFY(carol.session().document().access().invites().isEmpty());

    // The invitation still opens the version it carries, and joins nothing after it.
    InvitedWard invited = bob.session().readInvitation(invitation);
    QVERIFY(invited.status == InvitationStatus::Joinable);
    bob.session().join(invited, {"Bob Brown", ""}, "Bob's phone");
    QVERIFY(bob.session().takeUpdate(alices).status == TakeStatus::InviteUsedOrWithdrawn);
    QVERIFY(bob.session().isNoLongerShared());
}

void FileSharingTest::invitationOpenedOnADeviceTheWardNamesJoinsNothing()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(std::nullopt);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    WardPayload carolBefore = carol.session().payload();
    VersionRank carolVersion = carol.session().version();

    InvitedWard invited = carol.session().readInvitation(invitation);

    QVERIFY(invited.status == InvitationStatus::AlreadyHasAccess);
    QCOMPARE(invited.wardName, alice.session().document().wardName());
    QVERIFY(carol.session().openWard(invited.document).status == OpenStatus::Opened);
    QVERIFY(carol.session().payload() == carolBefore);
    QVERIFY(carol.session().version() == carolVersion);
    QVERIFY(alice.session().document().access().findInvite(made.invite).has_value());
}

void FileSharingTest::invitationOpenedAgainOnTheDeviceThatJoinedThroughItKeepsItsCopy()
{
    TestClient alice("Alice");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice}, TestSharing::accessOf({&alice}, {}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    bob.session().join(bob.session().readInvitation(invitation), {"Bob Brown", ""}, "Bob's phone");
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-2222") == CommitStatus::Applied);
    WardPayload bobBefore = bob.session().payload();
    VersionRank bobVersion = bob.session().version();

    InvitedWard invited = bob.session().readInvitation(invitation);

    QVERIFY(invited.status == InvitationStatus::AlreadyHasAccess);
    QVERIFY(invited.document == alice.session().documentId());
    QVERIFY(bob.session().openWard(invited.document).status == OpenStatus::Opened);
    QVERIFY(bob.session().payload() == bobBefore);
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(2));
    QVERIFY(bob.session().version() == bobVersion);
    QCOMPARE(bobVersion.writer, made.invite);
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
}

void FileSharingTest::deviceInviteJoinsAsOneOfItsMakersDevices()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    TestSharing::share({&laptop}, AccessList({AccessUser(UserId::generate(), {"Alice", "alice@example.com"},
                                                         AccessRole::Admin, {laptop.device()})},
                                             {}, {}));
    InviteResult made = laptop.session().invite(std::nullopt);
    AccessInvite invite = *laptop.session().document().access().findInvite(made.invite);
    QCOMPARE(laptop.session().document().access().describeInvite(invite), QString("a device for Alice"));
    QByteArray invitation = laptop.session().writeInvitation(made.invite);

    InvitedWard invited = phone.session().readInvitation(invitation);
    phone.session().join(invited, UserDetails(), "Phone");

    QVERIFY(invited.status == InvitationStatus::Joinable);
    QVERIFY(!invited.invite->invitedUser().has_value());
    AccessUser alice = TestSharing::userNamed(phone, "Alice");
    QCOMPARE(alice.devices().size(), qsizetype(2));
    QCOMPARE(alice.devices()[1].name(), QString("Phone"));
    QCOMPARE(alice.devices()[1].fingerprint(), phone.identity().fingerprint());
    QVERIFY(phone.session().ownUser().details() == UserDetails({"Alice", "alice@example.com"}));
    QVERIFY(phone.session().followUserDetails(UserDetails()) == UserDetails({"Alice", "alice@example.com"}));
    QVERIFY(TestSharing::send(phone, {&laptop}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(TestSharing::userNamed(laptop, "Alice").devices().size(), qsizetype(2));
}

void FileSharingTest::seedLeavesTheFileOnceTheInviteIsWithdrawnOrAbandoned()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult first = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(first.commit.status == CommitStatus::Applied);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QCOMPARE(alice.session().payload().device->inviteSeeds.size(), qsizetype(1));
    QVERIFY(alice.session().invitesMadeHere() == QSet<QByteArray>{first.invite});

    AccessInvite invite = *alice.session().document().access().findInvite(first.invite);
    QVERIFY(alice.session()
                .commit(WithdrawInviteCommand::create(alice.identity().fingerprint(), invite,
                                                      alice.session().document().access()))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());

    // An invite abandoned with a replay takes its seed with it.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    InviteResult second = alice.session().invite(InvitedUser{AccessRole::Member, "Dan"});
    QVERIFY(second.commit.status == CommitStatus::Applied);
    QCOMPARE(alice.session().payload().device->inviteSeeds.size(), qsizetype(1));
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-9999") == CommitStatus::Applied);

    TakeResult aliceTakes = alice.session().takeUpdate(carol.session().writeUpdate());

    QVERIFY(aliceTakes.replay.status == ReplayStatus::Abandoned);
    QVERIFY(alice.session().invitesMadeHere().isEmpty());
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-9999"));
}

void FileSharingTest::addAUserIsForAdminsAndAnyoneAddsADevice()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    InviteResult refused = carol.session().invite(InvitedUser{AccessRole::Member, "Dan"});
    InviteResult device = carol.session().invite(std::nullopt);

    QVERIFY(refused.commit.status == CommitStatus::Refused);
    QCOMPARE(refused.commit.refusal, QString("To add or remove users, ask an admin: Alice"));
    QVERIFY(refused.invite.isEmpty());
    QVERIFY(device.commit.status == CommitStatus::Applied);
    QVERIFY(carol.session().invitesMadeHere() == QSet<QByteArray>{device.invite});
    QCOMPARE(carol.session().journal().entries().size(), qsizetype(1));
    QVERIFY(carol.session().document().access().findInvite(device.invite)->maker()
            == TestSharing::userNamed(carol, "Carol").id());
}

void FileSharingTest::changesToShareThroughCrossedUpdates()
{
    TestClient first("Alice");
    TestClient second("Bob");
    TestSharing::share({&first, &second}, TestSharing::accessOf({&first, &second}, {}));
    bool firstIsHigher = first.identity().fingerprint() > second.identity().fingerprint();
    TestClient& high = firstIsHigher ? first : second;
    TestClient& low = firstIsHigher ? second : first;
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(!low.session().hasChangesToShare());

    // Shown on both after their edits, and cleared on both by sending 2.
    QVERIFY(TestSharing::commitBobsPhone(high, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::commitPhone(low, TestWard::jonesId(), TestWard::carolId(), "555-5555")
            == CommitStatus::Applied);
    QVERIFY(high.session().hasChangesToShare());
    QVERIFY(low.session().hasChangesToShare());
    QByteArray highs = high.session().writeUpdate();
    QByteArray lows = low.session().writeUpdate();
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(!low.session().hasChangesToShare());

    // The higher writer sets the lower 2 aside and keeps its entry, which its own 2 carries.
    QVERIFY(high.session().takeUpdate(lows).status == TakeStatus::NotNewer);
    QVERIFY(!high.session().hasChangesToShare());
    QCOMPARE(high.session().journal().entries().size(), qsizetype(1));

    // The lower writer takes the higher 2 and its change goes on top: shown again, until it sends 3.
    TakeResult lowTakes = low.session().takeUpdate(highs);
    QVERIFY(lowTakes.replay.status == ReplayStatus::Replayed);
    QVERIFY(low.session().hasChangesToShare());
    QByteArray lows3 = low.session().writeUpdate();
    QVERIFY(!low.session().hasChangesToShare());

    QVERIFY(high.session().takeUpdate(lows3).status == TakeStatus::Taken);
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(high.session().journal().isEmpty());
    QCOMPARE(TestSharing::phoneOf(high, TestWard::carolId()), QString("555-5555"));
}

void FileSharingTest::renamingThisDeviceJournalsOneRenameInEachWardThatNamesItOtherwise()
{
    TestClient laptop("Laptop");
    TestClient bob("Bob");
    DocumentId renamed = TestSharing::share({&laptop, &bob}, TestSharing::accessOf({&laptop, &bob}, {}));
    AccessDevice workLaptop("Work laptop", laptop.identity().publicKey(), laptop.identity().verifyKey());
    DocumentId alreadyNamed = TestSharing::share(
        {&laptop},
        AccessList({AccessUser(UserId::generate(), {"Alice", ""}, AccessRole::Admin, {workLaptop})}, {}, {}));
    DocumentId gone = TestSharing::share({&laptop, &bob}, TestSharing::accessOf({&bob}, {&laptop}));
    QVERIFY(bob.session()
                .commit(RemoveUserCommand::create(bob.identity().fingerprint(), TestSharing::userNamed(bob, "Laptop")))
                .status
            == CommitStatus::Applied);
    QVERIFY(TestSharing::send(bob, {&laptop}) == QList<TakeStatus>{TakeStatus::NotNamed});
    QVERIFY(laptop.session().openWard(renamed).status == OpenStatus::Opened);

    laptop.session().renameDeviceEverywhere("Work laptop");

    const QByteArray& self = laptop.identity().fingerprint();
    QCOMPARE(laptop.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(laptop.session().journal().entries()[0].command["type"].toString(), QString("renameDevice"));
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Work laptop"));
    QVERIFY(laptop.session().openWard(alreadyNamed).status == OpenStatus::Opened);
    QVERIFY(laptop.session().journal().isEmpty());
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Work laptop"));
    QVERIFY(laptop.session().openWard(gone).status == OpenStatus::Opened);
    QVERIFY(laptop.session().isNoLongerShared());
    QVERIFY(laptop.session().journal().isEmpty());
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Laptop"));

    // The rename reaches Bob by the ordinary path.
    QVERIFY(laptop.session().openWard(renamed).status == OpenStatus::Opened);
    QVERIFY(bob.session().openWard(renamed).status == OpenStatus::Opened);
    QVERIFY(TestSharing::send(laptop, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(bob.session().document().access().findDevice(self)->name(), QString("Work laptop"));
}

void FileSharingTest::secondDeviceTakingTheNewUserDetailsFollowsThem()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    TestClient carol("Carol");
    AccessUser alice(UserId::generate(), {"Alice", ""}, AccessRole::Admin, {laptop.device(), phone.device()});
    DocumentId first = TestSharing::share({&laptop, &phone}, AccessList({alice}, {}, {}));
    DocumentId second = TestSharing::share({&laptop, &phone}, AccessList({alice}, {}, {}));
    DocumentId phoneOnly = TestSharing::share(
        {&phone, &carol},
        AccessList({AccessUser(UserId::generate(), {"Alice", ""}, AccessRole::Admin, {phone.device()}),
                    AccessUser(UserId::generate(), {"Carol", ""}, AccessRole::Member, {carol.device()})},
                   {}, {}));
    UserDetails changed{"Alice Jones", "alice@example.com"};
    QVERIFY(laptop.session().openWard(first).status == OpenStatus::Opened);

    laptop.session().setUserDetailsEverywhere(changed);

    // Set in Settings, the details reach every ward the laptop holds, one command in each.
    QCOMPARE(laptop.session().journal().entries().size(), qsizetype(1));
    QVERIFY(laptop.session().ownUser().details() == changed);
    QVERIFY(laptop.session().openWard(second).status == OpenStatus::Opened);
    QCOMPARE(laptop.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(laptop.session().journal().entries()[0].command["type"].toString(), QString("setUserDetails"));
    QCOMPARE(laptop.session().ownUser().name(), QString("Alice Jones"));
    QCOMPARE(laptop.session().ownUser().email(), QString("alice@example.com"));
    QVERIFY(laptop.session().openWard(first).status == OpenStatus::Opened);
    QVERIFY(phone.session().openWard(first).status == OpenStatus::Opened);
    QVERIFY(TestSharing::send(laptop, {&phone}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(phone.session().ownUser().name(), QString("Alice Jones"));
    QCOMPARE(phone.session().ownUser().email(), QString("alice@example.com"));

    std::optional<UserDetails> followed = phone.session().followUserDetails({"Alice", ""});

    QVERIFY(followed == changed);
    QVERIFY(phone.session().journal().isEmpty());
    QVERIFY(phone.session().openWard(second).status == OpenStatus::Opened);
    QCOMPARE(phone.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(phone.session().journal().entries()[0].command["type"].toString(), QString("setUserDetails"));
    QCOMPARE(phone.session().ownUser().name(), QString("Alice Jones"));
    QCOMPARE(phone.session().ownUser().email(), QString("alice@example.com"));
    QVERIFY(phone.session().openWard(phoneOnly).status == OpenStatus::Opened);
    QCOMPARE(phone.session().ownUser().name(), QString("Alice Jones"));
    QCOMPARE(phone.session().ownUser().email(), QString("alice@example.com"));
    QCOMPARE(TestSharing::userNames(phone), (QStringList{"Alice Jones", "Carol"}));
    QCOMPARE(TestSharing::userNamed(phone, "Carol").email(), QString());
    QVERIFY(!phone.session().followUserDetails(changed).has_value());
}
