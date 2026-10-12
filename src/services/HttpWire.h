#pragma once

#include "ServerContract.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// The Ward Planning server's wire format: the JSON body of every request the
/// client sends and every answer it reads. Part of the protocol - the companion
/// server reads and writes exactly these shapes.
///
/// Every body is a JSON object. Raw bytes travel as standard base64 with padding;
/// fingerprints and member ids as lowercase hex, as in a header; times as ISO 8601
/// UTC with milliseconds; a header as the JSON object of its file's header line.
/// Every call but challenge, authenticate, and advanceClock carries
/// `Authorization: Bearer <token>`; advanceClock exists only on a development server.
///
/// | Call | Route | Request | Answer |
/// |---|---|---|---|
/// | challenge | `POST /auth/challenge` | - | `{challenge}` |
/// | authenticate | `POST /auth/token` | `{device, challenge, signature}` | `{token}` |
/// | registerPerson | `PUT /me` | `{member, displayName, deviceName, publicKey}` | - |
/// | me | `GET /me` | - | `{person, documents}` |
/// | unlinkDevice | `DELETE /me/devices/{fingerprint}` | - | - |
/// | startLink | `POST /links` | `{publicKey, deviceName}` | `{code}` |
/// | link | `GET /links/{code}` | - | `{deviceName, publicKey, verifyKey, approvedAs?, expires}` |
/// | approveLink | `POST /links/{code}/approve` | `{resealings: [{documentId, baseVersion, header, blob}]}` | - |
/// | registerDocument | `POST /documents` | `{header, blob}` | `{version}` |
/// | head | `GET /documents/{id}/head` | - | `{version, published, leaseHolder?, header, members, history}` |
/// | download | `GET /documents/{id}` | - | `{version, published, header, blob}` |
/// | upload | `PUT /documents/{id}` | `{baseVersion, header, blob}` | `{version}` |
/// | takeLease | `POST /documents/{id}/lease` | - | `{holder, expires}` |
/// | releaseLease | `DELETE /documents/{id}/lease` | - | - |
/// | history | `GET /documents/{id}/history?after={n}` | - | `{entries}` |
/// | requestAccess | `POST /documents/{id}/requests` | - | - |
/// | accessRequests | `GET /documents/{id}/requests` | - | `{requests}` |
/// | declineRequest | `DELETE /documents/{id}/requests/{n}` | - | - |
/// | myRequestStatus | `GET /documents/{id}/requests/mine` | - | `{status}`: `pending`, `granted`, or `declined` |
/// | grant | `POST /documents/{id}/members` | `{request, baseVersion, header, blob}` | - |
/// | removeMember | `DELETE /documents/{id}/members/{member}` | - | - |
/// | setRole | `PUT /documents/{id}/members/{member}/role` | `{role}`: `admin` or `member` | - |
/// | advanceClock | `POST /dev/clock` | `{seconds}` | - |
///
/// The shapes:
///
/// - A person is `{member, displayName, devices}`, each device
///   `{fingerprint, publicKey, verifyKey, name, linkedBy?, linkedAt}`. A device whose
///   fingerprint is not that of its keys is refused.
/// - A member, in `members`, is `{person, role}`.
/// - `published` is `{name, device, when}`: who published the version, and when.
/// - `history` is the newest entry's index, 0 for none; an entry is
///   `{index, kind, actor, subject, when}`, kind one of `added`, `removed`, `left`,
///   `madeAdmin`, `madeMember`. actor and subject are display names, never empty; for
///   `left`, and a `madeMember` that is someone stepping down, actor is the subject.
/// - An access request is `{index, person, when}`, its person listing only the device
///   that asked.
/// - `documents` holds document ids; `version` and `index` are positive integers.
///
/// A request that fails answers with `{error}`, the ServerError's name with a
/// lowercase first letter, and this status:
///
/// | Status | Errors |
/// |---|---|
/// | 400 | `invalidRequest` |
/// | 401 | `unauthenticated` |
/// | 403 | `accessLost`, `notMember`, `notAdmin` |
/// | 404 | `unknownDocument`, `notFound` |
/// | 409 | `versionConflict`, `idTaken`, `lastAdmin` |
/// | 410 | `expired` |
/// | 422 | `notOnAllowList` |
/// | 423 | `leaseHeld` |
/// | 429 | `rateLimited` |
///
/// `accessLost` also carries `how` (`removed`, `left`, or `unlinked`), `byWhom`, and
/// `when`. byWhom is the removing admin's display name for `removed`, the name of the
/// device that did the unlinking for `unlinked`, and absent for `left`.
///
/// The client reads the name. An answer naming none it knows is `invalidRequest`.
/// One with no `{error}` at all - a proxy's page, a wrong address - is Unreachable,
/// except a bare 401 or 429, which a front end may send for the server. A redirect, a
/// 5xx, and no answer are Unreachable.
namespace HttpWire
{

// --- Requests the client writes ---

QJsonObject authenticateRequest(const VerifyKey& device, const QByteArray& challenge, const QByteArray& signature);
QJsonObject registerPersonRequest(const ServerContract::Registration& registration);
QJsonObject startLinkRequest(const PublicKey& publicKey, const QString& deviceName);
QJsonObject registerRequest(const ServerContract::Upload& upload);
QJsonObject uploadRequest(qint64 baseVersion, const ServerContract::Upload& upload);
QJsonObject approveLinkRequest(const QList<ServerContract::Resealing>& resealings);
QJsonObject grantRequest(qint64 index, const ServerContract::Resealing& resealing);
QJsonObject setRoleRequest(ServerContract::Role role);
QJsonObject advanceClockRequest(qint64 seconds);

// --- Answers the client reads; nullopt for any missing or ill-formed field ---

std::optional<QByteArray> readChallenge(const QJsonObject& json);
std::optional<ServerContract::Session> readSession(const QJsonObject& json);
std::optional<ServerContract::Me> readMe(const QJsonObject& json);
std::optional<QString> readLinkCode(const QJsonObject& json);
std::optional<ServerContract::LinkState> readLinkState(const QJsonObject& json);
std::optional<qint64> readVersion(const QJsonObject& json);
std::optional<ServerContract::Head> readHead(const QJsonObject& json);
std::optional<ServerContract::Snapshot> readSnapshot(const QJsonObject& json);
std::optional<ServerContract::Lease> readLease(const QJsonObject& json);
std::optional<QList<ServerContract::HistoryEntry>> readHistory(const QJsonObject& json);
std::optional<QList<ServerContract::AccessRequest>> readAccessRequests(const QJsonObject& json);
std::optional<ServerContract::RequestStatus> readRequestStatus(const QJsonObject& json);

/// Why a request failed: the error it names, and for AccessLost how.
struct Failure
{
    ServerContract::ServerError error = ServerContract::ServerError::Unreachable;
    std::optional<ServerContract::LostAccess> lost;
};

/// The failure a request's answer names; see the table above for an answer that
/// names none. An accessLost answer whose details do not read is invalidRequest.
Failure readFailure(int httpStatus, const QByteArray& body);

} // namespace HttpWire
