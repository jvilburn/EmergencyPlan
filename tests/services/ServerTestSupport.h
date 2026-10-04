#pragma once

#include "ChainTestSupport.h"
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

using ChainTestSupport::TestDevice;

/// Runs call with a reply that stores the result, then spins a QEventLoop until
/// it arrives. Ten seconds without one is ServerError::Unreachable. Waiting on the
/// event loop serves the fake and a real server alike.
template<typename T>
ServerContract::Result<T> wait(const std::function<void(ServerContract::Reply<T>)>& call)
{
    constexpr int TimeoutMilliseconds = 10 * 1000;

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

/// One ward on the server, and what the test knows about it.
struct SharedDocument
{
    QString id;
    DataKey dataKey;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    ServerContract::Session aliceSession;
    ServerContract::Session bobSession;
    ServerContract::Session carolSession;
    DocumentHeader current;
    qint64 version = 0;
};

/// Signs in and remembers which device the session belongs to, so the signed
/// calls below can sign each request with that device's own key.
std::optional<ServerContract::Session> signIn(ServerContract::ServerApi& server,
                                              const TestDevice& device);

/// The device a session from signIn belongs to.
const TestDevice& signerOf(const ServerContract::Session& session);

// --- Every mutating call, signed by the session's own device ---
//
// A test that means to send a bad signature calls the ServerApi method directly.

ServerContract::Result<qint64> signedRegisterDocument(ServerContract::ServerApi& server,
                                                      const ServerContract::Session& session,
                                                      const ServerContract::Upload& upload);

ServerContract::Result<qint64> signedUpload(ServerContract::ServerApi& server,
                                            const ServerContract::Session& session,
                                            const QString& documentId,
                                            qint64 baseVersion,
                                            const ServerContract::Upload& upload);

ServerContract::Result<ServerContract::Lease> signedTakeLease(ServerContract::ServerApi& server,
                                                              const ServerContract::Session& session,
                                                              const QString& documentId);

ServerContract::Result<ServerContract::Done> signedReleaseLease(ServerContract::ServerApi& server,
                                                                const ServerContract::Session& session,
                                                                const QString& documentId);

ServerContract::Result<ServerContract::Done> signedRequestAccess(
    ServerContract::ServerApi& server,
    const ServerContract::Session& session,
    const QString& documentId,
    const ServerContract::AccessRequest& request);

ServerContract::Result<ServerContract::Done> signedClearAccessRequest(
    ServerContract::ServerApi& server,
    const ServerContract::Session& session,
    const QString& documentId,
    const Fingerprint& device);

ServerContract::Result<QString> signedStartPairing(ServerContract::ServerApi& server,
                                                   const ServerContract::Session& session,
                                                   const QString& deviceName,
                                                   const QByteArray& commitment);

ServerContract::Result<ServerContract::Done> signedRespondToPairing(
    ServerContract::ServerApi& server,
    const ServerContract::Session& session,
    const QString& code,
    const DeviceKeys& responder);

ServerContract::Result<ServerContract::Done> signedRevealPairing(
    ServerContract::ServerApi& server,
    const ServerContract::Session& session,
    const QString& code,
    const DeviceKeys& initiator,
    const QByteArray& nonce);

ServerContract::Result<ServerContract::Done> signedHandOver(ServerContract::ServerApi& server,
                                                            const ServerContract::Session& session,
                                                            const QString& code,
                                                            const QByteArray& handover);

DeviceKeys keysOf(const TestDevice& device);

/// A header and an opaque blob it vouches for: the payload hash is the blob's,
/// and the header is signed afresh over it.
ServerContract::Upload sealedUpload(DocumentHeader header, const TestDevice& writer);

ServerContract::AccessRequest requestFor(const TestDevice& device, const QString& pairingCode);

bool signInAll(ServerContract::ServerApi& server, SharedDocument& ward);

/// Moves the server's clock and signs everyone in again, since how long a
/// session lasts is the server's business.
bool advance(ServerContract::ServerApi& server, SharedDocument& ward, qint64 seconds);

/// A ward registered by this Alice, listing only these devices of hers. Each one
/// besides Alice herself must already have paired with her.
std::optional<SharedDocument> registerWardWith(ServerContract::ServerApi& server,
                                               const TestDevice& alice,
                                               const QList<TestDevice>& aliceDevices);

std::optional<SharedDocument> registerWard(ServerContract::ServerApi& server);

/// Alice's ward, with Bob and Carol granted by request. Alice is the only admin.
std::optional<SharedDocument> wardOfThree(ServerContract::ServerApi& server);

/// The next version with the same generation and data key. Written by a device
/// the devices no longer include, it is that device's leave.
DocumentHeader nextHeader(const SharedDocument& ward,
                          const QList<TestDevice>& devices,
                          const TestDevice& writer);

/// A version that drops someone: the generation advances under a new data key.
DocumentHeader removalHeader(const SharedDocument& ward,
                             const QList<TestDevice>& devices,
                             const TestDevice& writer);

/// Uploads on top of the ward's current version, and on success makes the
/// uploaded header the ward's current one.
ServerContract::ServerError publish(ServerContract::ServerApi& server,
                                    SharedDocument& ward,
                                    const ServerContract::Session& session,
                                    const TestDevice& writer,
                                    const DocumentHeader& header);

/// Every step of a pairing, in order: the responder reads the commitment, posts
/// its keys, the initiator reveals, and the responder hands over.
bool completePairing(ServerContract::ServerApi& server,
                     const TestDevice& initiator,
                     const ServerContract::Session& initiatorSession,
                     const TestDevice& responder,
                     const ServerContract::Session& responderSession);

} // namespace ServerTestSupport
