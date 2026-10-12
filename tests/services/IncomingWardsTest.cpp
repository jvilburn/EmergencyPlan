#include "IncomingWardsTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "IncomingWards.h"
#include "SessionTestSupport.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using ServerContract::Done;
using ServerContract::ServerApi;
using ServerContract::Session;
using namespace SessionTestSupport;
using namespace std::placeholders;

namespace
{

/// Bob's app: his settings and library in a folder of their own, over his connection.
struct BobsApp
{
    explicit BobsApp(Device& bob)
        : settings(AppSettings::open(folder.filePath("settings.json"), nullptr))
        , library(*settings, folder.filePath("wards"), bob.identity)
        , requests(*settings, library, bob.connection, nullptr)
    {
    }

    QTemporaryDir folder;
    std::unique_ptr<AppSettings> settings;
    DocumentLibrary library;
    IncomingWards requests;
};

/// The admin's grant of one pending request, as the admin's app makes it: the ward's
/// current version, resealed on the admin's device to the device that asked, goes
/// up with it.
bool grantOnServer(TestWard& ward, Device& admin)
{
    const Session session = ward.sessionFor(admin).value();
    const ServerContract::Result<QList<ServerContract::AccessRequest>> requests =
        ServerTestSupport::wait<QList<ServerContract::AccessRequest>>(
            std::bind(&ServerApi::accessRequests, &ward.server, session, ward.documentId, _1));
    if (!requests.ok() || requests.value->size() != 1)
    {
        return false;
    }
    const ServerContract::AccessRequest& request = requests.value->first();
    const std::optional<ServerContract::Resealing> resealing =
        ward.resealing(admin, ward.documentId, request.person.devices.first().keys);
    return resealing.has_value()
        && ServerTestSupport::wait<Done>(std::bind(&ServerApi::grant,
                                                   &ward.server,
                                                   session,
                                                   ward.documentId,
                                                   request.index,
                                                   resealing.value(),
                                                   _1))
               .ok();
}

} // namespace

void IncomingWardsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void IncomingWardsTest::aGrantedRequestArrivesAtOnce()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    aliceSession.reset();

    BobsApp app(bob);
    QSignalSpy requested(&app.requests, &IncomingWards::requested);
    QSignalSpy arrived(&app.requests, &IncomingWards::fetched);
    app.requests.request(ward.documentId);
    ward.settle();
    QCOMPARE(requested.count(), 1);
    QCOMPARE(app.requests.pending(), QStringList{ward.documentId});

    QVERIFY(!app.library.contains(ward.documentId));

    // Granted with a version sealed to Bob: one check drops the request and fetches
    // the ward, with no wait for access between.
    QVERIFY(grantOnServer(ward, alice));
    app.requests.checkNow();
    ward.settle();
    QVERIFY(app.requests.pending().isEmpty());
    QCOMPARE(arrived.count(), 1);
    QCOMPARE(arrived.first().first().toString(), ward.documentId);
    QCOMPARE(arrived.first().at(1).value<DocumentSession::Download>(), DocumentSession::Download::Done);
    QVERIFY(app.requests.waiting().isEmpty());
    QVERIFY(app.library.contains(ward.documentId));
}

void IncomingWardsTest::aDeclinedRequestIsDroppedAndSaysSo()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);

    BobsApp app(bob);
    QSignalSpy declined(&app.requests, &IncomingWards::declined);
    app.requests.request(ward.documentId);
    ward.settle();

    const Session session = ward.sessionFor(alice).value();
    QVERIFY(ServerTestSupport::wait<Done>(
                std::bind(&ServerApi::declineRequest, &ward.server, session, ward.documentId, 1, _1))
                .ok());

    app.requests.checkNow();
    ward.settle();
    QCOMPARE(declined.count(), 1);
    QVERIFY(app.requests.pending().isEmpty());
    QVERIFY(!app.library.contains(ward.documentId));
}

void IncomingWardsTest::aRequestOutlivesTheApp()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    {
        std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
        DocumentLibrary library(*settings, folder.filePath("wards"), bob.identity);
        IncomingWards requests(*settings, library, bob.connection, nullptr);
        requests.request(ward.documentId);
        ward.settle();
    }

    std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), bob.identity);
    IncomingWards requests(*settings, library, bob.connection, nullptr);
    QCOMPARE(requests.pending(), QStringList{ward.documentId});
}

