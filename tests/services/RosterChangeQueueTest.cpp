#include "RosterChangeQueueTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QtTest>

using namespace ServerContract;
using namespace SessionTestSupport;
using namespace std::placeholders;

using Timer = SessionTimers::Timer;

namespace
{

/// What a queue reported.
struct Reports
{
    QList<RosterChangeOutcome> outcomes;
    QStringList published;
    QStringList removed;
    bool left = false;

    void watch(RosterChangeQueue& queue);

    void onFinished(const QString&, const RosterChange&, RosterChangeOutcome outcome) { outcomes.append(outcome); }
    void onPublished(const QString& documentId) { published.append(documentId); }
    void onRemoved(const QString& documentId, bool byLeaving);
};

void Reports::watch(RosterChangeQueue& queue)
{
    QObject::connect(&queue, &RosterChangeQueue::changeFinished, &queue, std::bind(&Reports::onFinished, this, _1, _2, _3));
    QObject::connect(&queue, &RosterChangeQueue::published, &queue, std::bind(&Reports::onPublished, this, _1));
    QObject::connect(&queue, &RosterChangeQueue::removed, &queue, std::bind(&Reports::onRemoved, this, _1, _2));
}

void Reports::onRemoved(const QString& documentId, bool byLeaving)
{
    removed.append(documentId);
    left = byLeaving;
}

RosterChange changeOf(RosterChange::Kind kind)
{
    RosterChange change;
    change.kind = kind;
    return change;
}

RosterChange removal(const DeviceIdentity& person)
{
    RosterChange change = changeOf(RosterChange::Kind::Remove);
    change.target = person.member;
    return change;
}

RosterChange roleOf(const DeviceIdentity& person, RosterPerson::Role role)
{
    RosterChange change = changeOf(RosterChange::Kind::SetRole);
    change.target = person.member;
    change.role = role;
    return change;
}

RosterChange grantOf(const DeviceIdentity& person)
{
    RosterChange change = changeOf(RosterChange::Kind::Grant);
    change.person = person.asNewPerson();
    return change;
}

RosterChange deviceChange(RosterChange::Kind kind, const DeviceIdentity& device)
{
    RosterChange change = changeOf(kind);
    change.device = device.asRosterDevice();
    return change;
}

bool askForAccess(TestWard& ward, Device& device)
{
    const ChainTestSupport::TestDevice asking = asTestDevice(device.identity);
    const std::optional<Session> session = ServerTestSupport::signIn(ward.server, asking);
    return session.has_value()
        && ServerTestSupport::signedRequestAccess(ward.server,
                                                  session.value(),
                                                  ward.documentId,
                                                  ServerTestSupport::requestFor(asking, ""))
               .ok();
}

/// Whether the server refuses this device on this ward.
bool refused(TestWard& ward, Device& device, const QString& documentId)
{
    const std::optional<Session> session = ServerTestSupport::signIn(ward.server, asTestDevice(device.identity));
    return session.has_value()
        && ServerTestSupport::wait<Head>([&](Reply<Head> reply) { ward.server.head(session.value(), documentId, reply); })
                   .error
               == ServerError::NotAllowListed;
}

} // namespace

void RosterChangeQueueTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void RosterChangeQueueTest::aChangeReadsBackFromItsRecordExactly()
{
    const DeviceIdentity bob = newIdentity("Bob", "Phone");

    const QList<RosterChange> changes = {grantOf(bob),
                                         deviceChange(RosterChange::Kind::Link, bob),
                                         deviceChange(RosterChange::Kind::Unlink, bob),
                                         removal(bob),
                                         roleOf(bob, RosterPerson::Role::Admin),
                                         changeOf(RosterChange::Kind::Leave)};
    for (const RosterChange& change : changes)
    {
        const std::optional<RosterChange> back = RosterChange::fromJson(change.toJson());
        QVERIFY(back.has_value());
        QVERIFY(back.value() == change);
    }

    // A field its kind does not use, or one it needs missing, is refused.
    QJsonObject extra = removal(bob).toJson();
    extra["device"] = bob.asRosterDevice().toJson();
    QVERIFY(!RosterChange::fromJson(extra).has_value());

    QJsonObject missing = roleOf(bob, RosterPerson::Role::Member).toJson();
    missing.remove("role");
    QVERIFY(!RosterChange::fromJson(missing).has_value());
}

