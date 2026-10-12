#pragma once

#include "DocumentCrypto.h"
#include "DocumentHeader.h"
#include "ServerContract.h"

#include <QEventLoop>
#include <QList>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

/// Driving a ServerApi the way a client does, for the server test suites.
///
/// Everything here goes through ServerApi alone, so it serves the conformance
/// suite against any server. Ids and keys are fresh every time, so runs can share
/// a real server.
namespace ServerTestSupport
{

/// One device belonging to one person. Several devices may carry the same member
/// id, which is how a person's phone and laptop are expressed.
struct TestDevice
{
    MemberId member;
    QString displayName;
    QString deviceName;
    Keypair sealing;
    SigningKeypair signing;

    Fingerprint fingerprint() const;
    DeviceKeys keys() const { return DeviceKeys{sealing.publicKey, signing.verifyKey}; }
    ServerContract::Registration registration() const;
};

MemberId newMember();
QString newDocumentId();

/// A device belonging to a brand new person.
TestDevice newPerson(const QString& displayName);

/// Another device of the same person, not yet linked.
TestDevice newDeviceOf(const TestDevice& person, const QString& deviceName);

/// Runs call with a reply that stores the result, then spins a QEventLoop until
/// it arrives. Seventy seconds without one is ServerError::Unreachable: past
/// HttpServerApi's own transfer timeout, so a slow real server still answers
/// inside it. Waiting on the event loop serves the fake and a real server alike.
template<typename T>
ServerContract::Result<T> wait(const std::function<void(ServerContract::Reply<T>)>& call)
{
    constexpr int TimeoutMilliseconds = 70 * 1000;

    // Shared with the reply, which may still arrive after a timeout has returned.
    const std::shared_ptr<std::optional<ServerContract::Result<T>>> result =
        std::make_shared<std::optional<ServerContract::Result<T>>>();
    QEventLoop loop;
    QPointer<QEventLoop> waiting(&loop);

    call([result, waiting](const ServerContract::Result<T>& answer)
    {
        *result = answer;
        if (waiting)
        {
            waiting->quit();
        }
    });

    if (!result->has_value())
    {
        QTimer::singleShot(TimeoutMilliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    }

    if (!result->has_value())
    {
        return ServerContract::Result<T>::failure(ServerContract::ServerError::Unreachable);
    }
    return result->value();
}

/// Signs in with a fresh challenge; nullopt when the server refuses.
std::optional<ServerContract::Session> signIn(ServerContract::ServerApi& server, const TestDevice& device);

/// Signs in and registers the device's person, as ServerConnection does.
std::optional<ServerContract::Session> join(ServerContract::ServerApi& server, const TestDevice& device);

/// A header sealing a fresh data key to these devices, and an opaque blob. The
/// server never reads the blob, so it need not be a real payload.
ServerContract::Upload uploadFor(const QString& documentId, const QList<TestDevice>& devices, qint64 saveCounter);

/// The fingerprints the server lists for a ward, from its head.
QStringList allowListOf(const ServerContract::Head& head);

/// One ward on the server, and what the test knows about it.
struct SharedDocument
{
    QString id;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    ServerContract::Session aliceSession;
    ServerContract::Session bobSession;
    ServerContract::Session carolSession;
    qint64 version = 0;
    qint64 saveCounter = 1;
};

/// Alice's ward, registered with only her device.
std::optional<SharedDocument> registerWard(ServerContract::ServerApi& server);

/// Alice's ward, with Bob and Carol granted by request. Alice is the only admin.
std::optional<SharedDocument> wardOfThree(ServerContract::ServerApi& server);

/// Asks for access as device and has admin grant the request.
bool grantByRequest(ServerContract::ServerApi& server,
                    const QString& documentId,
                    const ServerContract::Session& admin,
                    const TestDevice& device,
                    const ServerContract::Session& deviceSession);

/// Links newDevice to the person existing belongs to; the new device's session.
std::optional<ServerContract::Session> link(ServerContract::ServerApi& server,
                                            const TestDevice& newDevice,
                                            const ServerContract::Session& existing);

/// Uploads a version sealed to these devices on top of the ward's current version;
/// on success the ward's version advances.
ServerContract::ServerError publish(ServerContract::ServerApi& server,
                                    SharedDocument& ward,
                                    const ServerContract::Session& session,
                                    const QList<TestDevice>& devices);

/// Moves the server's clock and signs everyone in again, since how long a
/// session lasts is the server's business.
bool advance(ServerContract::ServerApi& server, SharedDocument& ward, qint64 seconds);

} // namespace ServerTestSupport
