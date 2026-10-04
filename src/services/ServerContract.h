#pragma once

#include "CryptoTypes.h"
#include "DocumentHeader.h"
#include "RosterDevice.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

/// What the Ward Planning server accepts and returns - types only, no transport.
///
/// The server is a blob store with a version check and an allow-list. It holds
/// no key material and never reads a payload; its only cryptography is verifying
/// signatures over plaintext headers and login challenges, and hashing a blob. The
/// allow-list is the current accepted header's recipients, so every membership
/// change takes effect on the server in the same upload that makes it.
///
/// FakeServer (tests) implements this in memory and enforces every rule the real
/// server must, so a client written against it will not meet a stricter server.
namespace ServerContract
{

/// Why a call failed. None only on success.
enum class ServerError
{
    None,
    Unreachable,
    Unauthenticated,    ///< no session, an expired one, a login that did not verify, or an unsigned request
    NotAllowListed,     ///< the device is not in the document's current header
    UnknownDocument,
    VersionConflict,    ///< the upload was not built on the current version
    LeaseHeld,          ///< another device holds the edit lease
    ChainRefused,       ///< the header does not extend the membership chain, or is not a valid first header
    PayloadMismatch,    ///< the header's payload hash does not match the blob uploaded with it
    CounterJump,        ///< the save counter rose by more than DocumentHeader::MaxSaveCounterStep
    DeviceNotInvited,   ///< an added device has neither a pending access request nor a completed pairing
    IdTaken,            ///< a document id already registered
    RateLimited,
    NotFound,
    AlreadyWritten,     ///< a pairing step written twice
    OutOfOrder,         ///< a pairing step written before the one it follows
    Expired,            ///< a pairing past its ten minutes
    InvalidRequest      ///< the request contradicts the caller's own identity or is ill-formed
};

/// A call's outcome: a value, or the reason there is none.
template<typename T>
struct Result
{
    std::optional<T> value;
    ServerError error = ServerError::None;