void RosterChangeQueueTest::anUnlinkReachesEveryWardAndTheOpenSessionCatchesUp()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));

    QStringList wards;
    std::unique_ptr<DocumentSession> open;
    for (int count = 0; count < 3; ++count)
    {
        std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
        QVERIFY(session);
        wards.append(ward.documentId);
        if (count == 0)
        {
            open = std::move(session);
        }
    }
    QVERIFY(ward.pair(phone, laptop));

    std::unique_ptr<RosterChangeQueue> queue = laptop.queue();
    Reports reports;
    reports.watch(*queue);
    queue->setOpenSession(open.get());

    queue->enqueueForEveryWard(deviceChange(RosterChange::Kind::Link, phone.identity), wards);
    ward.settle();
    QCOMPARE(reports.outcomes.count(RosterChangeOutcome::Published), qsizetype(3));
    QVERIFY(open->header().entryFor(phone.identity.fingerprint()).has_value());

    queue->enqueueForEveryWard(deviceChange(RosterChange::Kind::Unlink, phone.identity), wards);
    ward.settle();

    QCOMPARE(reports.outcomes.count(RosterChangeOutcome::Published), qsizetype(6));
    for (const QString& documentId : wards)
    {
        QVERIFY(queue->pending(documentId).isEmpty());
        QVERIFY(refused(ward, phone, documentId));
        QVERIFY(!refused(ward, laptop, documentId));
    }

    QVERIFY(!open->header().entryFor(phone.identity.fingerprint()).has_value());
    QVERIFY(!open->document().accessRoster().deviceFor(phone.identity.fingerprint()).has_value());
    QCOMPARE(open->header().keyGeneration, 2);
}

void RosterChangeQueueTest::aWardWhoseLeaseIsHeldElsewhereStillTakesARemoval()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, bob));

    ward.server.holdLeaseAs(ward.documentId, bob.identity.fingerprint());
    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);
    QVERIFY(refused(ward, bob, ward.documentId));
}

void RosterChangeQueueTest::aChangeTheServerAlreadyReflectsIsAlreadyDone()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, bob));

    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::Published);
    const std::optional<ServerVersion::Fetched> before = ward.serverCopy(alice);

    QVERIFY(ward.change(alice, removal(bob.identity)) == RosterChangeOutcome::AlreadyDone);
    QCOMPARE(ward.serverCopy(alice)->version, before->version);
    QVERIFY(alice.records.record(ward.documentId).pendingChanges.isEmpty());
}

void RosterChangeQueueTest::aRemovalByAnAdminDemotedMeanwhileNoLongerApplies()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, bob));
    QVERIFY(ward.grant(alice, carol));
    QVERIFY(ward.change(alice, roleOf(carol.identity, RosterPerson::Role::Admin)) == RosterChangeOutcome::Published);

    // Carol, an admin when she decided, is a member by the time it publishes.
    QVERIFY(ward.change(alice, roleOf(carol.identity, RosterPerson::Role::Member)) == RosterChangeOutcome::Published);
    const qint64 before = ward.serverCopy(alice)->version;

    std::unique_ptr<RosterChangeQueue> queue = carol.queue();
    Reports reports;
    reports.watch(*queue);
    queue->enqueue(ward.documentId, removal(bob.identity));
    ward.settle();

    QCOMPARE(reports.outcomes, QList<RosterChangeOutcome>{RosterChangeOutcome::NoLongerApplies});
    QVERIFY(carol.records.record(ward.documentId).pendingChanges.isEmpty());
    QVERIFY(!carol.queueTimers->isRunning(Timer::Retry));
    QCOMPARE(ward.serverCopy(alice)->version, before);
    QVERIFY(!refused(ward, bob, ward.documentId));
}

void RosterChangeQueueTest::aLeaveWhoseReplyWasLostFinishesAsAlreadyDoneAndForgetsTheWard()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, bob));

    std::unique_ptr<RosterChangeQueue> queue = bob.queue();
    Reports reports;
    reports.watch(*queue);

    // The leave's fetch answers; its upload is accepted and the answer is lost.
    ward.server.holdReplies(true);
    queue->enqueue(ward.documentId, changeOf(RosterChange::Kind::Leave));
    ward.server.loseReplies(true);
    QVERIFY(ward.server.deliverNext());
    ward.server.loseReplies(false);
    ward.server.holdReplies(false);
    ward.settle();

    QVERIFY(reports.outcomes.isEmpty());
    QVERIFY(bob.queueTimers->isRunning(Timer::Retry));
    QVERIFY(refused(ward, bob, ward.documentId));

    QVERIFY(bob.queueTimers->fire(Timer::Retry));
    ward.settle();

    QCOMPARE(reports.outcomes, QList<RosterChangeOutcome>{RosterChangeOutcome::AlreadyDone});
    QCOMPARE(reports.removed, QStringList{ward.documentId});
    QVERIFY(reports.left);
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).file));
    QVERIFY(!QFile::exists(bob.paths(ward.documentId).base));
    QVERIFY(!bob.records.contains(ward.documentId));
}

