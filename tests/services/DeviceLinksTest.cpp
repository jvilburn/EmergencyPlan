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

bool sealsTo(TestWard& ward, Device& reader, const DeviceIdentity& device)
{
    const std::optional<ServerVersion::Fetched> copy = ward.serverCopy(reader);
    return copy.has_value() && copy.value().sealed.header.entryFor(device.fingerprint()).has_value();
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
    std::unique_ptr<DocumentSession> session = ward.createShared(laptop);
    QVERIFY(session);
    session.reset();

    std::unique_ptr<SealingQueue> queue = laptop.queue();
    DeviceLinks links(laptop.records, laptop.connection, *queue, nullptr);
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
    QVERIFY(sealsTo(ward, laptop, phone.identity));
}

void DeviceLinksTest::anUnknownCodeFails()
{
    TestWard ward;
    Device& laptop = ward.add(newIdentity("Alice", "Laptop"));
    QVERIFY(ward.sessionFor(laptop).has_value());
    std::unique_ptr<SealingQueue> queue = laptop.queue();
    DeviceLinks links(laptop.records, laptop.connection, *queue, nullptr);
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

    std::unique_ptr<SealingQueue> queue = laptop.queue();
    DeviceLinks links(laptop.records, laptop.connection, *queue, nullptr);
    QSignalSpy unlinked(&links, &DeviceLinks::unlinked);

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
