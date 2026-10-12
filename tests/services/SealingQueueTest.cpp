#include "SealingQueueTest.h"

#include "DocumentCrypto.h"
#include "ServerTestSupport.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QtTest>

using ServerContract::Done;
using ServerContract::Head;
using ServerContract::LostAccess;
using ServerContract::Reply;
using ServerContract::ServerApi;
using ServerContract::Session;
using namespace SessionTestSupport;
using namespace std::placeholders;

using Timer = SessionTimers::Timer;

namespace
{

void keepLost(QStringList& out, QList<LostAccess>& reasons, const QString& documentId, const LostAccess& lost)
{
    out.append(documentId);
    reasons.append(lost);
}

void keepDownload(std::optional<DocumentSession::Download>& out, DocumentSession::Download download)
{
    out = download;
}

std::optional<DocumentSession::Download> downloadTo(TestWard& ward, Device& device, const QString& documentId)
{
    QObject receiver;
    std::optional<DocumentSession::Download> result;
    DocumentSession::download(device.context(),
                              documentId,
                              device.paths(documentId),
                              &receiver,
                              std::bind(&keepDownload, std::ref(result), _1));
    ward.settle();
    return result;
}

/// Whether the server's current version of a ward seals to a device.
bool sealsTo(TestWard& ward, Device& reader, const QString& documentId, const DeviceIdentity& device)
{
    const QString previous = ward.documentId;
    ward.documentId = documentId;
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader);
    ward.documentId = previous;
    return copy.has_value() && copy.value().sealed.header.entryFor(device.fingerprint()).has_value();
}

qint64 versionOf(TestWard& ward, Device& reader, const QString& documentId)
{
    const QString previous = ward.documentId;
    ward.documentId = documentId;
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader);
    ward.documentId = previous;
    return copy.has_value() ? copy.value().version : -1;
}

} // namespace

void SealingQueueTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void SealingQueueTest::aLinkSealsTheNewDeviceIntoEveryWard()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));

    // Two wards: the first left closed, the second open in the window.
    std::unique_ptr<DocumentSession> first = ward.createShared(laptop);
    QVERIFY(first);
    const QString closedWard = ward.documentId;
    first.reset();
    std::unique_ptr<DocumentSession> open = ward.createShared(laptop);
    QVERIFY(open);
    const QString openWard = ward.documentId;

    QVERIFY(ward.link(phone, laptop));
    QVERIFY(!sealsTo(ward, laptop, closedWard, phone.identity));
    QVERIFY(!sealsTo(ward, laptop, openWard, phone.identity));

    std::unique_ptr<SealingQueue> queue = laptop.queue();
    queue->setOpenSession(open.get());
    queue->sealEverywhere();
    ward.settle();

    QCOMPARE(queue->pendingCount(), 0);
    QVERIFY(sealsTo(ward, laptop, closedWard, phone.identity));
    QVERIFY(sealsTo(ward, laptop, openWard, phone.identity));

    // The open ward went through its session, which is current with what it sent.
    QCOMPARE(open->syncState(), SyncState::UpToDate);
    QVERIFY(open->header().entryFor(phone.identity.fingerprint()).has_value());
    QVERIFY(downloadTo(ward, phone, closedWard) == DocumentSession::Download::Done);
    QVERIFY(downloadTo(ward, phone, openWard) == DocumentSession::Download::Done);

    // The closed ward's local file catches up when it next opens.
    std::unique_ptr<DocumentSession> reopened = laptop.open(closedWard);
    QVERIFY(reopened);
    ward.documentId = closedWard;
    ward.settle();
    QVERIFY(reopened->header().entryFor(phone.identity.fingerprint()).has_value());
}

void SealingQueueTest::aWardGrantedAfterALinkIsSealedTooOnTheNextPass()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Laptop"));
    Device& bobsPhone = ward.add(newDeviceOf(bob.identity, "Phone"));
    QVERIFY(ward.sessionFor(bob).has_value());
    QVERIFY(ward.link(bobsPhone, bob));

    // Alice grants Bob's request, made from his laptop, and seals the laptop alone.
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    QVERIFY(sealsTo(ward, alice, ward.documentId, bob.identity));
    QVERIFY(!sealsTo(ward, alice, ward.documentId, bobsPhone.identity));

    ward.seal(bob);

    QVERIFY(sealsTo(ward, alice, ward.documentId, bobsPhone.identity));
    QVERIFY(downloadTo(ward, bobsPhone, ward.documentId) == DocumentSession::Download::Done);
}

