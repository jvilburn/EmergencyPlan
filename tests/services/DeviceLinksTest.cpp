#include "DeviceLinksTest.h"

#include "DeviceLinks.h"
#include "DocumentCrypto.h"
#include "SessionTestSupport.h"

#include <QSignalSpy>
#include <QtTest>

using ServerContract::Head;
using ServerContract::LinkState;
using ServerContract::ServerApi;
using ServerContract::Session;
using namespace SessionTestSupport;
using namespace std::placeholders;

namespace
{

/// The code the new device shows, as its first screen asks for one.
QString codeFor(TestWard& ward, Device& newDevice)
{
    const std::optional<Session> session = ServerTestSupport::signIn(ward.server, asTestDevice(newDevice.identity));
    return ServerTestSupport::wait<QString>(std::bind(&ServerApi::startLink,
                                                      &ward.server,
                                                      session.value(),
                                                      newDevice.identity.sealing.publicKey,
                                                      newDevice.identity.deviceName,
                                                      _1))
        .value.value();
}

bool sealsTo(TestWard& ward, Device& reader, const QString& documentId, const DeviceIdentity& device)
{
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader, documentId);
    return copy.has_value() && copy.value().sealed.header.entryFor(device.fingerprint()).has_value();
}

qint64 versionOf(TestWard& ward, Device& reader, const QString& documentId)
{
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader, documentId);
    return copy.has_value() ? copy.value().version : -1;
}

/// The laptop's two shared wards, closed: the first's id, and the second's, which
/// the test ward is left on.
bool twoWardsOn(TestWard& ward, Device& laptop, QString& first, QString& second)
{
    if (!ward.createShared(laptop))
    {
        return false;
    }
    first = ward.documentId;
    if (!ward.createShared(laptop))
    {
        return false;
    }
    second = ward.documentId;
    return true;
}

/// Reads the code the new device shows and approves it with the keys it names.
void readAndApprove(TestWard& ward, DeviceLinks& links, const QString& code)
{
    QSignalSpy read(&links, &DeviceLinks::linkRead);
    links.read(code);
    ward.settle();
    links.approve(read.first().first().toString(), read.first().at(1).value<LinkState>().keys);
    ward.settle();
}

} // namespace

void DeviceLinksTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DeviceLinksTest::approvingALinkRemembersTheDeviceAndSealsItEverywhere()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    QString first;
    QString second;
    QVERIFY(twoWardsOn(ward, laptop, first, second));

    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy read(&links, &DeviceLinks::linkRead);
    QSignalSpy approved(&links, &DeviceLinks::approved);

    const QString code = codeFor(ward, phone);
    links.read(code.toLower() + " ");
    ward.settle();
    QCOMPARE(read.count(), 1);
    const LinkState state = read.first().at(1).value<LinkState>();
    QCOMPARE(state.deviceName, QString("Phone"));
    QCOMPARE(state.keys, phone.identity.keys());

    links.approve(read.first().first().toString(), state.keys);
    ward.settle();
    QCOMPARE(approved.count(), 1);
    QCOMPARE(approved.first().first().toString(), QString("Phone"));
    QCOMPARE(laptop.records.linkedDevices(), QList<DeviceKeys>{phone.identity.keys()});

    // Each ward's version 1, from its share, resealed to the phone as version 2.
    for (const QString& documentId : {first, second})
    {
        QCOMPARE(versionOf(ward, laptop, documentId), qint64(2));
        QVERIFY(sealsTo(ward, laptop, documentId, phone.identity));
        QCOMPARE(ward.download(phone, documentId),
                 std::optional<DocumentSession::Download>(DocumentSession::Download::Done));
        std::unique_ptr<DocumentSession> opened = phone.open(documentId);
        QVERIFY(opened);
        QVERIFY(opened->document() == ward.serverCopy(phone, documentId)->document);
    }
}

void DeviceLinksTest::aWardTheApproverCouldNotReadIsLeftOutAndArrivesWithItsNextWrite()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    QString first;
    QString second;
    QVERIFY(twoWardsOn(ward, laptop, first, second));
    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy approved(&links, &DeviceLinks::approved);
    QSignalSpy failed(&links, &DeviceLinks::failed);

    // The second ward's version is one the laptop cannot open.
    const std::optional<ServerVersion::Fetched> stored = ward.serverCopy(laptop, second);
    QVERIFY(stored.has_value());
    ward.server.serveInstead(second, stored->sealed.header.toJson(), "garbage");

    readAndApprove(ward, links, codeFor(ward, phone));
    QCOMPARE(approved.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(laptop.records.linkedDevices(), QList<DeviceKeys>{phone.identity.keys()});
    QCOMPARE(versionOf(ward, laptop, first), qint64(2));
    QVERIFY(sealsTo(ward, laptop, first, phone.identity));

    // Served as stored again: the second ward is still version 1, sealed to the
    // laptop alone, so the phone waits for it.
    ward.server.serveInstead(second, QJsonObject(), QByteArray());
    QCOMPARE(versionOf(ward, laptop, second), qint64(1));
    QVERIFY(!sealsTo(ward, laptop, second, phone.identity));
    QCOMPARE(ward.download(phone, second),
             std::optional<DocumentSession::Download>(DocumentSession::Download::ServerFault));

    // The laptop's next write of it seals to the phone it linked: opening it saves
    // it again resealed, as version 2, and an edit after goes up as version 3.
    std::unique_ptr<DocumentSession> session = laptop.open(second);
    QVERIFY(session);
    ward.settle();
    QCOMPARE(versionOf(ward, laptop, second), qint64(2));
    QVERIFY(sealsTo(ward, laptop, second, phone.identity));
    session->edit(DocumentSession::Edit::Directory, moveFamily(ward.adams, 45.0));
    QVERIFY(laptop.timers->fire(SessionTimers::Timer::Save));
    ward.settle();
    QCOMPARE(versionOf(ward, laptop, second), qint64(3));
    QVERIFY(sealsTo(ward, laptop, second, phone.identity));
    QCOMPARE(ward.download(phone, second),
             std::optional<DocumentSession::Download>(DocumentSession::Download::Done));
    std::unique_ptr<DocumentSession> phoneSession = phone.open(second);
    QVERIFY(phoneSession);
    QCOMPARE(latitudeOf(phoneSession->document(), ward.adams), std::optional<double>(45.0));
}