void IncomingWardsTest::aRequestForNoSuchWardFails()
{
    TestWard ward;
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    BobsApp app(bob);
    QSignalSpy failed(&app.requests, &IncomingWards::requestFailed);

    app.requests.request(ServerTestSupport::newDocumentId());
    ward.settle();

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(1).value<ServerContract::ServerError>(), ServerContract::ServerError::UnknownDocument);
    QVERIFY(app.requests.pending().isEmpty());
}

void IncomingWardsTest::aRequestTheServerNoLongerHasIsDropped()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    aliceSession.reset();

    BobsApp app(bob);
    QSignalSpy declined(&app.requests, &IncomingWards::declined);
    app.requests.request(ward.documentId);
    ward.settle();
    QCOMPARE(app.requests.pending(), QStringList{ward.documentId});

    // The last person leaves, and the ward's requests go with her.
    const Session session = ward.sessionFor(alice).value();
    QVERIFY(ServerTestSupport::wait<Done>(std::bind(
                &ServerApi::removeMember, &ward.server, session, ward.documentId, alice.identity.member, _1))
                .ok());

    app.requests.checkNow();
    ward.settle();
    QVERIFY(app.requests.pending().isEmpty());
    QCOMPARE(declined.count(), 0);
}

void IncomingWardsTest::aLinkedDevicesWardsArriveAtOnce()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();
    QVERIFY(ward.link(phone, laptop));

    // The ward the laptop held went up with the approval sealed to the phone.
    BobsApp app(phone);
    QSignalSpy fetched(&app.requests, &IncomingWards::fetched);
    app.requests.checkNow();
    ward.settle();
    QCOMPARE(fetched.count(), 1);
    QCOMPARE(fetched.first().first().toString(), ward.documentId);
    QCOMPARE(fetched.first().at(1).value<DocumentSession::Download>(), DocumentSession::Download::Done);
    QVERIFY(app.library.contains(ward.documentId));
    QVERIFY(app.requests.waiting().isEmpty());
    QVERIFY(app.requests.pending().isEmpty());
}

void IncomingWardsTest::aWardTheLinkerLackedWaitsForItsNextWrite()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    Device& carol = ward.add(newIdentity("Carol", "Tablet"));
    std::unique_ptr<DocumentSession> laptopWard = ward.createShared(laptop);
    QVERIFY(laptopWard);
    laptopWard.reset();
    const QString held = ward.documentId;
    QVERIFY(ward.link(phone, laptop));

    // After the link, Carol shares a ward and grants the laptop, which asked: the
    // version that goes up with the grant seals to the laptop, not the phone.
    std::unique_ptr<DocumentSession> carolsWard = ward.createShared(carol);
    QVERIFY(carolsWard);
    carolsWard.reset();
    const QString lacked = ward.documentId;
    QVERIFY(ward.grant(carol, laptop));

    BobsApp app(phone);
    QSignalSpy fetched(&app.requests, &IncomingWards::fetched);
    app.requests.checkNow();
    ward.settle();
    QCOMPARE(fetched.count(), 1);
    QCOMPARE(fetched.first().first().toString(), held);
    QVERIFY(app.library.contains(held));
    QCOMPARE(app.requests.waiting(), QStringList{lacked});
    QVERIFY(!app.library.contains(lacked));

    // The laptop opens and edits it; its publish seals to the phone it linked.
    std::unique_ptr<DocumentSession> session = laptop.open(lacked);
    QVERIFY(session);
    ward.settle();
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 45.0));
    QVERIFY(laptop.timers->fire(SessionTimers::Timer::Save));
    ward.settle();

    app.requests.checkNow();
    ward.settle();
    QCOMPARE(fetched.count(), 2);
    QCOMPARE(fetched.at(1).first().toString(), lacked);
    QCOMPARE(fetched.at(1).at(1).value<DocumentSession::Download>(), DocumentSession::Download::Done);
    QVERIFY(app.library.contains(lacked));
    QVERIFY(app.requests.waiting().isEmpty());
}