void SealingQueueTest::aGrantSealsTheDeviceThatAskedAndLeavesTheRecord()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    aliceSession.reset();

    QVERIFY(ward.grant(alice, bob));

    QVERIFY(sealsTo(ward, alice, ward.documentId, bob.identity));
    QVERIFY(alice.records.record(ward.documentId).granted.isEmpty());
    QCOMPARE(versionOf(ward, alice, ward.documentId), qint64(2));
}

void SealingQueueTest::aLeaseHeldElsewhereHoldsNothingOff()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();
    QVERIFY(ward.link(phone, laptop));

    ward.server.holdLeaseAs(ward.documentId, ServerTestSupport::newPerson("Someone").fingerprint());
    ward.seal(laptop);

    QVERIFY(sealsTo(ward, laptop, ward.documentId, phone.identity));
}

void SealingQueueTest::anUnreachableServerIsRetriedUntilEveryWardIsDone()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();
    QVERIFY(ward.link(phone, laptop));

    std::unique_ptr<SealingQueue> queue = laptop.queue();
    ward.server.setOffline(true);
    queue->sealEverywhere();
    ward.settle();
    QCOMPARE(queue->pendingCount(), 1);
    QCOMPARE(laptop.queueTimers->interval(Timer::Retry), SessionTimers::FirstRetryMilliseconds);

    QVERIFY(laptop.queueTimers->fire(Timer::Retry));
    ward.settle();
    QCOMPARE(laptop.queueTimers->interval(Timer::Retry), 2 * SessionTimers::FirstRetryMilliseconds);

    ward.server.setOffline(false);
    QVERIFY(laptop.queueTimers->fire(Timer::Retry));
    ward.settle();
    QCOMPARE(queue->pendingCount(), 0);
    QVERIFY(sealsTo(ward, laptop, ward.documentId, phone.identity));
}

void SealingQueueTest::aConflictStartsTheWardAgainOnTheNewerVersion()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();
    QVERIFY(ward.link(phone, laptop));
    const qint64 before = versionOf(ward, laptop, ward.documentId);

    ward.server.forceConflict(ward.documentId);
    ward.seal(laptop);

    // The first upload refused, the second, built afresh, accepted: one new version.
    QVERIFY(sealsTo(ward, laptop, ward.documentId, phone.identity));
    QCOMPARE(versionOf(ward, laptop, ward.documentId), before + 1);
}

void SealingQueueTest::aWardThisDeviceLostIsForgotten()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));
    const DocumentPaths paths = bob.paths(ward.documentId);
    QVERIFY(QFile::exists(paths.file));

    const Session admin = ward.sessionFor(alice).value();
    QVERIFY(ServerTestSupport::wait<Done>(
                std::bind(&ServerApi::removeMember, &ward.server, admin, ward.documentId, bob.identity.member, _1))
                .ok());

    std::unique_ptr<SealingQueue> queue = bob.queue();
    QStringList lost;
    QList<LostAccess> reasons;
    QObject::connect(queue.get(),
                     &SealingQueue::accessLost,
                     queue.get(),
                     std::bind(&keepLost, std::ref(lost), std::ref(reasons), _1, _2));
    queue->sealEverywhere();
    ward.settle();

    QCOMPARE(lost, QStringList{ward.documentId});
    QCOMPARE(reasons.first().how, LostAccess::How::Removed);
    QCOMPARE(reasons.first().byWhom, QString("Alice"));
    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!bob.records.contains(ward.documentId));
    QCOMPARE(queue->pendingCount(), 0);
}

void SealingQueueTest::aWardOwingNothingIsLeftUntouched()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    std::unique_ptr<DocumentSession> session = ward.createShared(alice);
    QVERIFY(session);
    session.reset();
    const qint64 before = versionOf(ward, alice, ward.documentId);

    ward.seal(alice);

    QCOMPARE(versionOf(ward, alice, ward.documentId), before);
}