void DeviceLinksTest::anApprovalRetriesOnceOnAConflict()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    QString first;
    QString second;
    QVERIFY(twoWardsOn(ward, laptop, first, second));
    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy approved(&links, &DeviceLinks::approved);
    QSignalSpy failed(&links, &DeviceLinks::failed);

    // The first approval is refused as a conflict on one ward, and neither ward
    // moves; the second, built again, lands both.
    ward.server.forceConflict(second);
    readAndApprove(ward, links, codeFor(ward, phone));

    QCOMPARE(approved.count(), 1);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(laptop.records.linkedDevices(), QList<DeviceKeys>{phone.identity.keys()});
    for (const QString& documentId : {first, second})
    {
        QCOMPARE(versionOf(ward, laptop, documentId), qint64(2));
        QVERIFY(sealsTo(ward, laptop, documentId, phone.identity));
    }
}

void DeviceLinksTest::anApprovalWithAWardLeftUnansweredDoesNotGoUp()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    QString first;
    QString second;
    QVERIFY(twoWardsOn(ward, laptop, first, second));
    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy approved(&links, &DeviceLinks::approved);
    QSignalSpy failed(&links, &DeviceLinks::failed);
    QSignalSpy read(&links, &DeviceLinks::linkRead);
    const QString code = codeFor(ward, phone);
    links.read(code);
    ward.settle();
    QCOMPARE(read.count(), 1);

    // The connection drops after the code was read: the approval is refused here,
    // rather than going up short of the wards it could not fetch.
    ward.server.setOffline(true);
    links.approve(code, read.first().at(1).value<LinkState>().keys);
    ward.settle();
    QCOMPARE(approved.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().first().value<ServerContract::ServerError>(), ServerContract::ServerError::Unreachable);
    QVERIFY(laptop.records.linkedDevices().isEmpty());
    ward.server.setOffline(false);
    for (const QString& documentId : {first, second})
    {
        QCOMPARE(versionOf(ward, laptop, documentId), qint64(1));
        QVERIFY(!sealsTo(ward, laptop, documentId, phone.identity));
    }

    // Back online, the same code approves and both wards go up.
    links.approve(code, read.first().at(1).value<LinkState>().keys);
    ward.settle();
    QCOMPARE(approved.count(), 1);
    QCOMPARE(laptop.records.linkedDevices(), QList<DeviceKeys>{phone.identity.keys()});
    for (const QString& documentId : {first, second})
    {
        QCOMPARE(versionOf(ward, laptop, documentId), qint64(2));
        QVERIFY(sealsTo(ward, laptop, documentId, phone.identity));
    }
}

void DeviceLinksTest::anUnknownCodeFails()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    QVERIFY(ward.sessionFor(laptop).has_value());
    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy failed(&links, &DeviceLinks::failed);

    links.read("NOSUCHCO");
    ward.settle();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().first().value<ServerContract::ServerError>(), ServerContract::ServerError::NotFound);
}

void DeviceLinksTest::unlinkingDropsTheDeviceAndCutsItOff()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    Device& phone = ward.add(newDeviceOf(laptop.identity, "Phone"));
    Device& tablet = ward.add(newDeviceOf(laptop.identity, "Tablet"));
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();
    QVERIFY(ward.link(phone, laptop));
    QVERIFY(ward.link(tablet, laptop));

    DeviceLinks links(laptop.identity,
                      laptop.records,
                      laptop.connection,
                      std::bind(&Device::paths, &laptop, _1),
                      nullptr);
    QSignalSpy unlinked(&links, &DeviceLinks::unlinked);
    QCOMPARE(laptop.records.linkedDevices().size(), 2);

    links.unlink({phone.identity.fingerprint(), tablet.identity.fingerprint()});
    ward.settle();
    QCOMPARE(unlinked.count(), 1);
    QCOMPARE(unlinked.first().first().toInt(), 2);
    QVERIFY(laptop.records.linkedDevices().isEmpty());

    const Session phoneSession = ServerTestSupport::signIn(ward.server, asTestDevice(phone.identity)).value();
    const ServerContract::Result<Head> refused =
        ServerTestSupport::wait<Head>(std::bind(&ServerApi::head, &ward.server, phoneSession, ward.documentId, _1));
    QCOMPARE(refused.error, ServerContract::ServerError::AccessLost);
    QCOMPARE(refused.lost->how, ServerContract::LostAccess::How::Unlinked);
    QCOMPARE(refused.lost->byWhom, QString("Laptop"));
}
