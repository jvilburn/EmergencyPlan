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
/// A request needing a session carries `Authorization: Bearer <token>`, and a signed
/// one `X-Request-Signature: <base64>` over the canonical body ServerContract names.
///
/// | Call | Route | Request | Answer |
/// |---|---|---|---|
/// | challenge | `POST /auth/challenge` | - | `{challenge}` |
/// | authenticate | `POST /auth/session` | `{device, challenge, signature}` | `{token}` |
/// | registerDocument | `POST /documents` | `{header, blob}` | `{version}` |
/// | head | `GET /documents/{id}/head` | - | `{version, leaseHolder?}` |
/// | headers | `GET /documents/{id}/headers?after={n}` | - | `{headers}` |
/// | download | `GET /documents/{id}?after={n}` | - | `{version, headers, blob}` |
/// | upload | `PUT /documents/{id}` | `{baseVersion, header, blob}` | `{version}` |
/// | takeLease | `POST /documents/{id}/lease` | - | `{holder, expires}` |
/// | releaseLease | `DELETE /documents/{id}/lease` | - | - |
/// | requestAccess | `POST /documents/{id}/requests` | an access request, below | - |
/// | accessRequests | `GET /documents/{id}/requests` | - | `{requests: [access request]}` |
/// | clearAccessRequest | `DELETE /documents/{id}/requests?device={fingerprint}` | - | - |
/// | myRequestStatus | `GET /documents/{id}/requests/mine` | - | `{status}`: `pending`, `granted`, or `withdrawn` |
/// | myDocuments | `GET /me/documents` | - | `{documents}` |
/// | startPairing | `POST /pairings` | `{deviceName, commitment}` | `{code}` |
/// | pairing | `GET /pairings/{code}` | - | a pairing state, below |
/// | respondToPairing | `PUT /pairings/{code}/responder` | `{publicKey, verifyKey}` | - |
/// | revealPairing | `PUT /pairings/{code}/reveal` | `{publicKey, verifyKey, nonce}` | - |
/// | handOver | `PUT /pairings/{code}/handover` | `{handover}` | - |
/// | advanceClock | `POST /dev/clock` | `{seconds}` | - |
///
/// In the answers, `version` is a positive integer; `leaseHolder` and `holder` are
/// fingerprints; `expires` is a time; `headers` is never empty and ends with the
/// header of `version`; `documents` holds document ids.
///
/// An access request is `{member, displayName, deviceName, publicKey, verifyKey,
/// pairingCode?}`: the member id in hex, the two names as text, the device's two
/// public keys in base64, and the pairing code of an in-person invite. A request
/// without one leaves `pairingCode` out, and its canonical body
/// (ServerContract::requestAccessBody) signs it as empty text.
///
/// A pairing state is `{code, initiatorDeviceName, commitment, responder?, initiator?,
/// nonce?, handover?, expires}`: `responder` and `initiator` are each
/// `{publicKey, verifyKey}`, present once written; `commitment`, `nonce`, and
/// `handover` are base64, the last two present once written.
///
/// A request that fails answers with `{error}`, the ServerError's name with a
/// lowercase first letter, and this status:
///
/// | Status | Errors |
/// |---|---|
/// | 400 | `invalidRequest` |
/// | 401 | `unauthenticated` |
/// | 403 | `notAllowListed`, `deviceNotInvited` |
/// | 404 | `unknownDocument`, `notFound` |
/// | 409 | `versionConflict`, `idTaken`, `alreadyWritten`, `outOfOrder` |
/// | 410 | `expired` |
/// | 422 | `chainRefused`, `payloadMismatch`, `counterJump` |
/// | 423 | `leaseHeld` |
/// | 429 | `rateLimited` |
///
/// The client reads the name. An answer naming none it knows is `invalidRequest`.
/// One with no `{error}` at all - a proxy's page, a wrong address - is Unreachable,
/// except a bare 401 or 429, which a front end may send for the server. A redirect, a
/// 5xx, and no answer are Unreachable.
namespace HttpWire
{

// --- Requests the client writes ---

QJsonObject authenticateRequest(const VerifyKey& device, const QByteArray& challenge, const QByteArray& signature);
QJsonObject registerRequest(const ServerContract::Upload& upload);
QJsonObject uploadRequest(qint64 baseVersion, const ServerContract::Upload& upload);
QJsonObject accessRequestRequest(const ServerContract::AccessRequest& request);
QJsonObject startPairingRequest(const QString& deviceName, const QByteArray& commitment);
QJsonObject deviceKeysRequest(const DeviceKeys& keys);
QJsonObject revealPairingRequest(const DeviceKeys& initiator, const QByteArray& nonce);
QJsonObject handOverRequest(const QByteArray& handover);
QJsonObject advanceClockRequest(qint64 seconds);

// --- Answers the client reads; nullopt for any missing or ill-formed field ---

std::optional<QByteArray> readChallenge(const QJsonObject& json);
std::optional<ServerContract::Session> readSession(const QJsonObject& json);
std::optional<qint64> readVersion(const QJsonObject& json);
std::optional<ServerContract::Head> readHead(const QJsonObject& json);
std::optional<ServerContract::HeaderChain> readHeaderChain(const QJsonObject& json);
std::optional<ServerContract::Snapshot> readSnapshot(const QJsonObject& json);
std::optional<ServerContract::Lease> readLease(const QJsonObject& json);
std::optional<QList<ServerContract::AccessRequest>> readAccessRequests(const QJsonObject& json);
std::optional<ServerContract::RequestStatus> readRequestStatus(const QJsonObject& json);
std::optional<QStringList> readDocumentIds(const QJsonObject& json);
std::optional<QString> readPairingCode(const QJsonObject& json);
std::optional<ServerContract::PairingState> readPairingState(const QJsonObject& json);

/// The error a failed request names; see the table above for an answer that names
/// none.
ServerContract::ServerError readError(int httpStatus, const QByteArray& body);

} // namespace HttpWire
