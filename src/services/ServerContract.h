#pragma once

#include "CryptoTypes.h"
#include "DocumentHeader.h"

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
/// The server holds the access list and the ciphertext. People, their devices,
/// each ward's members and roles, and the access history are its ordinary state,
/// changed by the calls below. It holds no key material and never reads a payload;
/// its only cryptography is checking a login signature and computing device
/// fingerprints.
///
/// Confidentiality does not rest on it. A client seals the data key only to devices
/// it approved itself or that the version it builds on already held, so a server
/// that lies about the access list can deny service but cannot read a ward.
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
    Unauthenticated,    ///< no session, an expired one, or a login that did not verify
    AccessLost,         ///< this device had access and no longer does; Result::lost says how
    NotMember,          ///< the caller's person is not a member of this ward
    NotAdmin,           ///< an admin's call made by a member
    LastAdmin,          ///< would leave a ward with people in it and no admin
    NotOnAllowList,     ///< the uploaded header names a device the allow-list lacks
    UnknownDocument,
    VersionConflict,    ///< the upload was not built on the current version
    LeaseHeld,          ///< another device holds the edit lease
    IdTaken,            ///< a document id already registered
    NotFound,           ///< an unknown link code, request, or member
    Expired,            ///< a link code past its ten minutes
    RateLimited,
    InvalidRequest      ///< ill-formed, or contradicts the caller's own identity
};

enum class Role
{
    Admin,
    Member
};

/// One of a person's devices, as the server holds it.
struct Device
{
    Fingerprint fingerprint;
    DeviceKeys keys;
    QString name;
    std::optional<Fingerprint> linkedBy;    ///< the device that linked it; none for a person's first
    QDateTime linkedAt;

    bool operator==(const Device& other) const;
    bool operator!=(const Device& other) const { return !(*this == other); }
};

/// Someone who uses Ward Planning: one record shared by all their wards.
struct Person
{
    MemberId member;
    QString displayName;
    QList<Device> devices;

    bool operator==(const Person& other) const;
    bool operator!=(const Person& other) const { return !(*this == other); }
};

/// A person in one ward.
struct Member
{
    Person person;
    Role role = Role::Member;

    bool isAdmin() const { return role == Role::Admin; }
    bool operator==(const Member& other) const { return person == other.person && role == other.role; }
    bool operator!=(const Member& other) const { return !(*this == other); }
};

/// Why a device that had access no longer does - the reason the app acts on.
struct LostAccess
{
    enum class How
    {
        Removed,
        Left,
        Unlinked
    };

    How how = How::Removed;
    QString byWhom;     ///< display name at the time; empty for Left
    QDateTime when;

    bool operator==(const LostAccess& other) const;
    bool operator!=(const LostAccess& other) const { return !(*this == other); }
};

/// A call's outcome: a value, or the reason there is none.
template<typename T>
struct Result
{
    std::optional<T> value;
    ServerError error = ServerError::None;
    std::optional<LostAccess> lost;     ///< set exactly when error is AccessLost

    bool ok() const { return value.has_value(); }
    static Result success(const T& result) { return Result{result, ServerError::None, std::nullopt}; }
    static Result failure(ServerError reason) { return Result{std::nullopt, reason, std::nullopt}; }
    static Result lostAccess(const LostAccess& how) { return Result{std::nullopt, ServerError::AccessLost, how}; }
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

/// A bearer token from a signed login challenge, sent with every call.
struct Session
{
    QString token;
};

/// PUT /me, sent by ServerConnection after every sign-in. Registers a new person
/// with this device as its first; for a device already in the person it changes
/// nothing. A device claiming a person it is not in is refused, and an unlinked
/// device's claim is refused as AccessLost.
struct Registration
{
    MemberId member;
    QString displayName;
    QString deviceName;
    PublicKey publicKey;    ///< the verify key is the session's own
};

/// GET /me
struct Me
{
    Person person;
    QStringList documents;  ///< the wards this person is a member of
};

/// Who published a version, and when.
struct Publication
{
    QString name;           ///< display name at the time
    Fingerprint device;
    QDateTime when;
};

// Reads carry the header as the server stored it. A reader parses it with
// Reconcile::validateDownload, so a header in a later format reaches the client as
// NeedsNewerVersion rather than being lost in transit.

/// GET /documents/{id}/head
struct Head
{
    qint64 version = 0;
    Publication published;
    std::optional<Fingerprint> leaseHolder;
    QJsonObject header;
    QList<Member> members;
    qint64 history = 0;     ///< the index of the newest access-history entry; 0 for none
};

/// GET /documents/{id} - one consistent state, so a publish landing mid-download
/// can never pair a header with another version's blob.
struct Snapshot
{
    qint64 version = 0;
    Publication published;
    QJsonObject header;
    QByteArray blob;
};

/// POST /documents and PUT /documents/{id}: the plaintext header and the opaque
/// blob. The server checks the header's devices against the allow-list and never
/// looks inside the blob.
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

/// GET /documents/{id}/history?after={n}
struct HistoryEntry
{
    enum class Kind
    {
        Added,
        Removed,
        Left,
        MadeAdmin,
        MadeMember      ///< actor and subject the same: stepped down
    };