void RosterChangeQueueTest::aPendingChangeSurvivesRecreatingTheQueue()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, bob));

    const RosterChange remove = removal(bob.identity);
    ward.server.setOffline(true);
    std::unique_ptr<RosterChangeQueue> queue = alice.queue();
    queue->enqueue(ward.documentId, remove);
    ward.settle();
    QVERIFY(alice.queueTimers->isRunning(Timer::Retry));
    QCOMPARE(queue->pending(ward.documentId), QList<RosterChange>{remove});
    queue.reset();

    ward.server.setOffline(false);
    queue = alice.queue();
    Reports reports;
    reports.watch(*queue);
    QCOMPARE(queue->pending(ward.documentId), QList<RosterChange>{remove});
    ward.settle();

    QCOMPARE(reports.outcomes, QList<RosterChangeOutcome>{RosterChangeOutcome::Published});
    QVERIFY(queue->pending(ward.documentId).isEmpty());
    QVERIFY(refused(ward, bob, ward.documentId));
}

void RosterChangeQueueTest::unpublishedDirectoryChangesSurviveARosterChange()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);

    ward.server.setOffline(true);
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 61.0));
    session.reset();
    ward.server.setOffline(false);

    QVERIFY(ward.grant(alice, carol));

    session = alice.open(ward.documentId);
    QVERIFY(session);
    ward.settle();

    QVERIFY(latitudeOf(session->document(), ward.adams) == 61.0);
    QVERIFY(session->document().accessRoster().contains(carol.identity.member));
    QCOMPARE(session->syncState(), SyncState::UpToDate);

    const ServerVersion::Fetched copy = ward.serverCopy(alice).value();
    QVERIFY(latitudeOf(copy.document, ward.adams) == 61.0);
    QVERIFY(copy.document.accessRoster().contains(carol.identity.member));
}

void RosterChangeQueueTest::twoAdminsGrantingAtOnceBothEndUpWithTheirPerson()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    Device& dan = ward.add(newIdentity("Dan", "Phone"));
    Device& erin = ward.add(newIdentity("Erin", "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    QVERIFY(ward.grant(alice, carol));
    QVERIFY(ward.change(alice, roleOf(carol.identity, RosterPerson::Role::Admin)) == RosterChangeOutcome::Published);
    QVERIFY(askForAccess(ward, dan));
    QVERIFY(askForAccess(ward, erin));

    std::unique_ptr<RosterChangeQueue> aliceQueue = alice.queue();
    std::unique_ptr<RosterChangeQueue> carolQueue = carol.queue();
    Reports aliceReports;
    aliceReports.watch(*aliceQueue);
    Reports carolReports;
    carolReports.watch(*carolQueue);

    // Both fetch the same version, so one upload is refused.
    ward.server.holdReplies(true);
    aliceQueue->enqueue(ward.documentId, grantOf(dan.identity));
    carolQueue->enqueue(ward.documentId, grantOf(erin.identity));
    ward.server.holdReplies(false);
    ward.settle();

    const bool aliceRetries = alice.queueTimers->isRunning(Timer::Retry);
    const bool carolRetries = carol.queueTimers->isRunning(Timer::Retry);
    QVERIFY(aliceRetries != carolRetries);
    QVERIFY((aliceRetries ? alice : carol).queueTimers->fire(Timer::Retry));
    ward.settle();

    QCOMPARE(aliceReports.outcomes, QList<RosterChangeOutcome>{RosterChangeOutcome::Published});
    QCOMPARE(carolReports.outcomes, QList<RosterChangeOutcome>{RosterChangeOutcome::Published});
    const AccessRoster roster = ward.serverCopy(alice)->document.accessRoster();
    QVERIFY(roster.contains(dan.identity.member));
    QVERIFY(roster.contains(erin.identity.member));
}

void RosterChangeQueueTest::anUnlinkOfADeviceWhoseLinkIsStillPendingDropsBoth()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    const DeviceIdentity phone = newDeviceOf(laptop.identity, "Phone");
    const QString documentId = ChainTestSupport::newDocumentId();
    QVERIFY(laptop.create(documentId));

    std::unique_ptr<RosterChangeQueue> queue = laptop.queue();
    queue->enqueue(documentId, deviceChange(RosterChange::Kind::Link, phone));
    QCOMPARE(queue->pending(documentId).size(), qsizetype(1));

    queue->enqueue(documentId, deviceChange(RosterChange::Kind::Unlink, phone));
    QVERIFY(queue->pending(documentId).isEmpty());
    QVERIFY(laptop.records.record(documentId).pendingChanges.isEmpty());
}
