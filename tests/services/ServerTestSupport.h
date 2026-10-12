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
/// a real server. The server knows devices only, by their keys; who a device
/// belongs to lives in the payload, which the server never reads.
namespace ServerTestSupport
{

/// One device: its two keypairs and a name for the test to tell it by.
struct TestDevice
{
    QString name;
    Keypair sealing;
    SigningKeypair signing;

    Fingerprint fingerprint() const;
    DeviceKeys keys() const { return DeviceKeys{sealing.publicKey, signing.verifyKey}; }

    /// This device signing bytes, as a write carries it.
    ServerContract::Signer signer(const QByteArray& bytes) const;
};

MemberId newMember();
QString newDocumentId();
TestDevice newDevice(const QString& name);

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

/// A header sealing a fresh data key to these devices, and an opaque blob. The
/// server never reads the blob, so it need not be a real payload.
ServerContract::Upload uploadFor(const QString& documentId, const QList<TestDevice>& devices, qint64 saveCounter);

// --- Each call, signed by device where it writes ---

ServerContract::Result<qint64> registerAs(ServerContract::ServerApi& server,
                                          const TestDevice& device,
                                          const ServerContract::Upload& upload);
ServerContract::Result<qint64> uploadAs(ServerContract::ServerApi& server,
                                        const TestDevice& device,
                                        const QString& documentId,
                                        qint64 baseVersion,
                                        const ServerContract::Upload& upload);
ServerContract::Result<ServerContract::Lease> takeLeaseAs(ServerContract::ServerApi& server,
                                                          const TestDevice& device,
                                                          const QString& documentId);
ServerContract::Result<ServerContract::Done> releaseLeaseAs(ServerContract::ServerApi& server,
                                                            const TestDevice& device,
                                                            const QString& documentId);
ServerContract::Result<ServerContract::Head> headOf(ServerContract::ServerApi& server, const QString& documentId);
ServerContract::Result<ServerContract::Snapshot> downloadOf(ServerContract::ServerApi& server,
                                                            const QString& documentId);
bool advanceClock(ServerContract::ServerApi& server, qint64 seconds);

/// The fingerprints a head's header seals to, as sorted hex.
QStringList recipientsOf(const ServerContract::Head& head);

/// One ward on the server, and what the test knows about it.
struct SharedDocument
{
    QString id;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    qint64 version = 0;
    qint64 saveCounter = 1;
};

/// Alice's ward, registered sealed to her device alone.
std::optional<SharedDocument> registerWard(ServerContract::ServerApi& server);

/// Alice's ward, its second version sealed by her to Alice, Bob, and Carol.
std::optional<SharedDocument> wardOfThree(ServerContract::ServerApi& server);

/// Uploads a version sealed to these devices on top of the ward's current version,
/// signed by writer; on success the ward's version advances.
ServerContract::ServerError publish(ServerContract::ServerApi& server,
                                    SharedDocument& ward,
                                    const TestDevice& writer,
                                    const QList<TestDevice>& devices);

} // namespace ServerTestSupport