    bool ok() const { return value.has_value(); }
    static Result success(const T& result) { return Result{result, ServerError::None}; }
    static Result failure(ServerError reason) { return Result{std::nullopt, reason}; }
};

/// How every call answers. Called exactly once, and never before the call that
/// was given it returns, so a caller may start a request from inside its own
/// state change without being re-entered halfway through it.
template<typename T>
using Reply = std::function<void(const Result<T>&)>;

/// The value of a call that returns nothing but success.
struct Done
{
};

/// A short-lived token from a signed login challenge. Holding one proves only that
/// the caller has a device's signing key - device authentication. Member
/// authentication is checked per document, against its current header.
struct Session
{
    QString token;
};

/// GET /documents/{id}/head - member authentication.
struct Head
{
    qint64 version = 0;
    std::optional<Fingerprint> leaseHolder;
};

// Reads carry headers as the server stored them. A reader parses them with
// Reconcile::readHeaders, so a header in a later format reaches the client as
// NeedsNewerVersion rather than being lost in transit.

/// GET /documents/{id}?after={base} - member authentication. One consistent
/// state: the kept headers newer than base, then the current header, and the
/// current blob, so a publish landing mid-download can never pair a header with
/// another version's blob.
///
/// after runs from 0 to the current version. 0 is a device with no base: the
/// whole kept chain. The current version itself is a device whose base is
/// current: the current header alone.
struct Snapshot
{
    qint64 version = 0;
    QList<QJsonObject> headers;     ///< never empty; ends with the header of version
    QByteArray blob;
};

/// GET /documents/{id}/headers?after={base} - device authentication. The kept
/// headers newer than base, then the current header; for a former device, up to
/// and including the header that removed it, so it can prove its removal without
/// knowing the current version, which the server no longer tells it.
struct HeaderChain
{
    QList<QJsonObject> headers;
};

/// POST /documents and PUT /documents/{id}: the plaintext header and the opaque
/// blob. The server stores the header for the chain and never looks inside the
/// blob.
struct Upload
{
    DocumentHeader header;
    QByteArray blob;
};

/// POST /documents/{id}/lease
struct Lease
{
    Fingerprint holder;
    QDateTime expires;
};

/// POST /documents/{id}/requests - the only write open to a non-member, since it
/// is how someone asks to become one. The device must be the caller's own.
struct AccessRequest
{
    MemberId member;
    QString displayName;
    RosterDevice device;
    QString pairingCode;    ///< set for an in-person invite; returned to members as posted
};

/// GET /documents/{id}/requests/mine
enum class RequestStatus
{
    Pending,
    Granted,
    Withdrawn
};

/// GET /pairings/{code} - a pairing belongs to no document.
struct PairingState
{
    QString code;
    QString initiatorDeviceName;
    QByteArray commitment;
    std::optional<DeviceKeys> responder;    ///< written once, by the existing device
    std::optional<DeviceKeys> initiator;    ///< the reveal, written once, by the new device
    QByteArray nonce;                       ///< the reveal's random value
    QByteArray handover;                    ///< the responder's signed handover; opaque here
    QDateTime expires;
};

// --- Request signing ---
//
// A session token proves a device signed in; every mutating call is also signed,
// under SignatureTag::Request, with the device's own key. A token taken from a
// device is then not enough to change anything, and a captured signature cannot be
// replayed against another document, another endpoint, or after its session ends.
//
// The signed message is the endpoint's route, the session token, the document id
// (empty for a pairing, which belongs to no document), and a BLAKE2b of the call's
// canonical body under HashDomain::Request. The bodies below are part of the protocol: the
// companion server rebuilds them from the request it received.

enum class Endpoint
{
    RegisterDocument,       ///< POST /documents
    Upload,                 ///< PUT /documents/{id}
    TakeLease,              ///< POST /documents/{id}/lease
    ReleaseLease,           ///< DELETE /documents/{id}/lease
    RequestAccess,          ///< POST /documents/{id}/requests
    ClearAccessRequest,     ///< DELETE /documents/{id}/requests
    StartPairing,           ///< POST /pairings
    RespondToPairing,       ///< PUT /pairings/{code}/responder
    RevealPairing,          ///< PUT /pairings/{code}/reveal
    HandOver                ///< PUT /pairings/{code}/handover
};

/// A header travels as the bytes its writer signed and the signature itself; the
/// blob is bound through the payload hash inside those bytes.
QByteArray registerBody(const Upload& upload);
QByteArray uploadBody(qint64 baseVersion, const Upload& upload);
QByteArray emptyBody();
QByteArray requestAccessBody(const AccessRequest& request);
QByteArray clearAccessRequestBody(const Fingerprint& device);
QByteArray startPairingBody(const QString& deviceName, const QByteArray& commitment);
QByteArray respondToPairingBody(const QString& code, const DeviceKeys& responder);
QByteArray revealPairingBody(const QString& code, const DeviceKeys& initiator, const QByteArray& nonce);
QByteArray handOverBody(const QString& code, const QByteArray& handover);

QByteArray signRequest(Endpoint endpoint,
                       const Session& session,
                       const QString& documentId,
                       const QByteArray& body,
                       const SigningKey& key);

bool requestVerifies(Endpoint endpoint,
                     const Session& session,
                     const QString& documentId,
                     const QByteArray& body,
                     const QByteArray& signature,
                     const VerifyKey& key);

/// The server, one method per endpoint, each answering through its reply.
///
/// The real server answers over the network and the window must not freeze while
/// it does, so no call returns its result. Every implementation keeps the two
/// rules on Reply: called exactly once, never before the call returns.
///
/// FakeServer implements it in memory. The conformance suite is written against
/// this interface only, so the same tests run against the fake and - through an
/// HTTP-backed implementation - against the real server, which is how two
/// implementations of the same rules are kept in agreement.
class ServerApi
{
public:
    virtual ~ServerApi() = default;

    // --- Authentication ---