    qint64 index = 0;
    Kind kind = Kind::Added;
    QString actor;      ///< display names at the time
    QString subject;
    QDateTime when;
};

/// GET /documents/{id}/requests
struct AccessRequest
{
    qint64 index = 0;
    Person person;      ///< devices: exactly the one that asked
    QDateTime when;
};

/// GET /documents/{id}/requests/mine
enum class RequestStatus
{
    Pending,
    Granted,
    Declined
};

/// GET /links/{code}. The approving device reads the new device's name and keys;
/// the new device reads whether it has been approved, and as whom.
struct LinkState
{
    QString deviceName;
    DeviceKeys keys;
    std::optional<Person> approvedAs;
    QDateTime expires;
};

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

    // --- Signing in ---

    /// Single-use bytes to sign with DocumentCrypto::signChallenge.
    virtual void challenge(Reply<QByteArray> reply) = 0;

    virtual void authenticate(const VerifyKey& device,
                              const QByteArray& challenge,
                              const QByteArray& signature,
                              Reply<Session> reply) = 0;

    // --- This device's person ---

    virtual void registerPerson(const Session& session, const Registration& registration, Reply<Done> reply) = 0;
    virtual void me(const Session& session, Reply<Me> reply) = 0;

    /// One of the caller's person's own devices, never its last.
    virtual void unlinkDevice(const Session& session, const Fingerprint& device, Reply<Done> reply) = 0;

    // --- Linking ---

    /// A signed-in device with no person yet. Answers the link code.
    virtual void startLink(const Session& session,
                           const PublicKey& publicKey,
                           const QString& deviceName,
                           Reply<QString> reply) = 0;

    virtual void link(const Session& session, const QString& code, Reply<LinkState> reply) = 0;

    /// Adds the link's device to the caller's person.
    virtual void approveLink(const Session& session, const QString& code, Reply<Done> reply) = 0;

    // --- Documents ---

    /// The caller's person becomes the ward's first admin. Answers the version.
    virtual void registerDocument(const Session& session, const Upload& upload, Reply<qint64> reply) = 0;

    virtual void head(const Session& session, const QString& documentId, Reply<Head> reply) = 0;
    virtual void download(const Session& session, const QString& documentId, Reply<Snapshot> reply) = 0;

    /// Conditional on baseVersion. Answers the new version.
    virtual void upload(const Session& session,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        Reply<qint64> reply) = 0;

    // --- Lease ---

    /// Takes the lease, or refreshes it when this device already holds it.
    virtual void takeLease(const Session& session, const QString& documentId, Reply<Lease> reply) = 0;
    virtual void releaseLease(const Session& session, const QString& documentId, Reply<Done> reply) = 0;

    // --- Who has access ---

    /// The entries after index after, oldest first.
    virtual void history(const Session& session,
                         const QString& documentId,
                         qint64 after,
                         Reply<QList<HistoryEntry>> reply) = 0;

    /// The caller's person asks, through the caller's device; the one document
    /// write open to a non-member.
    virtual void requestAccess(const Session& session, const QString& documentId, Reply<Done> reply) = 0;

    virtual void accessRequests(const Session& session,
                                const QString& documentId,
                                Reply<QList<AccessRequest>> reply) = 0;
    virtual void declineRequest(const Session& session, const QString& documentId, qint64 index, Reply<Done> reply) = 0;
    virtual void myRequestStatus(const Session& session, const QString& documentId, Reply<RequestStatus> reply) = 0;

    /// The request's person becomes a member.
    virtual void grant(const Session& session, const QString& documentId, qint64 index, Reply<Done> reply) = 0;

    /// An admin removing a person, or the person leaving.
    virtual void removeMember(const Session& session,
                              const QString& documentId,
                              const MemberId& member,
                              Reply<Done> reply) = 0;

    virtual void setRole(const Session& session,
                         const QString& documentId,
                         const MemberId& member,
                         Role role,
                         Reply<Done> reply) = 0;

    // --- Development only ---

    /// Moves the server's clock forward. The fake always honours it; the real
    /// server only in local development, never in a deployed build. It exists so
    /// the conformance suite can check session, lease, and link-code expiry.
    virtual void advanceClock(qint64 seconds, Reply<Done> reply) = 0;
};

} // namespace ServerContract
