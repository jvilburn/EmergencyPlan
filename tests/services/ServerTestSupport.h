#pragma once

#include "ChainTestSupport.h"
#include "ServerContract.h"

#include <QList>
#include <QString>

#include <optional>

/// Driving a ServerApi the way a client does, for the server test suites.
///
/// Everything here goes through ServerApi alone, so it serves the conformance
/// suite against any server. Ids and keys are fresh every time, so runs can share
/// a real server.
namespace ServerTestSupport
{

using ChainTestSupport::TestDevice;

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

std::optional<ServerContract::Session> signIn(ServerContract::ServerApi& server,
                                              const TestDevice& device);

DeviceKeys keysOf(const TestDevice& device);

/// A header and an opaque blob it vouches for: the payload hash is the blob's,
/// and the header is signed afresh over it.
ServerContract::Upload sealedUpload(DocumentHeader header, const TestDevice& writer);

ServerContract::AccessRequest requestFor(const TestDevice& device, const QString& pairingCode);

bool signInAll(ServerContract::ServerApi& server, SharedDocument& ward);

/// Moves the server's clock and signs everyone in again, since how long a
/// session lasts is the server's business.
bool advance(ServerContract::ServerApi& server, SharedDocument& ward, qint64 seconds);

/// A ward registered by this Alice, listing only these devices of hers.
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
