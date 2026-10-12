#include "PendingRequestsTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "PendingRequests.h"
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
    PendingRequests requests;
};

/// The admin's grant of Bob's one pending request, on the server alone.
bool grantOnServer(TestWard& ward, Device& admin)
{
    const Session session = ward.sessionFor(admin).value();
    const ServerContract::Result<QList<ServerContract::AccessRequest>> requests =
        ServerTestSupport::wait<QList<ServerContract::AccessRequest>>(
            std::bind(&ServerApi::accessRequests, &ward.server, session, ward.documentId, _1));
    return requests.ok() && requests.value->size() == 1
        && ServerTestSupport::wait<Done>(std::bind(
                   &ServerApi::grant, &ward.server, session, ward.documentId, requests.value->first().index, _1))
               .ok();
}

} // namespace

void PendingRequestsTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void PendingRequestsTest::aGrantedRequestArrivesOnceTheWardIsSealedToThisDevice()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    aliceSession.reset();

    BobsApp app(bob);
    QSignalSpy requested(&app.requests, &PendingRequests::requested);
    QSignalSpy arrived(&app.requests, &PendingRequests::arrived);
    app.requests.request(ward.documentId);
    ward.settle();
    QCOMPARE(requested.count(), 1);
    QCOMPARE(app.requests.pending(), QStringList{ward.documentId});

    // Granted, but not yet sealed to Bob: still waiting.
    QVERIFY(grantOnServer(ward, alice));
    app.requests.checkNow();
    ward.settle();
    QCOMPARE(arrived.count(), 0);
    QCOMPARE(app.requests.pending(), QStringList{ward.documentId});
    QVERIFY(!app.library.contains(ward.documentId));

    // Alice's app seals it to the device that asked.
    DocumentRecord record = alice.records.record(ward.documentId);
    record.granted.append(bob.identity.keys());
    alice.records.setRecord(ward.documentId, record);
    ward.seal(alice);

    app.requests.checkNow();
    ward.settle();
    QCOMPARE(arrived.count(), 1);
    QCOMPARE(arrived.first().first().toString(), ward.documentId);
    QVERIFY(app.requests.pending().isEmpty());
    QVERIFY(app.library.contains(ward.documentId));
}

void PendingRequestsTest::aDeclinedRequestIsDroppedAndSaysSo()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);

    BobsApp app(bob);
    QSignalSpy declined(&app.requests, &PendingRequests::declined);
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

void PendingRequestsTest::aRequestOutlivesTheApp()
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
        PendingRequests requests(*settings, library, bob.connection, nullptr);
        requests.request(ward.documentId);
        ward.settle();
    }

    std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), bob.identity);
    PendingRequests requests(*settings, library, bob.connection, nullptr);
    QCOMPARE(requests.pending(), QStringList{ward.documentId});
}

void PendingRequestsTest::aRequestForNoSuchWardFails()
{
    TestWard ward;
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    BobsApp app(bob);
    QSignalSpy failed(&app.requests, &PendingRequests::requestFailed);

    app.requests.request(ServerTestSupport::newDocumentId());
    ward.settle();

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(1).value<ServerContract::ServerError>(), ServerContract::ServerError::UnknownDocument);
    QVERIFY(app.requests.pending().isEmpty());
}