    /// Single-use bytes to sign under SignatureTag::Auth.
    virtual void challenge(Reply<QByteArray> reply) = 0;

    virtual void authenticate(const VerifyKey& device,
                              const QByteArray& challenge,
                              const QByteArray& signature,
                              Reply<Session> reply) = 0;

    // --- Documents ---

    /// POST /documents. Device authentication: the document does not exist yet.
    virtual void registerDocument(const Session& session,
                                  const Upload& upload,
                                  const QByteArray& requestSignature,
                                  Reply<qint64> reply) = 0;

    virtual void head(const Session& session, const QString& documentId, Reply<Head> reply) = 0;

    virtual void headers(const Session& session,
                         const QString& documentId,
                         qint64 after,
                         Reply<HeaderChain> reply) = 0;

    virtual void download(const Session& session,
                          const QString& documentId,
                          qint64 after,
                          Reply<Snapshot> reply) = 0;

    /// PUT /documents/{id}, conditional on baseVersion. Answers the new version.
    virtual void upload(const Session& session,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        const QByteArray& requestSignature,
                        Reply<qint64> reply) = 0;

    // --- Lease ---

    /// Takes the lease, or refreshes it when this device already holds it.
    virtual void takeLease(const Session& session,
                           const QString& documentId,
                           const QByteArray& requestSignature,
                           Reply<Lease> reply) = 0;
    virtual void releaseLease(const Session& session,
                              const QString& documentId,
                              const QByteArray& requestSignature,
                              Reply<Done> reply) = 0;

    // --- Access requests ---

    /// Device authentication: the one write open to a non-member.
    virtual void requestAccess(const Session& session,
                               const QString& documentId,
                               const AccessRequest& request,
                               const QByteArray& requestSignature,
                               Reply<Done> reply) = 0;

    virtual void accessRequests(const Session& session,
                                const QString& documentId,
                                Reply<QList<AccessRequest>> reply) = 0;

    virtual void clearAccessRequest(const Session& session,
                                    const QString& documentId,
                                    const Fingerprint& device,
                                    const QByteArray& requestSignature,
                                    Reply<Done> reply) = 0;

    /// Device authentication: the requester's own request only.
    virtual void myRequestStatus(const Session& session,
                                 const QString& documentId,
                                 Reply<RequestStatus> reply) = 0;

    // --- This device ---

    /// Device authentication: the documents whose current header lists this
    /// device, so a reinstall restores the ward list.
    virtual void myDocuments(const Session& session, Reply<QStringList> reply) = 0;

    // --- Pairings ---

    /// Device authentication. Answers the pairing code.
    virtual void startPairing(const Session& session,
                              const QString& deviceName,
                              const QByteArray& commitment,
                              const QByteArray& requestSignature,
                              Reply<QString> reply) = 0;

    virtual void pairing(const Session& session, const QString& code, Reply<PairingState> reply) = 0;

    /// Member authentication on some document; written once.
    virtual void respondToPairing(const Session& session,
                                  const QString& code,
                                  const DeviceKeys& responder,
                                  const QByteArray& requestSignature,
                                  Reply<Done> reply) = 0;

    /// The initiator only, after the responder; written once.
    virtual void revealPairing(const Session& session,
                               const QString& code,
                               const DeviceKeys& initiator,
                               const QByteArray& nonce,
                               const QByteArray& requestSignature,
                               Reply<Done> reply) = 0;

    /// The responder only, after the reveal; written once. Completing it is what
    /// lets the initiator's device be added under the responder's person.
    virtual void handOver(const Session& session,
                          const QString& code,
                          const QByteArray& handover,
                          const QByteArray& requestSignature,
                          Reply<Done> reply) = 0;

    // --- Development only ---

    /// Moves the server's clock forward. The fake always honours it; the real
    /// server only in local development, never in a deployed build. It exists so
    /// the conformance suite can check session, lease, and pairing expiry.
    virtual void advanceClock(qint64 seconds, Reply<Done> reply) = 0;
};

} // namespace ServerContract
